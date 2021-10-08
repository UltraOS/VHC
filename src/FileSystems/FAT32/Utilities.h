#pragma once

// Imported from the Ultra repo and tweaked to use STL types.

#include <string>
#include <string_view>
#include <cstdint>
#include <utility>
#include <algorithm>

namespace FAT {

static constexpr size_t short_name_length = 8;
static constexpr size_t short_extension_length = 3;
static constexpr uint8_t max_sequence_number = 20;

std::pair<size_t, size_t> length_of_name_and_extension(std::string_view file_name)
{
    auto last_dot = file_name.find_last_of(".");
    size_t name_length = (last_dot == std::string_view::npos) ? file_name.size() : last_dot;

    size_t extension_length = 0;

    if (name_length == 0)
        name_length = file_name.size();
    else if (name_length < file_name.size())
        extension_length = file_name.size() - name_length - 1;

    return { name_length, extension_length };
}

std::string generate_short_name(std::string_view long_name)
{
    std::string short_name;

    auto* long_name_ptr = long_name.data();

    auto [name_length, extension_length] = length_of_name_and_extension(long_name_ptr);

    bool needs_numeric_tail = false;

    if (name_length > short_name_length || extension_length > short_extension_length)
        needs_numeric_tail = true;

    auto name_chars_to_copy = std::min(needs_numeric_tail ? short_name_length - 2 : short_name_length, name_length);

    size_t name_chars_copied = 0;
    for (size_t i = 0; i < name_length; ++i) {
        if (long_name[i] == '.' || long_name[i] == ' ')
            continue;

        short_name.push_back(long_name[i]);
        ++name_chars_copied;

        if (name_chars_to_copy == name_chars_copied)
            break;
    }

    if (needs_numeric_tail) {
        short_name.push_back('~');
        short_name.push_back('1');
        name_chars_copied += 2;
    }

    auto padding_chars = short_name_length - name_chars_copied;
    while (padding_chars--)
        short_name.push_back(' ');

    auto extension_chars = std::min(short_extension_length, extension_length);
    padding_chars = short_extension_length - extension_chars;

    if (extension_chars) {
        size_t extension_index = name_length + 1;
        size_t last = extension_index + extension_chars;

        for (size_t i = extension_index; i < last; ++i)
            short_name.push_back(long_name[i]);
    }

    while (padding_chars--)
        short_name.push_back(' ');

    std::for_each(short_name.begin(), short_name.end(), [](char& c) { c = std::toupper(c); });

    return short_name;
}

std::string next_short_name(std::string_view current, bool& ok)
{
    if (std::string_view(current.data() + 1, 7) == "~999999") {
        ok = false;
        return {};
    }

    ok = true;

    std::string new_short_name(current);

    auto numeric_tail_pos = current.find_last_of("~");
    auto end_of_name = current.find(" ");

    if (numeric_tail_pos == std::string::npos) {
        if (end_of_name == std::string::npos)
            end_of_name = short_name_length - 2;

        auto new_tail_pos = std::min(short_name_length - 2, end_of_name);

        new_short_name[new_tail_pos + 0] = '~';
        new_short_name[new_tail_pos + 1] = '1';
        return new_short_name;
    }

    if (end_of_name == std::string::npos)
        end_of_name = short_name_length + 1;

    auto end_of_numeric_tail = std::min(end_of_name, short_name_length);

    size_t number = 0;
    bool would_overflow = true;

    for (size_t i = numeric_tail_pos + 1; i < end_of_numeric_tail; ++i) {
        number *= 10;
        number += new_short_name[i] - '0';
        would_overflow &= (new_short_name[i] == '9');
    }

    number++;

    if (!would_overflow) {
        for (size_t i = end_of_numeric_tail - 1; i > numeric_tail_pos; --i) {
            new_short_name[i] = static_cast<char>(number % 10) + '0';
            number /= 10;
        }

        return new_short_name;
    }

    bool can_grow_downwards = end_of_numeric_tail != short_name_length;

    size_t new_start = can_grow_downwards ? numeric_tail_pos : numeric_tail_pos - 1;
    size_t new_end = can_grow_downwards ? end_of_numeric_tail + 1 : end_of_numeric_tail;

    for (size_t i = new_end - 1; i > new_start; --i) {
        new_short_name[i] = static_cast<char>(number % 10) + '0';
        number /= 10;
    }

    new_short_name[new_start] = '~';
    return new_short_name;
}

uint8_t generate_short_name_checksum(std::string_view name)
{
    uint8_t sum = 0;
    const uint8_t* byte_ptr = reinterpret_cast<const uint8_t*>(name.data());

    for (size_t i = short_name_length + short_extension_length; i != 0; i--) {
        sum = (sum >> 1) + ((sum & 1) << 7);
        sum += *byte_ptr++;
    }

    return sum;
}

struct FilenameInfo {
    std::string_view name;
    std::string_view extension;

