#include "FAT32.h"
#include "Utilities/Common.h"

#include "Utilities.h"
#include "FileAllocationTable.h"
#include "Directory.h"

namespace FAT {

FAT32::FAT32(DiskImage& image, size_t lba_offset, size_t sector_count, const additional_options_t& options)
    : FileSystem(image, lba_offset, sector_count)
    , m_sectors_per_cluster(pick_sectors_per_cluster())
{
    auto fat_length = calculate_fat_length();
    m_allocation_table = std::make_shared<FileAllocationTable>(*this, fat_length.first, fat_length.second);

    m_byte_offset_to_data = lba_offset * DiskImage::sector_size;
    m_byte_offset_to_data += reserved_sector_count * DiskImage::sector_size;
    m_byte_offset_to_data += m_allocation_table->size_in_sectors() * DiskImage::sector_size * 2;

    m_root_dir = std::make_shared<Directory>(*this);

    if (options.count("vbr")) {
        AutoFile file(options.at("vbr"), AutoFile::Mode::READ);
        file.read(m_vbr, vbr_size);
        validate_vbr();
    } else {
        // TODO: placeholder vbr
        throw std::runtime_error("FAT32 requires a valid vbr=path option");
    }

    if (options.count("vfat"))
        m_use_vfat = interpret_boolean(options.at("vfat"));
}

FileAllocationTable& FAT32::allocation_table()
{
    return *m_allocation_table;
}

size_t FAT32::cluster_to_byte_offset(size_t cluster) const
{
    return m_byte_offset_to_data + ((cluster - 2) * (m_sectors_per_cluster * DiskImage::sector_size));
}

std::pair<uint32_t, uint32_t> FAT32::calculate_fat_length()
{
    auto total_free_sectors = static_cast<uint32_t>(sector_count()) - reserved_sector_count;

    auto bytes_per_fat = (total_free_sectors / static_cast<uint32_t>(m_sectors_per_cluster)) * 4;
    bytes_per_fat += 4 * 2; // first two clusters are reserved

    auto sectors_per_fat = 1 + ((bytes_per_fat - 1) / static_cast<uint32_t>(DiskImage::sector_size));

    // round up to 4K boundary
    static constexpr uint32_t sectors_per_page = 4096 / DiskImage::sector_size;
    auto rem = sectors_per_fat % sectors_per_page;
    if (rem)
        sectors_per_fat += sectors_per_page - rem;

    total_free_sectors -= sectors_per_fat * 2;

    return { total_free_sectors / static_cast<uint32_t>(m_sectors_per_cluster),
             sectors_per_fat * static_cast<uint32_t>(DiskImage::sector_size) / 4 };
}

void FAT32::construct_ebpb()
{
    EBPB ebpb;

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
    ebpb.sectors_per_fat_legacy = 0;
    ebpb.total_logical_sectors_legacy = 0;

    ebpb.media_descriptor = hard_disk_media_descriptor;

    ebpb.sectors_per_track = 0;
    ebpb.heads = 0;

    ebpb.hidden_sectors = static_cast<uint32_t>(lba_offset());
    ebpb.total_logical_sectors = static_cast<uint32_t>(sector_count());

    // has to be 0, otherwise Windows won't mount it
    ebpb.ext_flags = 0;

    // has to be 0, same as above
    ebpb.version = 0x0000;

    ebpb.sectors_per_fat = m_allocation_table->size_in_sectors();

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
        throw std::runtime_error("unexpected filesystem type in the EBPB - " + std::string(ebpb.filesystem_type, filesystem_description_size));

    memcpy(m_vbr + ebpb_offset, &ebpb, expected_ebpb_size);
}

void FAT32::finalize()
{
    construct_ebpb();

    auto& image = FileSystem::image();

    image.set_offset(lba_offset() * DiskImage::sector_size);

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

    fsinfo.free_cluster_count = m_allocation_table->free_cluster_count();
    fsinfo.last_allocated_cluster = m_allocation_table->last_allocated();

    image.write(reinterpret_cast<uint8_t*>(&fsinfo), fsinfo_size);

    image.skip((reserved_sector_count - 2) * DiskImage::sector_size);
    m_allocation_table->write_into(image);
}

void FAT32::store(const FSObject& obj)
{
    if (obj.type == FSObject::INVALID)
        throw std::runtime_error("invalid type of fs object");

    std::filesystem::path path(obj.path);
    auto filename = path.filename().string();

    auto* directory = m_root_dir.get();

    for (auto& component : path.parent_path()) {
        if (component == "/" || component == "\\")
            continue;

        auto as_string = component.string();

        directory = &directory->subdirectory(as_string);
    }

    if (obj.type == FSObject::DIRECTORY)
        directory->store_directory(filename);
    else
        directory->store_file(filename, obj.data);
}

void FAT32::validate_vbr()
{
    if (m_vbr[510] != 0x55 && m_vbr[511] != 0xAA)
        throw std::runtime_error("Incorrect VBR signature, has to end with 0x55AA");
}

size_t FAT32::pick_sectors_per_cluster()
{
    size_t size_in_bytes = sector_count() * DiskImage::sector_size;

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

FAT32::~FAT32()
{
    finalize();
}

}
