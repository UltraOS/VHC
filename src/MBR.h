#pragma once
#include <string>
#include <vector>

#include "Utility.h"
#include "DiskImages/DiskImage.h"

class MBR
{
public:
    static constexpr size_t mbr_size = 512;

    class Partition
    {
    public:
        enum class type : uint8_t
        {
            FREE      = 0,
            FAT32_CHS = 0x0B,
            FAT32_LBA = 0x0C
        };

        enum class status : uint8_t
        {
            INACTIVE = 0x0,
            INVALID  = 0x7F,
            BOOTABLE = 0x80
        };
    private:
        Partition::status m_status;
        Partition::type   m_type;
        uint32_t m_sector_count;
    public:
        Partition(size_t sector_count, status s = status::BOOTABLE, type t = type::FAT32_LBA);

        uint32_t sector_count() const noexcept;

        void serialize(uint8_t* into, const disk_geometry& geometry, size_t lba_offset) const;
    };

private:
    static constexpr size_t partition_entry_size = 16;
    static constexpr size_t partition_count = 4;
    static constexpr size_t partition_base = 446;
    static constexpr size_t mpt_size = partition_count * partition_entry_size;

    uint8_t m_mbr[mbr_size];
    size_t m_active_partition;
    size_t m_active_lba_offset;
    size_t m_initial_lba_offset;
    disk_geometry m_disk_geometry;
public:
    MBR(const std::string& path, const disk_geometry& geometry, size_t offset_of_first_partition);

    void write_into(DiskImage& image);

    size_t add_partition(const Partition& partition);
private:
    void validate_mbr();
};