    bool is_name_entirely_lower;
    bool is_extension_entirely_lower;
    bool is_vfat;
};

FilenameInfo analyze_filename(std::string_view name)
{
    auto [name_length, extension_length] = length_of_name_and_extension(name);

    FilenameInfo info {};

    info.is_vfat = name_length > short_name_length || extension_length > short_extension_length || name_length == 0;

    bool is_name_entirely_upper = false;
    info.is_name_entirely_lower = false;

    bool is_extension_entirely_upper = true;
    info.is_extension_entirely_lower = false;

    static constexpr char minimum_allowed_ascii_value = 0x20;

    static const char banned_characters[] = { '"', '*', '/', ':', '<', '>', '?', '\\', '|' };
    static constexpr size_t banned_characters_length = sizeof(banned_characters);

    static const char banned_but_allowed_in_vfat_characters[] = { '.', '+', ',', ';', '=', '[', ']' };
    static constexpr size_t banned_but_allowed_in_vfat_characters_length = sizeof(banned_but_allowed_in_vfat_characters);

    auto is_one_of = [](char c, const char* set, size_t length) -> bool {
        for (size_t i = 0; i < length; ++i) {
            if (c == set[i])
                return true;
        }

        return false;
    };

    for (char c : name) {
        if (is_one_of(c, banned_characters, banned_characters_length) || c < minimum_allowed_ascii_value)
            throw std::runtime_error("invalid FAT32 filename");
    }

    info.name = std::string_view(name.data(), name_length);
    info.extension = std::string_view(info.name.data() + info.name.size() + 1, extension_length);

    if (!info.is_vfat) {
        auto count_lower_and_upper_chars = [](std::string_view node, size_t& lower_count, size_t& upper_count) {
            for (char c : node) {
                lower_count += std::islower(c);
                upper_count += std::isupper(c);
            }
        };

        size_t name_lower_chars = 0;
        size_t name_upper_chars = 0;
        count_lower_and_upper_chars(info.name, name_lower_chars, name_upper_chars);

        info.is_name_entirely_lower = name_lower_chars && !name_upper_chars;
        is_name_entirely_upper = !name_lower_chars; // name_upper_chars don't matter

        if (extension_length) {
            size_t extension_lower_chars = 0;
            size_t extension_upper_chars = 0;
            count_lower_and_upper_chars(info.extension, extension_lower_chars, extension_upper_chars);

            info.is_extension_entirely_lower = extension_lower_chars && !extension_upper_chars;
            is_extension_entirely_upper = !extension_lower_chars; // extension_upper_chars don't matter
        }

        info.is_vfat = !((info.is_name_entirely_lower || is_name_entirely_upper) && (info.is_extension_entirely_lower || is_extension_entirely_upper));

        if (!info.is_vfat) {
            bool contains_banned_for_non_vfat = false;

            for (char c : info.name) {
                contains_banned_for_non_vfat |= is_one_of(c, banned_but_allowed_in_vfat_characters, banned_but_allowed_in_vfat_characters_length);

                if (contains_banned_for_non_vfat)
                    break;
            }

            if (extension_length) {
                for (char c : info.extension) {
                    contains_banned_for_non_vfat |= is_one_of(c, banned_but_allowed_in_vfat_characters, banned_but_allowed_in_vfat_characters_length);

                    if (contains_banned_for_non_vfat)
                        break;
                }
            }

            info.is_vfat = contains_banned_for_non_vfat;
        }
    }

    return info;
}

}
