#pragma once

#include <string_view>
#include <cstdint>
#include <vector>
#include <map>

#include "DiskImages/DiskImage.h"
#include "FAT32.h"

namespace FAT {

class Directory
{
public:
    Directory(FAT32& parent);

    void store_file(std::string_view name, const std::vector<uint8_t>& data);
    void store_directory(std::string_view name);

    [[nodiscard]] bool has_subdirectory(std::string_view name);
    [[nodiscard]] Directory& subdirectory(std::string_view name);

private:
    Directory(FAT32& parent, size_t cluster);

    void do_store(std::string_view name, const std::vector<uint8_t>& data, bool is_directory);

    bool contains_name(std::string_view name) const;
    bool contains_short_name(std::string_view short_name) const;

    static constexpr uint8_t lowercase_name_bit = 1 << 3;
    static constexpr uint8_t lowercase_extension_bit = 1 << 4;

    static constexpr uint8_t subdirectory_bit = 1 << 4;

    struct Entry
    {
        char filename[8];
        char extension[3];
        uint8_t attributes;
        uint8_t case_info;
        uint8_t created_ms;
        uint16_t created_time;
        uint16_t created_date;
        uint16_t last_accessed_date;
        uint16_t cluster_high;
        uint16_t last_modified_time;
        uint16_t last_modified_date;
        uint16_t cluster_low;
        uint32_t size;
    };

    static constexpr size_t name_1_characters = 5;
    static constexpr size_t name_2_characters = 6;
    static constexpr size_t name_3_characters = 2;
    static constexpr size_t characters_per_entry = name_1_characters + name_2_characters + name_3_characters;
    static constexpr size_t bytes_per_ucs2_char = 2;

    static constexpr uint8_t last_logical_entry_bit = 1 << 6;
    static constexpr uint8_t sequence_bits_mask = 0b11111;

    struct LongEntry
    {
        uint8_t sequence_number;
        uint8_t name_1[name_1_characters * bytes_per_ucs2_char];
        uint8_t attributes;
        uint8_t type;
        uint8_t checksum;
        uint8_t name_2[name_2_characters * bytes_per_ucs2_char];
        uint16_t first_cluster;
        uint8_t name_3[name_3_characters * bytes_per_ucs2_char];
    };

    struct EntrySpec {
        std::string_view name;
        uint32_t first_cluster;
        size_t size;
        bool is_name_lower;
        bool is_extension_lower;
        bool is_directory;
    };
    void build_entry(Entry&, const EntrySpec&);

    void store_normal_entry(const EntrySpec&, uint32_t cluster, uint32_t offset);
    void store_normal_entry(const EntrySpec&);
    void store_dot_and_dot_dot(size_t cluster, size_t parent_cluster);

    void store_entry(void*);
    void store_entry(void*, uint32_t cluster, uint32_t offset_within_cluster);

private:
    static constexpr size_t entry_size = 32;
    static_assert(sizeof(Entry) == entry_size, "Incorrect Entry size, you might wanna force the alignment of 1 manually");
    static_assert(sizeof(LongEntry) == entry_size, "Incorrect LongEntry size, you might wanna force the alignment of 1 manually");

    static constexpr size_t max_filename_length = 8;
    static constexpr size_t max_file_extension_length = 3;


    FAT32& m_parent;

    size_t m_bytes_per_cluster { 0 };
    size_t m_size { 0 };

    size_t m_offset_within_cluster { 0 };
    size_t m_first_cluster { 0 };
    size_t m_current_cluster { 0 };

    struct StoredEntry {
        std::string name;
        std::string stored_short_name; // to prevent collisions

        // nullptr -> not a directory
        std::unique_ptr<Directory> directory;
    };
    std::vector<StoredEntry> m_entries;
};

}
