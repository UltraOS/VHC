#pragma once

#include <utility>
#include <vector>
#include <cstdint>

#include "DiskImages/DiskImage.h"
#include "FAT32.h"

namespace FAT {

class FileAllocationTable
{
public:
    FileAllocationTable(FAT32& parent, uint32_t capacity, uint32_t padded_capacity);

    size_t size_in_clusters() const { return size_in_sectors() / m_parent.sectors_per_cluster(); }
    uint32_t size_in_sectors() const { return ceiling_divide<size_t>((m_padded_capacity * 4ull), DiskImage::sector_size); }

    uint32_t allocate(uint32_t cluster_count, uint32_t connect_to = free_cluster);
    void write_into(DiskImage& image, size_t count = 2);

    [[nodiscard]] uint32_t get_entry(uint32_t index) const;
    [[nodiscard]] uint32_t free_cluster_count() const { return m_table.size() - m_next_to_allocate; }
    [[nodiscard]] uint32_t last_allocated() const { return m_next_to_allocate - 1; }

private:
    uint32_t next_free(uint32_t after_index = 1) const;
    bool has_atleast(uint32_t free_clusters) const;
    void put_entry(uint32_t cluster, uint32_t value);
    void ensure_legal_cluster(uint32_t index) const;

private:
    static constexpr uint32_t max_cluster_index = 0x0FFFFFEF;
    static constexpr uint8_t hard_disk_media_descriptor = 0xF8;
    static constexpr uint32_t end_of_chain = 0x0FFFFFFF;
    static constexpr uint32_t free_cluster = 0x00000000;

    FAT32& m_parent;

    std::vector<uint32_t> m_table;

    // real cluster value is m_next_to_allocate + 2
    uint32_t m_next_to_allocate { 0 };
    uint32_t m_padded_capacity { 0 };
};

}
