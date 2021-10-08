#pragma once
#include <string>
#include <vector>

#include "Utilities/Common.h"
#include "DiskImages/DiskImage.h"

class MBR
{
public:

    MBR(std::string_view path, const DiskGeometry& geometry, size_t offset_of_first_partition);

    void write_into(DiskImage& image);


    class Partition
    {
    public:
        enum class Type : uint8_t
        {
            FREE      = 0,
            FAT32_CHS = 0x0B,
            FAT32_LBA = 0x0C
        };

        enum class Status : uint8_t
        {
            INACTIVE = 0x0,
            INVALID  = 0x7F,
            BOOTABLE = 0x80
        };

        Partition(size_t sector_count, Status s = Status::BOOTABLE, Type t = Type::FAT32_LBA);
        uint32_t sector_count() const noexcept;

        void serialize(uint8_t* into, const DiskGeometry& geometry, size_t lba_offset) const;

    private:
        Status m_status { Status::INACTIVE };
        Type m_type { Type::FREE };
        uint32_t m_sector_count;
    };

    size_t add_partition(const Partition& partition);

private:
    static constexpr size_t mbr_size = 512;
    static constexpr size_t partition_entry_size = 16;
    static constexpr size_t partition_count = 4;
    static constexpr size_t partition_base = 446;
    static constexpr size_t mpt_size = partition_count * partition_entry_size;

    uint8_t m_mbr[mbr_size];
    size_t m_active_partition;
    size_t m_active_lba_offset;
    size_t m_initial_lba_offset;
    DiskGeometry m_DiskGeometry;

private:
    void validate_mbr();
};
