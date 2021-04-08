#include <iostream>
#include <ctime>

#include "FAT32.h"
#include "Utility.h"

FAT32::FileAllocationTable::FileAllocationTable(std::pair<uint32_t, uint32_t> length_in_entries_and_actual_cluster_capacity)
    : m_table(length_in_entries_and_actual_cluster_capacity.first, 0)
    , m_actual_capacity(length_in_entries_and_actual_cluster_capacity.second)
{
    m_table[0] = 0xFFFFFFFF;
    *reinterpret_cast<uint8_t*>(&m_table[0]) = hard_disk_media_descriptor;

    m_table[1] = end_of_chain;
}

size_t FAT32::FileAllocationTable::size_in_clusters() const
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
    m_last_allocated = prev_cluster;

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
    uint32_t last_free = 1;

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

uint32_t FAT32::FileAllocationTable::size_in_sectors() const
{
    return static_cast<uint32_t>(1 + ((size_in_clusters() * sizeof(uint32_t) - 1) / (DiskImage::sector_size)));
}

void FAT32::FileAllocationTable::write_into(DiskImage& image, size_t count)
{
    // maybe make a separate function for that
    size_t bytes_to_pad = (size_in_sectors() * DiskImage::sector_size) - (m_table.size() * sizeof(uint32_t));

    uint8_t* padding = new uint8_t[bytes_to_pad]{};

    while (count--)
    {
        image.write(reinterpret_cast<uint8_t*>(m_table.data()), m_table.size() * sizeof(uint32_t));

        if (bytes_to_pad)
            image.write(padding, bytes_to_pad);
    }

    delete[] padding;
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
        stream << "ENTRY[" << last_entry << " - " << m_table.size() << "] OMITTED (empty)" << std::endl;

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
        throw std::runtime_error("Directory size has to be aligned to entry size");
}

void FAT32::Directory::store_file(const std::string& name, const std::string& extension, uint32_t cluster, uint32_t size)
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

    auto now = std::time(nullptr);
    auto time = *std::gmtime(&now);

    entry.created_ms = 0;

    uint16_t sec = time.tm_sec / 2;
    uint16_t min = time.tm_min;
    uint16_t hr = time.tm_hour;
    uint16_t yr = time.tm_year - 80;
    uint16_t mt = time.tm_mon + 1;
    uint16_t d = time.tm_mday;

    entry.created_time = (hr << 11) | (min << 5) | sec;
    entry.created_date = (yr << 9) | (mt << 5) | d;
    entry.last_accessed_date = entry.created_date;
    entry.last_modified_date = entry.created_date;
    entry.last_modified_time = entry.created_time;

    entry.cluster_high = (cluster & 0xFFFF0000) >> 16;
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
    static_assert(sizeof(Entry) == entry_size, "Incorrect Entry size, you might wanna force the alignment of 1 manually");

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

FAT32::FAT32(const std::string& vbr_path, size_t lba_offset, size_t sector_count, const disk_geometry& geometry)
    : m_lba_offset(lba_offset), m_vbr{}, m_sector_count(sector_count),
    m_sectors_per_cluster(pick_sectors_per_cluster()),
    m_fat_table(calculate_fat_length()),
    m_root_dir(m_sectors_per_cluster * DiskImage::sector_size),
    m_geometry(geometry)
{
    AutoFile file(vbr_path, "rb");
    file.read(m_vbr, vbr_size);
    validate_vbr();

    // preallocate one cluster for the root directory
    m_fat_table.allocate(1);
}

