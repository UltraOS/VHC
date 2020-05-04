#include <iostream>

#include "FAT32.h"
#include "Utility.h"

FAT32::FileAllocationTable::FileAllocationTable(size_t length_in_clusters)
    : m_table(length_in_clusters, 0)
{
    m_table[0] = 0xFFFFFFFF;
    *reinterpret_cast<uint8_t*>(&m_table[0]) = hard_disk_media_descriptor;

    m_table[1] = end_of_chain;
}

size_t FAT32::FileAllocationTable::size_in_clusters()
{
    return m_table.size();
}

uint32_t FAT32::FileAllocationTable::allocate(uint32_t cluster_count)
{
    uint32_t first_cluster = 0;
    uint32_t prev_cluster = 0;

    if (!has_atleast(cluster_count))
        return 0;

    for (size_t i = 0; i < cluster_count; i++)
    {
        uint32_t cluster = next_free();

        if (i == 0)
            first_cluster = cluster;

        if (prev_cluster != 0)
            put_entry(prev_cluster, cluster);

        prev_cluster = cluster;

        put_entry(cluster, 0x1);
    }

    put_entry(prev_cluster, end_of_chain);

    return first_cluster;
}

uint32_t FAT32::FileAllocationTable::next_free(uint32_t after_index) const
{
    ensure_legal_cluster(after_index + 1);

    for (uint32_t i = after_index + 1; i < m_table.size(); ++i)
    {
        if (m_table[i] == free_cluster)
            return i;
    }

    return 0;
}

bool FAT32::FileAllocationTable::has_atleast(uint32_t free_clusters) const
{
    uint32_t last_free = 0;

    while (free_clusters--)
    {
        last_free = next_free(last_free);

        if (!last_free)
            return false;
    }

    return true;
}

void FAT32::FileAllocationTable::put_entry(uint32_t cluster, uint32_t value)
{
    ensure_legal_cluster(cluster);

    m_table[cluster] = value;
}

void FAT32::FileAllocationTable::ensure_legal_cluster(uint32_t index) const
{
    if (index < 2)
        throw std::runtime_error("First two entries of the file allocation table are reserved");
    if (index > max_cluster_index)
        throw std::runtime_error("Maximum cluster index is 0x0FFFFFEF");
    if (index > m_table.size() - 1)
        throw std::runtime_error("File allocation table overflow");
}

void FAT32::FileAllocationTable::write_into(DiskImage& image, size_t count)
{
    while (count--)
        image.write(reinterpret_cast<uint8_t*>(m_table.data()), m_table.size() * sizeof(uint32_t));
}

uint32_t FAT32::FileAllocationTable::get_entry(uint32_t index) const
{
    ensure_legal_cluster(index);

    return m_table[index];
}

std::ostream& FAT32::FileAllocationTable::ls(std::ostream& stream) const
{
    stream << "\nFILE ALLOCATION TABLE (FAT32): " << std::endl;

    stream << "ENTRY[0] RESERVED" << std::endl;
    stream << "ENTRY[1] RESERVED" << std::endl;

    uint32_t last_entry = static_cast<uint32_t>(m_table.size() + 1);

    for (uint32_t i = 2; i < m_table.size(); i++)
    {
        auto entry_value = get_entry(i);

        if (!entry_value)
        {
            last_entry = i;
            break;
        }

        stream << "ENTRY[" << i << "] " << entry_value << std::endl;
    }

    if (last_entry < m_table.size() + 1)
        stream << "ENTRY[" << last_entry << " - " << m_table.size() << "] EMITTED (empty)" << std::endl;

    return stream;
}

std::ostream& operator<<(std::ostream& stream, const FAT32::FileAllocationTable& table)
{
    return table.ls(stream);
}

FAT32::Directory::Directory(size_t size_in_bytes)
    : m_size(size_in_bytes)
{
    if (size_in_bytes % entry_size)
        throw std::runtime_error("Directory size has to aligned to entry size");
}

