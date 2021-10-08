#include <stdexcept>
#include <string_view>
#include <cstddef>
#include <memory>

#include "DiskImage.h"
#include "VMDKDiskImage.h"

std::shared_ptr<DiskImage> DiskImage::create(std::string_view type, std::string_view out_directory, std::string_view out_name, size_t out_size)
{
    if (type == "vmdk" || type == "VMDK")
        return std::make_shared<VMDKDiskImage>(out_directory, out_name, out_size);

    throw std::runtime_error("Unknown disk image type " + std::string(type));
}

DiskImage::DiskImage(const DiskGeometry& geometry)
    : m_geometry(geometry)
{
}
