#include "FAT32.h"

#include "Utility.h"

FAT32::FAT32(const std::string& vbr_path, size_t lba_offset, size_t sector_count)
    : m_vbr{}
{
    auto vbr_file = fopen(vbr_path.c_str(), "rb");

    READ_EXACTLY(vbr_file, m_vbr, vbr_size);
}

void FAT32::write_into(DiskImage& image)
{

}

void FAT32::add_file(const std::string& path)
{

}
