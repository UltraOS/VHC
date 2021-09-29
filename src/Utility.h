#pragma once

#include <iostream>
#include <stdexcept>
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>
#include <cstring>
#include <algorithm>

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

        bool is_list()  const noexcept { return type == ArgType::list;  }
        bool is_param() const noexcept { return type == ArgType::param; }
        bool is_flag()  const noexcept { return type == ArgType::flag;  }
        bool is_help()  const noexcept { return type == ArgType::help;  }
    };

    help_callback m_help_callback;
    std::vector<ArgSpec> m_args;
    std::unordered_map<std::string, std::vector<std::string>> m_parsed_args;
public:
    ArgParser& add_param(const std::string& full_arg, char short_arg, const std::string& description, bool optional = true)
    {
        return add_custom(full_arg, short_arg, ArgType::param, description, optional);
    }

    ArgParser& add_flag(const std::string& full_arg, char short_arg, const std::string& description, bool optional = true)
    {
        return add_custom(full_arg, short_arg, ArgType::flag, description, optional);
    }

    ArgParser& add_list(const std::string& full_arg, char short_arg, const std::string& description, bool optional = true)
    {
        return add_custom(full_arg, short_arg, ArgType::list, description, optional);
    }

    ArgParser& add_help(const std::string& full_arg, char short_arg, const std::string& description, help_callback on_help_requested)
    {
        m_help_callback = on_help_requested;

        return add_custom(full_arg, short_arg, ArgType::help, description, true);
    }

    void parse(int argc, char** argv)
    {
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

    const std::vector<std::string>& get_list(const std::string& arg)
    {
        ensure_arg_is_parsed(arg);

        return m_parsed_args[arg];
    }

    const std::string& get(const std::string& arg)
    {
        return get_list(arg)[0];
    }

    uint64_t get_uint(const std::string& arg)
    {
        return std::stoull(get(arg));
    }

    int64_t get_int(const std::string& arg)
    {
        return std::stoll(get(arg));
    }

    bool is_set(const std::string& arg)
    {
        return is_arg_parsed(arg);
    }

    const std::vector<std::string>& get_list(char arg)
    {
        const auto& arg_spec = arg_spec_of(arg);

        return get_list(arg_spec.as_full);
    }

    const std::string& get(char arg)
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

    ArgParser& add_custom(const std::string& full_arg, char short_arg, ArgType type, const std::string& description, bool optional)
    {
        m_args.push_back({ full_arg, short_arg, type, description, optional });

        return *this;
    }

    bool is_arg_parsed(const std::string& arg)
    {
        return m_parsed_args.count(arg);
    }

    void ensure_arg_is_parsed(const std::string& arg)
    {
        if (!is_arg_parsed(arg))
            throw std::runtime_error("Couldn't find argument " + arg);
    }

    ArgSpec& arg_spec_of(const std::string& arg)
    {
        auto arg_itr = std::find_if(m_args.begin(),
                                    m_args.end(),
                                    [&](const ArgSpec& my_arg)
                                    {
                                       return my_arg.as_full == arg;
                                    });

        if (arg_itr == m_args.end())
            throw std::runtime_error("Couldn't find arg " + arg);

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

inline std::string construct_path(const std::string& l, const std::string& r)
{
    char slash = l.find('\\') == std::string::npos ? '/' : '\\';

    bool l_slash = l.back()  == '/' || l.back()  == '\\';
    bool r_slash = r.front() == '/' || r.front() == '\\';

    if (l_slash && r_slash)
        return l + std::string(r, 1);
    else if (l_slash && !r_slash)
        return l + r;
    else if (!l_slash && r_slash)
        return l + r;
    else
        return l + slash + r;
}

struct disk_geometry
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

inline CHS to_chs(size_t lba, const disk_geometry& geometry)
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

class AutoFile
{
private:
    FILE* m_file;
public:
    AutoFile()
        : m_file(nullptr)
    {
    }

    AutoFile(const std::string& path, const std::string& mode)
        : m_file(nullptr)
    {
        open(path, mode);
    }

    AutoFile(const AutoFile& other_file) = delete;
    AutoFile& operator=(const AutoFile& other_file) = delete;

    AutoFile(AutoFile&& other_file) noexcept
        : m_file(other_file.m_file)
    {
        other_file.m_file = nullptr;
    }

    AutoFile& operator=(AutoFile&& other_file) noexcept
    {
        std::swap(m_file, other_file.m_file);

        return *this;
    }

    void open(const std::string& path, const std::string& mode)
    {
        if (m_file)
            fclose(m_file);

        m_file = fopen(path.c_str(), mode.c_str());

        if (!m_file)
            throw std::runtime_error("Failed to open file: " + path);
    }

    size_t size()
    {
        fseek(m_file, 0, SEEK_END);
        size_t file_size = ftell(m_file);
        rewind(m_file);
        return file_size;
    }

    void write(const std::string& data)
    {
        write(data.c_str(), data.size());
    }

    void write(const char* data, size_t size)
    {
        write(reinterpret_cast<const uint8_t*>(data), size);
    }

    void write(const uint8_t* data, size_t size)
    {
        if (fwrite(data, sizeof(uint8_t), size, m_file) != size)
            throw std::runtime_error("Failed to write " + std::to_string(size) + " bytes to file");
    }

    void read(uint8_t* into, size_t size)
    {
        if (fread(into, sizeof(uint8_t), size, m_file) != size)
            throw std::runtime_error("Failed to read " + std::to_string(size) + " bytes from file");
    }

    ~AutoFile()
    {
        if (m_file)
            fclose(m_file);
    }
};

inline std::pair<std::string, std::string> split_filename(const std::string& filename)
{
    std::pair<std::string, std::string> file_to_extension;

    auto file_offset = filename.find_last_of('/');
    if (file_offset == std::string::npos)
        file_offset = filename.find_last_of('\\');
    auto extension_offset = filename.find_last_of('.');

    bool has_extension = extension_offset != std::string::npos && extension_offset > file_offset;

    file_to_extension.first = filename.substr(file_offset + 1, extension_offset - file_offset - 1);

    if (has_extension)
        file_to_extension.second = filename.substr(extension_offset + 1);

    return file_to_extension;
}

inline std::vector<uint8_t> read_entire(const std::string& path)
{
    AutoFile f(path, "rb");

    std::vector<uint8_t> data(f.size());
    f.read(data.data(), data.size());

    return data;
}
