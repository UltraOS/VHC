#pragma once

#include "DiskImage.h"

class VMDKDiskImage final : public DiskImage
{
private:
    size_t m_size;
    size_t m_bytes_written;
    FILE* m_disk_file;
    FILE* m_description_file;
public:
    VMDKDiskImage(const std::string& dir_path, const std::string& image_name, size_t size);

    void write(uint8_t* data, size_t size) override;

    void finalize() override;

    ~VMDKDiskImage();
private:
    void write_description();
};