std::pair<uint32_t, uint32_t> FAT32::calculate_fat_length()
{
    auto total_free_sectors = static_cast<uint32_t>(m_sector_count) - reserved_sector_count;

    auto bytes_per_fat = (total_free_sectors / static_cast<uint32_t>(m_sectors_per_cluster)) * 4;
    bytes_per_fat += 4 * 2; // first two clusters are reserved

    auto sectors_per_fat = 1 + ((bytes_per_fat - 1) / static_cast<uint32_t>(DiskImage::sector_size));

    // round up to 4K boundary
    static constexpr uint32_t sectors_per_page = 4096 / DiskImage::sector_size;
    auto rem = sectors_per_fat % sectors_per_page;
    if (rem)
        sectors_per_fat += sectors_per_page - rem;

    total_free_sectors -= sectors_per_fat * 2;

    return { sectors_per_fat * static_cast<uint32_t>(DiskImage::sector_size) / 4,
             total_free_sectors / static_cast<uint32_t>(m_sectors_per_cluster) + 2 };
}

void FAT32::construct_ebpb()
{
#ifdef _MSVC_LANG
    #pragma pack(push, 1)
    #define FORCE_NO_ALIGNMENT
#elif defined(__GNUC__)
    #define FORCE_NO_ALIGNMENT __attribute__((packed))
#else
    #error "Please add your compiler's way of forcing no alignment here"
#endif
    struct EBPB
    {
        // BPB
        uint16_t bytes_per_sector;
        uint8_t  sectors_per_cluster;
        uint16_t reserved_sectors;
        uint8_t  fat_count;
        uint16_t max_root_dir_entries;
        uint16_t unused_1; // total logical sectors for FAT12/16
        uint8_t  media_descriptor;
        uint16_t unused_2; // logical sectors per file allocation table for FAT12/16
        uint16_t sectors_per_track;
        uint16_t heads;
        uint32_t hidden_sectors;
        uint32_t total_logical_sectors;

        // EBPB
        uint32_t sectors_per_fat;
        uint16_t ext_flags;
        uint16_t version;
        uint32_t root_dir_cluster;
        uint16_t fs_information_sector;
        uint16_t backup_boot_sectors;
        uint8_t  reserved[12];
        uint8_t  drive_number;
        uint8_t  unused_3;
        uint8_t  signature;
        uint32_t volume_id;
        char     volume_label[11];
        char     filesystem_type[8];
    } FORCE_NO_ALIGNMENT ebpb;
#ifdef _MSVC_LANG
    #pragma pack(pop)
#endif
#undef FORCE_NO_ALIGNMENT
    constexpr size_t expected_ebpb_size = 79;

    static_assert(sizeof(EBPB) == expected_ebpb_size, "Incorrect EBPB size, force no alignment manually for your compiler");

    constexpr size_t ebpb_offset = 11;

    memcpy(&ebpb, m_vbr + ebpb_offset, expected_ebpb_size);

    ebpb.bytes_per_sector = static_cast<uint16_t>(DiskImage::sector_size);
    ebpb.sectors_per_cluster = static_cast<uint8_t>(m_sectors_per_cluster);
    ebpb.reserved_sectors = reserved_sector_count;

    ebpb.fat_count = 2;

    // We're FAT32, we can have as many as we want
    ebpb.max_root_dir_entries = 0;

    // This is too small for us
    ebpb.unused_1 = 0;
    ebpb.unused_2 = 0;

    ebpb.media_descriptor = hard_disk_media_descriptor;

    ebpb.sectors_per_track = 0;
    ebpb.heads = 0;

    ebpb.hidden_sectors = static_cast<uint32_t>(m_lba_offset);
    ebpb.total_logical_sectors = static_cast<uint32_t>(m_sector_count);

    // has to be 0, otherwise Windows won't mount it
    ebpb.ext_flags = 0;

    // has to be 0, same as above
    ebpb.version = 0x0000;

    ebpb.sectors_per_fat = m_fat_table.size_in_sectors();

    ebpb.fs_information_sector = 1;

    ebpb.root_dir_cluster = 2;

    auto generate_volume_id = []() -> uint32_t
    {
        auto now = std::time(nullptr);
        auto time = *std::gmtime(&now);

        uint16_t dx_1 = ((time.tm_mon + 1) << 8) | time.tm_mday;
        uint16_t dx_2 = time.tm_sec << 8;
        uint16_t upper_word = dx_1 + dx_2;

        uint16_t cx_1 = time.tm_year + 80;
        uint16_t cx_2 = (time.tm_hour << 8) | time.tm_min;
        uint16_t lower_word = cx_1 + cx_2;

        return (upper_word << 16) | lower_word;
    };

    ebpb.volume_id = generate_volume_id();

    // we don't have one
    ebpb.backup_boot_sectors = 0x0000;

    memset(ebpb.reserved, 0, sizeof(ebpb.reserved) / sizeof(uint8_t));

    // fixed disk 1
    ebpb.drive_number = 0x80;

    ebpb.unused_3 = 0;

    // extended boot signature
    ebpb.signature = 0x29;

    size_t filesystem_description_size = sizeof(ebpb.filesystem_type) / sizeof(char);
    char expected_filesystem[] = "FAT32   ";
    if (memcmp(ebpb.filesystem_type, expected_filesystem, filesystem_description_size))
        throw std::runtime_error("Unexpected filesystem type in the EBPB - " + std::string(ebpb.filesystem_type, filesystem_description_size));

    memcpy(m_vbr + ebpb_offset, &ebpb, expected_ebpb_size);
}