void FAT32::Directory::store_file(const std::string& name, const std::string& extension, uint32_t cluster, uint32_t size)
{
    auto entry = Entry();

    auto filename_length = name.size();
    auto extension_length = extension.size();

    if (filename_length > filename_length_limit)
        filename_length = filename_length_limit;
    else if (filename_length < filename_length_limit)
        memset(entry.filename + filename_length, ' ', filename_length_limit - filename_length);

    if (extension_length > extension_length_limit)
        extension_length = extension_length_limit;
    else if (extension_length < extension_length_limit)
        memset(entry.extension + extension_length, ' ', extension_length_limit - extension_length);

    memcpy(entry.filename, name.c_str(), filename_length);
    memcpy(entry.extension, extension.c_str(), extension_length);

    entry.attributes = 0b00000100; // a system file
    // TODO: set time
    // TODO: set date
    entry.cluster_high = cluster & 0xFFFF0000;
    entry.cluster_low =  cluster & 0x0000FFFF;
    entry.size = size;

    m_entries.emplace_back(entry);
}

size_t FAT32::Directory::max_entires()
{
    return m_size / entry_size;
}

bool FAT32::Directory::write_into(DiskImage& image)
{
    static_assert(sizeof(Entry) == entry_size, "Incorrect Entry size, you might wanna force the alignment");

    for (auto& entry : m_entries)
        image.write(reinterpret_cast<uint8_t*>(&entry), entry_size);

    if (m_entries.size() != max_entires())
    {
        auto free_entries_to_write = max_entires() - m_entries.size();

        uint8_t fake_entry[entry_size]{};

        while (free_entries_to_write--)
            image.write(fake_entry, entry_size);
    }

    return true;
}

FAT32::FAT32(const std::string& vbr_path, size_t lba_offset, size_t sector_count)
    : m_vbr{}, m_sector_count(sector_count),
    m_sectors_per_cluster(pick_sectors_per_cluster()),
    m_fat_table(sector_count / m_sectors_per_cluster),
    m_root_dir(m_sectors_per_cluster * DiskImage::sector_size)
{
    auto vbr_file = fopen(vbr_path.c_str(), "rb");
    READ_EXACTLY(vbr_file, m_vbr, vbr_size);
    fclose(vbr_file);

    validate_vbr();
}

void FAT32::write_into(DiskImage& image)
{
    // set the EBPB in the VBR
    image.write(m_vbr, vbr_size);
    // we skip the FS information sector for now
    m_fat_table.write_into(image);
    m_root_dir.write_into(image);
    image.write(m_data.data(), m_data.size());
}

void FAT32::store_file(const std::string& path)
{
    auto file = fopen(path.c_str(), "rb");

    if (!file)
        throw std::runtime_error("Bad path - " + path);

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    uint32_t required_clusters = 1 + ((file_size - 1) / (DiskImage::sector_size * m_sectors_per_cluster));

    uint32_t first_cluster = m_fat_table.allocate(required_clusters);

    if (!first_cluster)
        throw std::runtime_error("Out of space!");

    size_t byte_offset = m_data.size();
    m_data.resize(byte_offset + (required_clusters * m_sectors_per_cluster * DiskImage::sector_size));

    READ_EXACTLY(file, m_data.data() + byte_offset, file_size);

    auto file_offset = path.find_last_of("/");
    if (file_offset == std::string::npos)
        file_offset = path.find_last_of("\\");
    auto extension_offset = path.find_last_of(".");

    std::string extension = path.substr(extension_offset + 1);
    std::string file_name = path.substr(file_offset + 1, extension_offset - file_offset - 1);

    m_root_dir.store_file(file_name, extension, first_cluster, static_cast<uint32_t>(file_size));
}

void FAT32::validate_vbr()
{
    if (m_vbr[510] != 0x55 && m_vbr[511] != 0xAA)
        throw std::runtime_error("Incorrect VBR signature, has to end with 0x55AA");
}

size_t FAT32::pick_sectors_per_cluster()
{
    size_t size_in_bytes = m_sector_count * DiskImage::sector_size;

    // Got this table from microsoft's website
    // (They probably know their own filesystem better than me)
    // https://support.microsoft.com/en-gb/help/140365/default-cluster-size-for-ntfs-fat-and-exfat

    if (size_in_bytes < 32 * MB)
        throw std::runtime_error("FAT32 cannot be less than 32 megabytes in size");
    else if (size_in_bytes < 64 * MB)
        return 1;
    else if (size_in_bytes < 128 * MB)
        return 2;
    else if (size_in_bytes < 256 * MB)
        return 4;
    else if (size_in_bytes < 8 * GB)
        return 8;
    else if (size_in_bytes < 16 * GB)
        return 16;
    else if (size_in_bytes < 32 * GB)
        return 32;
    else if (size_in_bytes < 2 * TB)
        return 64;
    else
        throw std::runtime_error("FAT32 cannot be greater than 2 terabytes in size");
}
