#pragma once

#include <iostream>
#include <stdexcept>
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>
#include <cstring>
#include <algorithm>
#include <filesystem>

#include "AutoFile.h"

class ArgParser
{
public:
    using help_callback = std::function<void()>;
private:
    enum class ArgType
    {
        flag,
        param,
        list,
        help
    };

    struct ArgSpec
    {
        std::string as_full;
        char        as_short;
        ArgType     type;
        std::string description;
        bool        is_optional;

        bool is_list()  const noexcept { return type == ArgType::list; }
        bool is_param() const noexcept { return type == ArgType::param; }
        bool is_flag()  const noexcept { return type == ArgType::flag; }
        bool is_help()  const noexcept { return type == ArgType::help; }
    };

    help_callback m_help_callback;
    std::vector<ArgSpec> m_args;
    std::unordered_map<std::string, std::vector<std::string>> m_parsed_args;
public:
    ArgParser& add_param(std::string_view full_arg, char short_arg, std::string_view description, bool optional = true)
    {
        return add_custom(full_arg, short_arg, ArgType::param, description, optional);
    }

    ArgParser& add_flag(std::string_view full_arg, char short_arg, std::string_view description, bool optional = true)
    {
        return add_custom(full_arg, short_arg, ArgType::flag, description, optional);
    }

    ArgParser& add_list(std::string_view full_arg, char short_arg, std::string_view description, bool optional = true)
    {
        return add_custom(full_arg, short_arg, ArgType::list, description, optional);
    }

    ArgParser& add_help(std::string_view full_arg, char short_arg, std::string_view description, help_callback on_help_requested)
    {
        m_help_callback = on_help_requested;

        return add_custom(full_arg, short_arg, ArgType::help, description, true);
    }

    void parse(int argc, char** argv)
    {
        if (argc < 2) {
            if (m_help_callback)
                m_help_callback();
            return;
        }

        ArgSpec* active_spec = nullptr;

        for (auto arg_index = 1; arg_index < argc; ++arg_index)
        {
            const char* current_arg = argv[arg_index];

            bool is_new_arg = is_arg(current_arg);

            if (active_spec && is_new_arg)
            {
                if ((active_spec->is_param() || active_spec->is_list()) && m_parsed_args[active_spec->as_full].empty())
                    throw std::runtime_error(std::string("Expected an argument for ") + active_spec->as_full);
            }

            if (active_spec && !is_new_arg)
            {
                if (active_spec->is_flag())
                    throw std::runtime_error(std::string("Unexpected argument ") + current_arg);
                else if (active_spec->is_param() && m_parsed_args[active_spec->as_full].size() == 1)
                    throw std::runtime_error(std::string("Too many arguments for " + active_spec->as_full));

                m_parsed_args[active_spec->as_full].emplace_back(current_arg);
                continue;
            }

            std::string as_full_arg = extract_full_arg(current_arg);
            if (as_full_arg.empty())
                throw std::runtime_error(std::string("Unexpected argument ") + current_arg);
            else
                active_spec = &arg_spec_of(as_full_arg);

            if (active_spec->is_help())
            {
                if (m_help_callback)
                    m_help_callback();

                return;
            }

            m_parsed_args[as_full_arg];
        }

        ensure_mandatory_args_are_satisfied();
    }

    const std::vector<std::string>& get_list(std::string_view arg)
    {
        ensure_arg_is_parsed(arg);

        return m_parsed_args.at(std::string(arg));
    }

    const std::vector<std::string>& get_list_or(std::string_view arg, const std::vector<std::string>& default_value)
    {
        if (is_arg_parsed(arg))
            return get_list(arg);

        return default_value;
    }

    const std::string& get(std::string_view arg)
    {
        return get_list(arg)[0];
    }

    const std::string& get_or(std::string_view arg, const std::string& default_value)
    {
        if (is_arg_parsed(arg))
            return get(arg);

        return default_value;
    }

    uint64_t get_uint(std::string_view arg)
    {
        return std::stoull(get(arg));
    }

    uint64_t get_uint_or(std::string_view arg, uint64_t default_value)
    {
        if (is_arg_parsed(arg))
            return get_uint(arg);

        return default_value;
    }

    int64_t get_int(std::string_view arg)
    {
        return std::stoll(get(arg));
    }

    int64_t get_int_or(std::string_view arg, int64_t default_value)
    {
        if (is_arg_parsed(arg))
            return get_int(arg);

        return default_value;
    }

    bool is_set(std::string_view arg)
    {
        return is_arg_parsed(arg);
    }

    const std::vector<std::string>& get_list(char arg)
    {
        const auto& arg_spec = arg_spec_of(arg);

        return get_list(arg_spec.as_full);
    }

    std::string_view get(char arg)
    {
        const auto& arg_spec = arg_spec_of(arg);

        return get(arg_spec.as_full);
    }

    uint64_t get_uint(char arg)
    {
        const auto& arg_spec = arg_spec_of(arg);

        return get_uint(arg_spec.as_full);
    }

