#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstddef>
#include <memory>

#include "Utilities/Common.h"
#include "FileSystems/FileSystem.h"

namespace FAT {

class FileAllocationTable;
class Directory;

class FAT32 final : public FileSystem
{
public:
    FAT32(DiskImage& image, size_t lba_offset, size_t sector_count, const additional_options_t& options);

    void finalize() override;
    void store(const FSObject&);

    [[nodiscard]] FileAllocationTable& allocation_table();
    [[nodiscard]] size_t sectors_per_cluster() const { return m_sectors_per_cluster; }
    [[nodiscard]] size_t cluster_to_byte_offset(size_t) const;
    [[nodiscard]] bool use_vfat() const { return m_use_vfat; }

    ~FAT32();

private:
    std::pair<uint32_t, uint32_t> calculate_fat_length();
    void validate_vbr();
    void construct_ebpb();

    size_t pick_sectors_per_cluster();

private:
    static constexpr uint32_t max_cluster_index = 0x0FFFFFEF;
    static constexpr uint32_t end_of_chain = 0x0FFFFFFF;
    static constexpr uint8_t hard_disk_media_descriptor = 0xF8;
    static constexpr uint32_t free_cluster = 0x00000000;
    static constexpr size_t vbr_size = 512;

    // Has to be exactly 32, otherwise Windows will not mount it
    static constexpr uint32_t reserved_sector_count = 32;

    uint8_t m_vbr[vbr_size];
    size_t m_byte_offset_to_data { 0 };
    size_t m_sectors_per_cluster { 0 };

    bool m_use_vfat { true };

    std::shared_ptr<FileAllocationTable> m_allocation_table;
    std::shared_ptr<Directory> m_root_dir;
};

}
