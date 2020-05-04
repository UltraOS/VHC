#include "MBR.h"

MBR::Partition::Partition(size_t sector_count, MBR::Partition::status s, MBR::Partition::type t)
    : m_status(s), m_type(t), m_sector_count(static_cast<uint32_t>(sector_count))
{

}

void MBR::Partition::serialize(uint8_t* into, const disk_geometry& geometry, size_t lba_offset) const
{
    if (m_type == Partition::type::FAT32_CHS || geometry.within_chs_limit())
    {
        auto chs_begin = to_chs(lba_offset, geometry);
        auto chs_end = to_chs(lba_offset, geometry);

        if (chs_begin.head > ((1 << 8) - 1) || chs_end.head > ((1 << 8) - 1))
            throw std::runtime_error("Head number cannot exceed 2^7");
        if (chs_begin.sector > ((1 << 6) - 1) || chs_end.sector > ((1 << 6) - 1))
            throw std::runtime_error("Sector number cannot exceed 2^5");
        if (chs_begin.cylinder > ((1 << 10) - 1) || chs_end.cylinder > ((1 << 10) - 1))
            throw std::runtime_error("Sector number cannot exceed 2^9");

        // CC SSSSSS
        // 00 000000
        uint8_t sector_cylinder_begin = static_cast<uint8_t>(chs_begin.sector);
        uint8_t sector_cylinder_end = static_cast<uint8_t>(chs_end.sector);

        sector_cylinder_begin |= chs_begin.cylinder & 0b1100000000;
        sector_cylinder_end   |= chs_end.cylinder   & 0b1100000000;

        uint8_t cylinder_begin = chs_begin.cylinder & 0b0011111111;
        uint8_t cylinder_end   = chs_end.cylinder   & 0b0011111111;

        into[1] = static_cast<uint8_t>(chs_begin.head);
        into[2] = sector_cylinder_begin;
        into[3] = cylinder_begin;
        into[5] = static_cast<uint8_t>(chs_end.head);
        into[6] = sector_cylinder_end;
        into[7] = cylinder_end;
    }
    else if (m_type == Partition::type::FAT32_LBA)
    {
        into[1] = 0xFF;
        into[2] = 0xFF;
        into[3] = 0xFF;
        into[5] = 0xFF;
        into[6] = 0xFF;
        into[7] = 0xFF;
    }

    into[0] = static_cast<uint8_t>(m_status);
    into[4] = static_cast<uint8_t>(m_type);

    *reinterpret_cast<uint32_t*>(&into[8]) = static_cast<uint32_t>(lba_offset);
    *reinterpret_cast<uint32_t*>(&into[12]) = static_cast<uint32_t>(m_sector_count);
}

uint32_t MBR::Partition::sector_count() const noexcept
{
    return m_sector_count;
}

MBR::MBR(const std::string& path, const disk_geometry& geometry)
    : m_mbr{}, m_active_partition(0), m_disk_geometry(geometry), m_active_lba_offset(1)
{
    auto mbr_file = fopen(path.c_str(), "rb");
    READ_EXACTLY(mbr_file, m_mbr, mbr_size);
    fclose(mbr_file);
    validate_mbr();
}

void MBR::write_into(DiskImage& image)
{
    image.write(m_mbr, mbr_size);
}

size_t MBR::add_partition(const Partition& partition)
{
    size_t partition_offset = partition_base + (m_active_partition * partition_entry_size);

    size_t partition_lba_offset = m_active_lba_offset;

    partition.serialize(&m_mbr[partition_offset], m_disk_geometry, partition_lba_offset);

    m_active_lba_offset += partition.sector_count();

    return partition_lba_offset;
}

void MBR::validate_mbr()
{
    uint8_t expected_partition_data[mpt_size]{};

    if (memcmp(&m_mbr[partition_base], expected_partition_data, mpt_size))
        throw std::runtime_error("Detected a non-empty MPT");

    if (m_mbr[510] != 0x55 && m_mbr[511] != 0xAA)
        throw std::runtime_error("Incorrect bootsector signature, has to end with 0x55AA");
}
