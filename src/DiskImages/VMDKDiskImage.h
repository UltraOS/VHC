#pragma once

#include "Utility.h"
#include "DiskImage.h"

class VMDKDiskImage final : public DiskImage
{
private:
    const size_t m_final_size;
    size_t m_bytes_written;
    disk_geometry m_geometry;
    FILE* m_disk_file;
public:
    VMDKDiskImage(const std::string& dir_path, const std::string& image_name, size_t size);

    void write(uint8_t* data, size_t size) override;

    const disk_geometry& geometry() const noexcept;

    void finalize() override;

    static disk_geometry calculate_geometry(size_t size_in_bytes);

    ~VMDKDiskImage();
private:
    void write_description(const std::string& image_name, const std::string& path_to_image_description);
};
