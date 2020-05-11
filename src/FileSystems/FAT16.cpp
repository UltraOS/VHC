#include "FAT16.h"
#include "DiskImages/FloppyImage.h"

void FAT16::RootDirectory::store_file(const std::string& name, const std::string& extension, uint16_t cluster, uint32_t size)
{
    auto entry = Entry();

    auto filename_length = name.size();
    auto extension_length = extension.size();

    if (filename_length > max_filename_length)
        filename_length = max_filename_length;
    else if (filename_length < max_filename_length)
        memset(entry.filename + filename_length, ' ', max_filename_length - filename_length);

    if (extension_length > max_file_extension_length)
        extension_length = max_file_extension_length;
    else if (extension_length < max_file_extension_length)
        memset(entry.extension + extension_length, ' ', max_file_extension_length - extension_length);

    memcpy(entry.filename, name.c_str(), filename_length);
    memcpy(entry.extension, extension.c_str(), extension_length);

    constexpr uint8_t system_file_attribs = 0b00000100;

    entry.attributes = system_file_attribs; // a system file
    // TODO: set time
    // TODO: set date
    entry.cluster = cluster;
    entry.size = size;

    m_directory_entries.emplace_back(entry);
}

bool FAT16::RootDirectory::dump_into(DiskImage& image)
{
    static_assert(sizeof(Entry) == entry_size, "Incorrect FAT16 directory entry size");

    for (auto& entry : m_directory_entries)
    {
        image.write(reinterpret_cast<uint8_t*>(&entry), sizeof(Entry));
    }


    if (m_directory_entries.size() != rootdir_entry_count)
    {
        auto free_entries_to_write = rootdir_entry_count - m_directory_entries.size();

        uint8_t fake_entry[entry_size]{};

        while (free_entries_to_write--)
        {
            image.write(fake_entry, entry_size);
        }
    }

    return true;
}

FAT16::FileAllocationTable::FileAllocationTable()
    : m_table(fat_table_size, 0)
{
    m_table[0] = 0xF0; // 3.5 inch floppy 1.4KB
    m_table[1] = 0xFF; // ^v
    m_table[2] = 0xFF; // end of chain indicator
}

uint16_t FAT16::FileAllocationTable::allocate(uint16_t clusters)
{
    uint16_t first_cluster = 0;
    uint16_t prev_cluster = 0;

    if (!has_atleast(clusters))
        return 0;

    for (size_t i = 0; i < clusters; i++)
    {
        uint16_t cluster = next_free();

        if (i == 0)
            first_cluster = cluster;

        if (prev_cluster != 0)
            put_entry(prev_cluster, cluster);

        prev_cluster = cluster;

        put_entry(cluster, 0x1);
    }

    put_entry(prev_cluster, EOC_mark);

    return first_cluster;
}

size_t FAT16::FileAllocationTable::free_clusters() const
{
    size_t free_count = 0;
    uint16_t last_free = 0;

    for (;;)
    {
        last_free = next_free(last_free);

        if (!last_free)
            return free_count;
        else
            free_count++;
    }
}

bool FAT16::FileAllocationTable::has_atleast(uint16_t clusters) const
{
    uint16_t last_free = 0;

    while (clusters--)
    {
        last_free = next_free(last_free);

        if (!last_free)
            return false;
    }

    return true;
}

std::ostream& FAT16::FileAllocationTable::ls(std::ostream& stream) const
{
    stream << "\nFILE ALLOCATION TABLE (FAT12): " << std::endl;

    stream << "ENTRY[0] RESERVED" << std::endl;
    stream << "ENTRY[1] RESERVED" << std::endl;

    uint16_t last_entry = clusters + 1;

    for (uint16_t i = 2; i < clusters; i++)
    {
        auto entry_value = get_entry(i);

        if (!entry_value)
        {
            last_entry = i;
            break;
        }

        stream << "ENTRY[" << i << "] " << entry_value << std::endl;
    }

    if (last_entry < clusters + 1)
        stream << "ENTRY[" << last_entry << " - " << clusters << "] EMITTED (empty)" << std::endl;

    return stream;
}

std::ostream& operator<<(std::ostream& stream, const FAT16::FileAllocationTable& table)
{
    return table.ls(stream);
}