    uint64_t get_int(char arg)
    {
        const auto& arg_spec = arg_spec_of(arg);

        return get_int(arg_spec.as_full);
    }

    bool is_set(char arg)
    {
        const auto& arg_spec = arg_spec_of(arg);

        return is_set(arg_spec.as_full);
    }

    std::ostream& print(std::ostream& stream = std::cout) const
    {
        for (const auto& arg : m_args)
        {
            stream << (arg.is_optional ? "(optional) " : "           ")
                   << "[--"
                   << arg.as_full
                   << "/-"
                   << arg.as_short
                   << "] "
                   << arg.description
                   << std::endl;
        }

        return stream;
    }

    friend std::ostream& operator<<(std::ostream& stream, const ArgParser& argp)
    {
        return argp.print(stream);
    }

private:
    void ensure_mandatory_args_are_satisfied()
    {
        for (const auto& arg : m_args)
        {
            if (arg.is_optional)
                continue;

            if (!m_parsed_args.count(arg.as_full))
                throw std::runtime_error(std::string("Expected a non-optional argument --") + arg.as_full);
        }
    }

    ArgParser& add_custom(std::string_view full_arg, char short_arg, ArgType type, std::string_view description, bool optional)
    {
        ArgSpec spec {};
        spec.as_full = full_arg;
        spec.as_short = short_arg;
        spec.type = type;
        spec.description = description;
        spec.is_optional = optional;

        m_args.emplace_back(std::move(spec));

        return *this;
    }

    bool is_arg_parsed(std::string_view arg)
    {
        return m_parsed_args.count(std::string(arg));
    }

    void ensure_arg_is_parsed(std::string_view arg)
    {
        if (!is_arg_parsed(arg))
            throw std::runtime_error("Couldn't find argument " + std::string(arg));
    }

    ArgSpec& arg_spec_of(std::string_view arg)
    {
        auto arg_itr = std::find_if(m_args.begin(),
                                    m_args.end(),
                                    [&](const ArgSpec& my_arg)
                                    {
                                       return my_arg.as_full == arg;
                                    });

        if (arg_itr == m_args.end())
            throw std::runtime_error("Couldn't find arg " + std::string(arg));

        return *arg_itr;
    }

    ArgSpec& arg_spec_of(char arg)
    {
        auto arg_itr = std::find_if(m_args.begin(),
                                    m_args.end(),
                                    [&](const ArgSpec& my_arg)
                                    {
                                        return my_arg.as_short == arg;
                                    });

        if (arg_itr == m_args.end())
            throw std::runtime_error(std::string("Couldn't find arg ") + arg);

        return *arg_itr;
    }

    bool is_arg(const char* arg)
    {
        size_t length = strlen(arg);

        switch (length)
        {
        case 0:
        case 1:
            return false;
        case 2:
            return arg[0] == '-';
        default:
            return arg[0] == '-' && arg[1] == '-';
        }
    }

    std::string extract_full_arg(const char* arg)
    {
        size_t length = strlen(arg);

        switch (length)
        {
        case 0:
        case 1:
            return {};
        case 2:
            if (arg[0] != '-')
                return {};
            return arg_spec_of(arg[1]).as_full;
        default:
            if (arg[0] != '-' || arg[1] != '-')
                return {};
            return arg_spec_of(std::string(arg + 2)).as_full;
        }
    }
};

struct DiskGeometry
{
    size_t total_sector_count;
    size_t cylinders;
    size_t heads;
    size_t sectors;

    bool within_chs_limit() const noexcept
    {
        if (heads > ((1 << 8) - 1))
            return false;
        if (sectors > ((1 << 6) - 1))
            return false;
        if (cylinders > ((1 << 10) - 1))
            return false;

        return true;
    }
};

struct CHS
{
    size_t cylinder;
    size_t head;
    size_t sector;
};

inline CHS to_chs(size_t lba, const DiskGeometry& geometry)
{
    CHS chs;

    chs.head = (lba / geometry.sectors) % geometry.heads;
    chs.cylinder = (lba / geometry.sectors) / geometry.heads;
    chs.sector = (lba % geometry.sectors) + 1;

    return chs;
}

#define KB  1024ull
#define MB (1024ull * KB)
#define GB (1024ull * MB)
#define TB (1024ull * GB)

inline std::pair<std::string, std::string> split_filename(std::string_view filename)
{
    std::filesystem::path p(filename);

    if (!p.has_stem())
        throw std::runtime_error("invalid filename " + std::string(filename));

    std::string extension = p.extension().string();

    // erase the '.'
    extension.erase(0);

    return { p.stem().string(), std::move(extension) };
}

inline std::vector<uint8_t> read_entire(std::string_view path)
{
    AutoFile f(path, AutoFile::Mode::READ);

    std::vector<uint8_t> data(f.size());
    f.read(data.data(), data.size());

    return data;
}
