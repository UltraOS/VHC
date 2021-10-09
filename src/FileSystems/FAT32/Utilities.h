#pragma once

// Imported from the Ultra repo and tweaked to use STL types.

#include <string>
#include <string_view>
#include <cstdint>
#include <utility>
#include <algorithm>

#include "Utilities/Common.h"

namespace FAT {

static constexpr size_t short_name_length = 8;
static constexpr size_t short_extension_length = 3;
static constexpr uint8_t max_sequence_number = 20;

std::pair<size_t, size_t> length_of_name_and_extension(std::string_view file_name);

std::string generate_short_name(std::string_view long_name);
std::string next_short_name(std::string_view current, bool& ok);
uint8_t generate_short_name_checksum(std::string_view name);

struct FilenameInfo {
    std::string_view name;
    std::string_view extension;

    bool is_name_entirely_lower;
    bool is_extension_entirely_lower;
    bool is_vfat;
};
FilenameInfo analyze_filename(std::string_view name);

PACKED(struct EBPB
{
    // BPB
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t max_root_dir_entries;
    uint16_t total_logical_sectors_legacy;
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat_legacy;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_logical_sectors;

    // EBPB
    uint32_t sectors_per_fat;
    uint16_t ext_flags;
    uint16_t version;
    uint32_t root_dir_cluster;
    uint16_t fs_information_sector;
    uint16_t backup_boot_sectors;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  unused_3;
    uint8_t  signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     filesystem_type[8];
});

}