void FAT16::FileAllocationTable::emplace_tables_into(DiskImage& image, size_t count)
{
    while (count--)
        dump_into(image);
}

void FAT16::FileAllocationTable::dump_into(DiskImage& image)
{
    image.write(m_table.data(), fat_table_size);
}

uint16_t FAT16::FileAllocationTable::next_free(uint16_t after_offset) const
{
    if (after_offset < 2) after_offset = 2;

    for (uint16_t i = after_offset; i < clusters; i++)
    {
        if (get_entry(i) == free_cluster)
            return i;
    }

    return 0;
}

void FAT16::FileAllocationTable::put_entry(size_t index, uint16_t val)
{
    if (index == 0 || index == 1)
        throw std::runtime_error("Indicies 0 and 1 are reserved");

    bool is_lower_half = (index % 2) == 0;

    index = (index / 2) * 3;

    if (is_lower_half)
    {
        m_table[index] = val & 0b000011111111;
        m_table[index + 1] = (m_table[index + 1] & 0b11110000) | ((val & 0b111100000000) >> 8);
    }
    else
    {
        m_table[index + 1] = (m_table[index + 1] & 0b00001111) | ((val & 0b000000001111) << 4);
        m_table[index + 2] = (val & 0b111111110000) >> 4;
    }
}

uint16_t FAT16::FileAllocationTable::get_entry(size_t index) const
{
    if (index == 0 || index == 1)
        throw std::runtime_error("Indicies 0 and 1 are reserved");

    bool is_lower_half = (index % 2) == 0;

    index = (index / 2) * 3;

    uint16_t entry = 0;

    if (is_lower_half)
    {
        entry = m_table[index];
        entry = entry | ((m_table[index + 1] & 0b00001111) << 8);
    }
    else
    {
        entry = m_table[index + 1] >> 4;
        entry = entry | (m_table[index + 2] << 4);
    }

    return entry;
}

FAT16::FAT16(const std::string& bootsector_path, const std::vector<std::string>& other_files)
{
    set_bootsector(bootsector_path);

    for (const auto& file : other_files)
        store_file(file);
}

void FAT16::set_bootsector(const std::string& path)
{
    AutoFile bootsector_file(path, "rb");

    m_bootsector.resize(DiskImage::sector_size);

    bootsector_file.read(m_bootsector.data(), DiskImage::sector_size);

    if (m_bootsector[510] != 0x55 && m_bootsector[511] != 0xAA)
        throw std::runtime_error("Incorrect bootsector signature, has to end with 0x55AA");
}

void FAT16::store_file(const std::string& path)
{
    AutoFile file(path, "rb");

    size_t file_size = file.size();

    uint16_t required_clusters = static_cast<uint16_t>(1 + ((file_size - 1) / DiskImage::sector_size));

    uint16_t first_cluster = m_file_allocation_table.allocate(required_clusters);

    if (!first_cluster)
        throw std::runtime_error("Out of space!");

    size_t byte_offset = m_raw_data.size();
    m_raw_data.resize(byte_offset + (required_clusters * DiskImage::sector_size));

    file.read(m_raw_data.data() + byte_offset, file_size);

    auto name_to_extension = split_filename(path);

    m_root_directory.store_file(name_to_extension.first, name_to_extension.second, first_cluster, static_cast<uint32_t>(file_size));
}

void FAT16::dump(const std::string& path)
{
    std::cout << "Creating floppy image file... ";
    FloppyImage image(path);
    std::cout << "OK" << std::endl;

    std::cout << "Writing bootsector... ";
    image.write(m_bootsector.data(), m_bootsector.size());
    std::cout << "OK" << std::endl;

    std::cout << "Writing FAT tables... ";
    m_file_allocation_table.emplace_tables_into(image);
    std::cout << "OK" << std::endl;

    std::cout << "Writing root directory... ";
    m_root_directory.dump_into(image);
    std::cout << "OK" << std::endl;

    std::cout << "Writing data clusters... ";
    image.write(m_raw_data.data(), m_raw_data.size());
    std::cout << "OK" << std::endl;

    std::cout << "Padding the image... ";
    image.finalize();
    std::cout << "OK" << std::endl;
}
