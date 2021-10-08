#include <filesystem>

#include "Utilities/Common.h"
#include "VMDKDiskImage.h"

VMDKDiskImage::VMDKDiskImage(std::string_view dir_path, std::string_view image_name, size_t size)
    : DiskImage(calculate_geometry(size))
    , m_final_size(size)
    , m_disk_file()
{
    if (image_name.find('.') != std::string::npos)
        throw std::runtime_error("Image name cannot contain dots");

    std::string full_image_name = std::string(image_name) + "-flat.vmdk";
    std::string full_image_description_name = std::string(image_name) + ".vmdk";

    auto image_file_path = std::filesystem::path(dir_path) / full_image_name;
    auto image_description_file_path = std::filesystem::path(dir_path) / full_image_description_name;

    m_disk_file.open(image_file_path.string(), static_cast<AutoFile::Mode>(AutoFile::Mode::WRITE | AutoFile::Mode::TRUNCATE));

    write_description(full_image_name, image_description_file_path.string());
}

void VMDKDiskImage::write_at(const void* data, size_t size, size_t offset)
{
    if (offset + size > m_final_size)
        throw std::runtime_error("disk size overflow");

    auto previous_offset = m_disk_file.set_offset(offset);

    m_disk_file.write(reinterpret_cast<const uint8_t*>(data), size);
    m_disk_file.set_offset(previous_offset);
}

void VMDKDiskImage::write(const void* data, size_t size)
{
    if (m_disk_file.offset() + size > m_final_size)
        throw std::runtime_error("disk size overflow");

    m_disk_file.write(reinterpret_cast<const uint8_t*>(data), size);
}

void VMDKDiskImage::set_offset(size_t offset)
{
    if (offset >= m_final_size)
        throw std::runtime_error("offset past end of image");

    m_disk_file.set_offset(offset);
}

void VMDKDiskImage::skip(size_t bytes)
{
    if (m_disk_file.skip(bytes) >= m_final_size)
        throw std::runtime_error("skipped past the end of image");
}

void VMDKDiskImage::finalize()
{
    m_disk_file.set_size(m_final_size);
}

void VMDKDiskImage::write_description(std::string_view image_name, std::string_view path_to_image_description)
{
    AutoFile description_file(path_to_image_description, static_cast<AutoFile::Mode>(AutoFile::Mode::WRITE | AutoFile::Mode::TRUNCATE));

    static std::string VMDK_header =
        "# Disk DescriptorFile\n"
        "version=1\n"
        "encoding=\"UTF-8\"\n"
        "CID=fffffffe\n"
        "parentCID=ffffffff\n"
        "createType=\"monolithicFlat\"\n\n"
        "# Extent description\n";

    std::string extent_description = "RW ";
    extent_description += std::to_string(geometry().total_sector_count);
    extent_description += " FLAT \"";
    extent_description += image_name;
    extent_description += "\" 0\n\n";

    static std::string disk_database =
        "# The Disk Data Base\n"
        "#DDB\n\n";

    static std::string ddb_vhv = "ddb.virtualHWVersion=\"16\"\n";
    static std::string ddb_at = "ddb.adapterType=\"ide\"\n";
    static std::string ddb_tv = "ddb.toolsVersion=\"0\"\n";
    std::string ddb_gc = "ddb.geometry.cylinders=\"" + std::to_string(geometry().cylinders) + "\"\n";
    std::string ddb_gh = "ddb.geometry.heads=\"" + std::to_string(geometry().heads) + "\"\n";
    std::string ddb_gs = "ddb.geometry.sectors=\"" + std::to_string(geometry().sectors) + "\"\n";

    description_file.write(VMDK_header);
    description_file.write(extent_description);
    description_file.write(disk_database);
    description_file.write(ddb_vhv);
    description_file.write(ddb_gc);
    description_file.write(ddb_gh);
    description_file.write(ddb_gs);
    description_file.write(ddb_at);
    description_file.write(ddb_tv);
}

DiskGeometry VMDKDiskImage::calculate_geometry(size_t size_in_bytes)
{
    constexpr size_t vmdk_byte_limit = 8455200768;
    constexpr size_t vmdk_cylinder_count_limit = 16383;
    constexpr size_t vmdk_ide_heads = 16;
    constexpr size_t vmdk_ide_sectors = 63;
    constexpr size_t vmdk_ide_combined = vmdk_ide_heads * vmdk_ide_sectors;
    constexpr size_t vmdk_sector_size = 512;

    if (size_in_bytes % vmdk_sector_size)
        throw std::runtime_error("Disk size must be aligned to sector size");

    DiskGeometry dg;
    dg.total_sector_count = size_in_bytes / 512;
    dg.heads = vmdk_ide_heads;
    dg.sectors = vmdk_ide_sectors;
    dg.cylinders = dg.total_sector_count / vmdk_ide_combined;

    if (dg.cylinders > vmdk_cylinder_count_limit)
        dg.cylinders = vmdk_cylinder_count_limit;

    return dg;
}

VMDKDiskImage::~VMDKDiskImage()
{
    finalize();
}
