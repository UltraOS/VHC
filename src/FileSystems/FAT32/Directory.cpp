#include <ctime>

#include "DiskImages/DiskImage.h"

#include "Utilities.h"
#include "Directory.h"
#include "FileAllocationTable.h"

namespace FAT {

Directory::Directory(FAT32& parent)
    : m_parent(parent)
{
    m_current_cluster = parent.allocation_table().allocate(1);
    m_first_cluster = m_current_cluster;
}

Directory::Directory(FAT32& parent, size_t cluster)
    : m_parent(parent)
    , m_first_cluster(cluster)
    , m_current_cluster(cluster)
    , m_offset_within_cluster(2) // '.' and '..' are already created by the parent directory
{
}

bool Directory::has_subdirectory(std::string_view name)
{
    auto res = std::find_if(m_entries.begin(), m_entries.end(), [&name](const StoredEntry& entry) { return entry.name == name; });

    return res != m_entries.end() && res->directory;
}

Directory& Directory::subdirectory(std::string_view name)
{
    auto res = std::find_if(m_entries.begin(), m_entries.end(), [&name](const StoredEntry& entry) { return entry.name == name; });
    if (res == m_entries.end())
        throw std::runtime_error("no such subdirectory " + std::string(name));

    if (!res->directory)
        throw std::runtime_error("not a directory " + std::string(name));

    return *res->directory;
}

bool Directory::contains_short_name(std::string_view short_name) const
{
    auto res = std::find_if(m_entries.begin(), m_entries.end(),
        [&short_name](const StoredEntry& entry) {
            return entry.stored_short_name == short_name;
        }
    );

    return res != m_entries.end();
}

bool Directory::contains_name(std::string_view name) const
{
    auto res = std::find_if(m_entries.begin(), m_entries.end(),
        [&name](const StoredEntry& entry) {
            return entry.name == name;
        }
    );

    return res != m_entries.end();
}

void Directory::build_entry(Entry& entry, const EntrySpec& spec)
{
    memcpy(entry.filename, spec.name.data(), max_filename_length);
    memcpy(entry.extension, spec.name.data() + max_filename_length, max_file_extension_length);

    auto now = std::time(nullptr);
    auto time = *std::gmtime(&now);

    entry.created_ms = 0;

    uint16_t sec = time.tm_sec / 2;
    uint16_t min = time.tm_min;
    uint16_t hr = time.tm_hour;
    uint16_t yr = time.tm_year - 80;
    uint16_t mt = time.tm_mon + 1;
    uint16_t d = time.tm_mday;

    entry.created_time = (hr << 11) | (min << 5) | sec;
    entry.created_date = (yr << 9) | (mt << 5) | d;
    entry.last_accessed_date = entry.created_date;
    entry.last_modified_date = entry.created_date;
    entry.last_modified_time = entry.created_time;

    entry.cluster_high = (spec.first_cluster & 0xFFFF0000) >> 16;
    entry.cluster_low = spec.first_cluster & 0x0000FFFF;
    entry.size = spec.size;

    if (spec.is_directory)
        entry.attributes |= subdirectory_bit;

    if (spec.is_name_lower)
        entry.case_info |= lowercase_name_bit;
    if (spec.is_extension_lower)
        entry.case_info |= lowercase_extension_bit;
}

void Directory::store_normal_entry(const EntrySpec& spec, uint32_t current_cluster, uint32_t offset_within_cluster)
{
    auto entry = Entry();
    build_entry(entry, spec);

    store_entry(&entry, current_cluster, offset_within_cluster);
}

void Directory::store_normal_entry(const EntrySpec& spec)
{
    auto entry = Entry();
    build_entry(entry, spec);

    store_entry(&entry);
}

void Directory::store_dot_and_dot_dot(size_t cluster, size_t parent_cluster)
{
    EntrySpec spec {};
    spec.size = 0;
    spec.is_directory = true;

    spec.first_cluster = cluster;
    spec.name = ".          ";
    store_normal_entry(spec, cluster, 0);

    spec.first_cluster = (parent_cluster == 2) ? 0 : parent_cluster;
    spec.name = "..         ";
    store_normal_entry(spec, cluster, 1);
}

void Directory::do_store(std::string_view name, const std::vector<uint8_t>& data, bool is_directory)
{
    if (contains_name(name))
        throw std::runtime_error(std::string(name) + " already exists");
    if (name.size() > 255)
        throw std::runtime_error("name is too long");

    auto info = analyze_filename(name);
    info.is_vfat &= m_parent.use_vfat();

    auto short_name = generate_short_name(name);

    while (contains_short_name(short_name)) {
        bool ok = false;

        short_name = next_short_name(short_name, ok);

        if (!ok)
            throw std::runtime_error("too many short name file collisions");
    }

    if (info.is_vfat) {
        struct NamePiece {
            const char* ptr;
            size_t characters;
        } pieces[max_sequence_number]{};

        auto generate_name_pieces = [](std::string_view name, NamePiece(&pieces)[20]) {
            size_t characters_left = name.size();
            const char* name_ptr = name.data();

            for (size_t i = 0;; i++) {
                auto characters_for_this_piece = std::min(characters_per_entry, characters_left);
                characters_left -= characters_for_this_piece;

                pieces[i].ptr = name_ptr;
                pieces[i].characters = characters_for_this_piece;

                name_ptr += characters_for_this_piece;

                if (characters_left == 0)
                    break;
            }
        };

        generate_name_pieces(name, pieces);

        auto entries_to_write = ceiling_divide(name.size(), characters_per_entry);
        auto sequence_number = entries_to_write;

        auto write_long_directory_character = [](LongEntry& entry, uint16_t character, size_t index) {
            if (index < name_1_characters) {
                entry.name_1[index * 2] = character >> 0;
                entry.name_1[index * 2 + 1] = character >> 8;
            } else if (index < (name_1_characters + name_2_characters)) {
                index -= name_1_characters;
                entry.name_2[index * 2] = character >> 0;
                entry.name_2[index * 2 + 1] = character >> 8;
            } else if (index < characters_per_entry) {
                index -= name_1_characters + name_2_characters;
                entry.name_3[index * 2] = character >> 0;
                entry.name_3[index * 2 + 1] = character >> 8;
            }
        };

        LongEntry long_entry{};

        auto checksum = generate_short_name_checksum(short_name);
        long_entry.checksum = checksum;

        static constexpr size_t vfat_name_attributes = 0x0F;
        long_entry.attributes = vfat_name_attributes;

        while (sequence_number--) {
            long_entry.sequence_number = sequence_number + 1;

            if ((sequence_number + 1) == entries_to_write)
                long_entry.sequence_number |= last_logical_entry_bit;

            size_t characters_written = 0;
            auto& this_piece = pieces[sequence_number];

            for (; characters_written < this_piece.characters; ++characters_written)
                write_long_directory_character(long_entry, static_cast<uint16_t>(this_piece.ptr[characters_written]), characters_written);

            if (characters_written < characters_per_entry)
                write_long_directory_character(long_entry, 0, characters_written++);

            while (characters_written < characters_per_entry)
                write_long_directory_character(long_entry, 0xFFFF, characters_written++);

            store_entry(&long_entry);
        }
    }

    uint32_t first_cluster = 0;

    StoredEntry stored_entry{};
    stored_entry.name = name;
    stored_entry.stored_short_name = short_name;

    if (is_directory) {
        first_cluster = m_parent.allocation_table().allocate(1);
        store_dot_and_dot_dot(first_cluster, m_first_cluster);

        stored_entry.directory = std::unique_ptr<Directory>(new Directory(m_parent, first_cluster));
    }

    m_entries.emplace_back(std::move(stored_entry));

    if (!data.empty()) {
        auto clusters_needed = ceiling_divide(data.size(), m_parent.sectors_per_cluster() * DiskImage::sector_size);
        first_cluster = m_parent.allocation_table().allocate(clusters_needed);
        m_parent.image().write_at(data.data(), data.size(), m_parent.cluster_to_byte_offset(first_cluster));

        if (is_directory)
            throw std::runtime_error("non-empty data for directory");
    }

    EntrySpec spec{};
    spec.first_cluster = first_cluster;
    spec.is_directory = is_directory;
    spec.is_extension_lower = info.is_extension_entirely_lower;
    spec.is_name_lower = info.is_name_entirely_lower;
    spec.size = data.size();
    spec.name = short_name;
    store_normal_entry(spec);
}

void Directory::store_file(std::string_view name, const std::vector<uint8_t>& data)
{
    do_store(name, data, false);
}

void Directory::store_directory(std::string_view name)
{
    do_store(name, {}, true);
}

void Directory::store_entry(void* entry)
{
    auto entries_per_cluster = (m_parent.sectors_per_cluster() * DiskImage::sector_size) / entry_size;

    if (m_offset_within_cluster == entries_per_cluster) {
        m_current_cluster = m_parent.allocation_table().allocate(1, m_current_cluster);
        m_offset_within_cluster = 0;
    }

    store_entry(entry, m_current_cluster, m_offset_within_cluster++);
}

void Directory::store_entry(void* entry, uint32_t cluster, uint32_t entry_index)
{
    auto offset = m_parent.cluster_to_byte_offset(cluster) + entry_index * sizeof(Entry);
    m_parent.image().write_at(entry, sizeof(Entry), offset);
}

}
