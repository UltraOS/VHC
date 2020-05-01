#include "Utility.h"
#include "VMDKDiskImage.h"

VMDKDiskImage::VMDKDiskImage(const std::string& dir_path, const std::string& image_name, size_t size)
    : m_size(size),
      m_bytes_written(0),
      m_disk_file(nullptr),
      m_description_file(nullptr)
{
    if (image_name.find('.') != std::string::npos)
        throw std::runtime_error("Image name cannot contain dots");

    std::string full_image_name = image_name + ".vmdk";
    std::string full_image_description_name = image_name + "-flat.vmdk";

    auto image_file_path = construct_path(dir_path, full_image_name);
    auto image_description_file_path = construct_path(dir_path, full_image_description_name);

    m_disk_file = fopen(image_file_path.c_str(), "wb");
    m_description_file = fopen(image_description_file_path.c_str(), "wb");

    if (!m_disk_file)
        throw std::runtime_error("Couldn't create/open file " + image_file_path);

    if (!m_description_file)
        throw std::runtime_error("Couldn't create/open file " + image_description_file_path);
}

void VMDKDiskImage::write(uint8_t* data, size_t size)
{

}

void VMDKDiskImage::finalize()
{

}

VMDKDiskImage::~VMDKDiskImage()
{

}

void VMDKDiskImage::write_description()
{
    const char* VMDK_header = \
        "# Disk DescriptorFile\n"
        "version = 1\n"
        "encoding = \"UTF-8\"\n"
        "CID = fffffffe\n"
        "parentCID = ffffffff\n"
        "createType = \"monolithicFlat\"\n"
        "# Extent description\n";

    size_t sectors = static_cast<size_t>(ceil(static_cast<float>(m_size) / sector_size));

    // RW + sectors + image_name + 0
    // Disk database (CHS values)
}
