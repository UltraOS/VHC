#pragma once

#include "Utility.h"
#include "FileSystem.h"

class FAT16 : public FileSystem
{
public:
    class FileAllocationTable
    {
    private:
        static constexpr uint16_t clusters = 3072;
        static constexpr uint16_t free_cluster = 0x0;
        static constexpr uint16_t EOC_mark = 0xFF8;
        static constexpr uint16_t FAT_clusters = 9;
        static constexpr uint16_t cluster_size = 512;
        static constexpr uint16_t fat_table_size = cluster_size * FAT_clusters;
        std::vector<uint8_t> m_table;
    public:
        FileAllocationTable();

        uint16_t allocate(uint16_t clusters);

        size_t free_clusters() const;

        bool has_atleast(uint16_t clusters) const;

        std::ostream& ls(std::ostream& stream = std::cout) const;

        friend std::ostream& operator<<(std::ostream& stream, const FileAllocationTable& table);

        void emplace_tables_into(DiskImage& image, size_t count = 2);

        void dump_into(DiskImage& image);
    public:
        uint16_t next_free(uint16_t after_offset = 2) const;

        void put_entry(size_t index, uint16_t val);

        uint16_t get_entry(size_t index) const;
    };

    class RootDirectory
    {
    private:
        struct Entry
        {
            char filename[8];
            char extension[3];
            uint8_t attributes;
            uint8_t unused[10];
            uint8_t time[2];
            uint8_t date[2];
            uint16_t cluster;
            uint32_t size;
        };

        static constexpr size_t max_filename_length = 8;
        static constexpr size_t max_file_extension_length = 3;
        static constexpr size_t rootdir_entry_count = 224;
        static constexpr size_t entry_size = 32;
        std::vector<Entry> m_directory_entries;
    public:
        void store_file(const std::string& name, const std::string& extension, uint16_t cluster, uint32_t size);

        bool dump_into(DiskImage& image);
    };

    FAT16(const std::string& bootsector_path, const std::vector<std::string>& other_files);

    void set_bootsector(const std::string& path);

    void store_file(const std::string& path);

    void dump(const std::string& path);
private:
    std::vector<uint8_t> m_bootsector;
    FileAllocationTable  m_file_allocation_table;
    RootDirectory        m_root_directory;
    std::vector<uint8_t> m_raw_data;
};