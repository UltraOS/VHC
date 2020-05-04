#pragma once
#include <string>

#include <vector>
#include "FileSystem.h"

class FAT32 final : public FileSystem
{
private:
    static constexpr uint32_t max_cluster_index = 0x0FFFFFEF;
    static constexpr uint32_t end_of_chain = 0x0FFFFFF8;
    static constexpr uint8_t hard_disk_media_descriptor = 0xF8;
    static constexpr uint32_t free_cluster = 0x00000000;
public:
    class FileAllocationTable
    {
    private:
        std::vector<uint32_t> m_table;
    public:
        FileAllocationTable(size_t length_in_clusters);

        size_t size_in_clusters();

        uint32_t allocate(uint32_t cluster_count);
        void write_into(DiskImage& image, size_t count = 2);

        std::ostream& ls(std::ostream& stream = std::cout) const;
        friend std::ostream& operator<<(std::ostream& stream, const FileAllocationTable& table);

        uint32_t get_entry(uint32_t index) const;
    private:
        uint32_t next_free(uint32_t after_index = 1) const;
        bool has_atleast(uint32_t free_clusters) const;
        void put_entry(uint32_t cluster, uint32_t value);
        void ensure_legal_cluster(uint32_t index) const;
    };
private:
    static constexpr size_t vbr_size = 512;
    uint8_t m_vbr[vbr_size];
    size_t m_sector_count;
    size_t m_sectors_per_cluster;
    FileAllocationTable m_fat_table;
public:
    FAT32(const std::string& vbr_path, size_t lba_offset, size_t sector_count);

    void write_into(DiskImage& image) override;

    void add_file(const std::string& path) override;
private:
    void validate_vbr();

    size_t pick_sectors_per_cluster();
};