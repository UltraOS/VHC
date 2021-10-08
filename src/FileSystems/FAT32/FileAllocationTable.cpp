#include "FileAllocationTable.h"
#include <set>
namespace FAT {

FileAllocationTable::FileAllocationTable(FAT32& parent, uint32_t capacity, uint32_t padded_capacity)
    : m_parent(parent)
    , m_table(capacity + 2, 0)
    , m_padded_capacity(padded_capacity)
    , m_next_to_allocate(2)
{
    m_table[0] = 0x0FFFFFFF;
    memcpy(&m_table[0], &hard_disk_media_descriptor, sizeof(uint8_t));
    m_table[1] = end_of_chain;
}

uint32_t FileAllocationTable::allocate(uint32_t cluster_count, uint32_t connect_to)
{
    uint32_t first_cluster = 0;
    uint32_t prev_cluster = m_next_to_allocate - 1;

    if (!has_atleast(cluster_count))
        return 0;

    for (size_t i = 0; i < cluster_count; i++)
    {
        uint32_t cluster = next_free(prev_cluster);

        if (i == 0)
            first_cluster = cluster;

        if (i != 0)
            put_entry(prev_cluster, cluster);

        prev_cluster = cluster;

        put_entry(cluster, 0x1);
    }

    put_entry(prev_cluster, end_of_chain);
    m_next_to_allocate = prev_cluster + 1;

    if (connect_to)
        put_entry(connect_to, first_cluster);

    return first_cluster;
}

uint32_t FileAllocationTable::next_free(uint32_t after_index) const
{
    ensure_legal_cluster(after_index + 1);

    for (uint32_t i = after_index + 1; i < m_table.size(); ++i)
    {
        if (m_table[i] == free_cluster)
            return i;
    }

    return 0;
}

bool FileAllocationTable::has_atleast(uint32_t free_clusters) const
{
    uint32_t last_free = m_next_to_allocate;

    while (free_clusters--)
    {
        last_free = next_free(last_free);

        if (!last_free)
            return false;
    }

    return true;
}

void FileAllocationTable::put_entry(uint32_t cluster, uint32_t value)
{
    ensure_legal_cluster(cluster);

    m_table[cluster] = value;
}

void FileAllocationTable::ensure_legal_cluster(uint32_t index) const
{
    if (index < 2)
        throw std::runtime_error("First two entries of the file allocation table are reserved");
    if (index > max_cluster_index)
        throw std::runtime_error("Maximum cluster index is 0x0FFFFFEF");
    if (index > m_table.size() - 1)
        throw std::runtime_error("File allocation table overflow");
}

void FileAllocationTable::write_into(DiskImage& image, size_t count)
{
    auto padding_bytes = (m_padded_capacity - m_table.size()) * sizeof(uint32_t);

    while (count--) {
        image.write(m_table.data(), m_table.size() * sizeof(uint32_t));
        if (padding_bytes)
            image.skip(padding_bytes);
    }
}

uint32_t FileAllocationTable::get_entry(uint32_t index) const
{
    ensure_legal_cluster(index);

    return m_table[index];
}

}