void FAT32::write_into(DiskImage& image)
{
    construct_ebpb();

    // set the EBPB in the VBR
    image.write(m_vbr, vbr_size);

    struct FSINFO {
        char signature_1[4];
        uint8_t reserved_1[480];
        char signature_2[4];
        uint32_t free_cluster_count;
        uint32_t last_allocated_cluster;
        uint8_t reserved_2[12];
        char signature_3[4];
    };

    static constexpr size_t fsinfo_size = 512;
    static_assert(sizeof(FSINFO) == fsinfo_size, "incorrect size of FSINFO");

    static constexpr auto* fsinfo_signature_1 = "RRaA";
    static constexpr auto* fsinfo_signature_2 = "rrAa";
    static constexpr uint8_t fsinfo_signature_3[] = { 0x00, 0x00, 0x55, 0xAA };

    FSINFO fsinfo {};
    memcpy(fsinfo.signature_1, fsinfo_signature_1, 4);
    memcpy(fsinfo.signature_2, fsinfo_signature_2, 4);
    memcpy(fsinfo.signature_3, fsinfo_signature_3, 4);

    fsinfo.free_cluster_count = m_fat_table.free_cluster_count();
    fsinfo.last_allocated_cluster = m_fat_table.last_allocated();

    image.write(reinterpret_cast<uint8_t*>(&fsinfo), fsinfo_size);

    uint8_t reserved_sector[DiskImage::sector_size] {};

    // Write the remainder of reserved sectors, should be 2 less because of VBR and FSINFO
    for (size_t bytes = 0; bytes < (reserved_sector_count - 2) * DiskImage::sector_size; bytes += DiskImage::sector_size)
        image.write(reserved_sector, DiskImage::sector_size);

    m_fat_table.write_into(image);

    m_root_dir.write_into(image);

    image.write(m_data.data(), m_data.size());
}

void FAT32::store_file(const std::string& path)
{
    AutoFile file(path, "rb");

    size_t file_size = file.size();

    size_t required_clusters = 1 + ((file_size - 1) / (DiskImage::sector_size * m_sectors_per_cluster));

    uint32_t first_cluster = m_fat_table.allocate(static_cast<uint32_t>(required_clusters));

    if (!first_cluster)
        throw std::runtime_error("Out of space!");

    size_t byte_offset = m_data.size();
    m_data.resize(byte_offset + (required_clusters * m_sectors_per_cluster * DiskImage::sector_size));

    file.read(m_data.data() + byte_offset, file_size);

    auto upper_path = path;
    for (auto& c : upper_path)
        c = toupper(c);

    auto name_to_extension = split_filename(upper_path);

    m_root_dir.store_file(name_to_extension.first, name_to_extension.second, first_cluster, static_cast<uint32_t>(file_size));
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
