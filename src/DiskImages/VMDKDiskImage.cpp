#include "Utility.h"
#include "VMDKDiskImage.h"

VMDKDiskImage::VMDKDiskImage(const std::string& dir_path, const std::string& image_name, size_t size)
    : m_final_size(size),
      m_bytes_written(0),
      m_geometry(calculate_geometry(size)),
      m_disk_file(nullptr)
{
    if (image_name.find('.') != std::string::npos)
        throw std::runtime_error("Image name cannot contain dots");

    std::string full_image_name = image_name + "-flat.vmdk";
    std::string full_image_description_name = image_name + ".vmdk";

    auto image_file_path = construct_path(dir_path, full_image_name);
    auto image_description_file_path = construct_path(dir_path, full_image_description_name);

    m_disk_file = fopen(image_file_path.c_str(), "wb");

    if (!m_disk_file)
        throw std::runtime_error("Couldn't create disk file at " + image_file_path);

    write_description(full_image_name, image_description_file_path);
}

void VMDKDiskImage::write(uint8_t* data, size_t size)
{
    if (m_bytes_written + size > m_final_size)
        throw std::runtime_error("Disk size overflow");

    WRITE_EXACTLY(m_disk_file, data, size);
    m_bytes_written += size;
}

const disk_geometry& VMDKDiskImage::geometry() const noexcept
{
    return m_geometry;
}

void VMDKDiskImage::finalize()
{
    auto padding_size = m_final_size - m_bytes_written;

    if (!padding_size)
        return;

    char padding[sector_size]{};

    size_t size_to_write = 0;

    while (padding_size)
    {
        size_to_write = padding_size > sector_size ? sector_size : padding_size;
        WRITE_EXACTLY(m_disk_file, padding, size_to_write);
        padding_size -= size_to_write;
    }
}

VMDKDiskImage::~VMDKDiskImage()
{
    if (m_disk_file)
        fclose(m_disk_file);
}

void VMDKDiskImage::write_description(const std::string& image_name, const std::string& path_to_image_description)
{
    auto description_file = fopen(path_to_image_description.c_str(), "wb");

    if (!description_file)
        throw std::runtime_error("Couldn't create image description file at " + path_to_image_description);

    static std::string VMDK_header =
        "# Disk DescriptorFile\n"
        "version=1\n"
        "encoding=\"UTF-8\"\n"
        "CID=fffffffe\n"
        "parentCID=ffffffff\n"
        "createType=\"monolithicFlat\"\n\n"
        "# Extent description\n";

    std::string extent_description = "RW ";
    extent_description += std::to_string(geometry().total_sector_count) + " FLAT \"" + image_name + "\" 0\n\n";

    static std::string disk_database =
        "# The Disk Data Base\n"
        "#DDB\n\n";

    static std::string ddb_vhv = "ddb.virtualHWVersion=\"16\"\n";
    static std::string ddb_at  = "ddb.adapterType=\"ide\"\n";
    static std::string ddb_tv  = "ddb.toolsVersion=\"0\"\n";
    std::string ddb_gc  = "ddb.geometry.cylinders=\"" + std::to_string(geometry().cylinders) + "\"\n";
    std::string ddb_gh  = "ddb.geometry.heads=\"" + std::to_string(geometry().heads) + "\"\n";
    std::string ddb_gs  = "ddb.geometry.sectors=\"" + std::to_string(geometry().sectors) + "\"\n";

    WRITE_EXACTLY(description_file, VMDK_header.c_str(), VMDK_header.size());
    WRITE_EXACTLY(description_file, extent_description.c_str(), extent_description.size());
    WRITE_EXACTLY(description_file, disk_database.c_str(), disk_database.size());
    WRITE_EXACTLY(description_file, ddb_vhv.c_str(), ddb_vhv.size());
    WRITE_EXACTLY(description_file, ddb_gc.c_str(), ddb_gc.size());
    WRITE_EXACTLY(description_file, ddb_gh.c_str(), ddb_gh.size());
    WRITE_EXACTLY(description_file, ddb_gs.c_str(), ddb_gs.size());
    WRITE_EXACTLY(description_file, ddb_at.c_str(), ddb_at.size());
    WRITE_EXACTLY(description_file, ddb_tv.c_str(), ddb_tv.size());

    fclose(description_file);
}

disk_geometry VMDKDiskImage::calculate_geometry(size_t size_in_bytes)
{
    constexpr size_t vmdk_byte_limit = 8455200768;
    constexpr size_t vmdk_cylinder_count_limit = 16383;
    constexpr size_t vmdk_ide_heads = 16;
    constexpr size_t vmdk_ide_sectors = 63;
    constexpr size_t vmdk_ide_combined = vmdk_ide_heads * vmdk_ide_sectors;

    if (size_in_bytes % 512)
        throw std::runtime_error("Disk size must be aligned to 512 bytes");

    disk_geometry dg;
    dg.total_sector_count = size_in_bytes / 512;
    dg.heads = vmdk_ide_heads;
    dg.sectors = vmdk_ide_sectors;
    dg.cylinders = dg.total_sector_count / vmdk_ide_combined;

    if (dg.cylinders > vmdk_cylinder_count_limit)
        dg.cylinders = vmdk_cylinder_count_limit;

    return dg;
}
