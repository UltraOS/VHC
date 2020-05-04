#pragma once
#include <string>

#include "FileSystem.h"

class FAT32 final : public FileSystem
{
private:
    class FileAllocationTable
    {

    };
public:
    FAT32(const std::string& vbr_path, size_t lba_offset, size_t sector_count);

    void write_into(DiskImage& image) override;

    void add_file(const std::string& path) override;
private:
    static constexpr size_t vbr_size = 512;
    uint8_t m_vbr[vbr_size];
};
