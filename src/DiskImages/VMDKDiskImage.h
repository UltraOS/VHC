#pragma once

#include "Utilities/Common.h"
#include "DiskImage.h"

class VMDKDiskImage final : public DiskImage
{
public:
    VMDKDiskImage(std::string_view dir_path, std::string_view image_name, size_t size);

    void write_at(const void* data, size_t size, size_t offset) override;
    void write(const void* data, size_t size) override;
    void set_offset(size_t) override;
    void skip(size_t) override;

    void finalize() override;

    static DiskGeometry calculate_geometry(size_t size_in_bytes);

    ~VMDKDiskImage();

private:
    void write_description(std::string_view image_name, std::string_view path_to_image_description);

private:
    size_t m_final_size { 0 };
    AutoFile m_disk_file;
};
