#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <functional>
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <memory>

class ArgParser
{
public:
    using help_callback = std::function<void()>;

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

        const ArgSpec* active_spec = nullptr;

        for (auto arg_index = 1; arg_index < argc; ++arg_index)
        {
            const char* current_arg = argv[arg_index];

            bool is_new_arg = is_arg(current_arg);

            if (active_spec && is_new_arg)
            {
                if ((active_spec->is_param() || active_spec->is_list()) && m_parsed_args[active_spec->as_full].empty())
                    throw std::runtime_error("expected an argument for " + active_spec->as_full);
            }

            if (active_spec && !is_new_arg)
            {
                if (active_spec->is_flag())
                    throw std::runtime_error(std::string("unexpected argument ") + current_arg);
                else if (active_spec->is_param() && m_parsed_args[active_spec->as_full].size() == 1)
                    throw std::runtime_error("too many arguments for " + active_spec->as_full);

                m_parsed_args[active_spec->as_full].emplace_back(current_arg);
                continue;
            }

            std::string as_full_arg = extract_full_arg(current_arg);
            if (as_full_arg.empty())
                throw std::runtime_error(std::string("unexpected argument ") + current_arg);
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

    const std::vector<std::string>& get_list(std::string_view arg) const
    {
        ensure_arg_is_parsed(arg);

        return m_parsed_args.at(std::string(arg));
    }

    const std::vector<std::string>& get_list_or(std::string_view arg, const std::vector<std::string>& default_value) const
    {
        if (is_arg_parsed(arg))
            return get_list(arg);

        return default_value;
    }

    const std::string& get(std::string_view arg) const
    {
        return get_list(arg)[0];
    }

    const std::string& get_or(std::string_view arg, const std::string& default_value) const
    {
        if (is_arg_parsed(arg))
            return get(arg);

        return default_value;
    }

    uint64_t get_uint(std::string_view arg) const
    {
        return std::stoull(get(arg));
    }

    uint64_t get_uint_or(std::string_view arg, uint64_t default_value) const
    {
        if (is_arg_parsed(arg))
            return get_uint(arg);

        return default_value;
    }

    int64_t get_int(std::string_view arg) const
    {
        return std::stoll(get(arg));
    }

    int64_t get_int_or(std::string_view arg, int64_t default_value) const
    {
        if (is_arg_parsed(arg))
            return get_int(arg);

        return default_value;
    }

    bool is_set(std::string_view arg) const
    {
        return is_arg_parsed(arg);
    }

    const std::vector<std::string>& get_list(char arg) const
    {
        const auto& arg_spec = arg_spec_of(arg);

        return get_list(arg_spec.as_full);
    }

    std::string_view get(char arg) const
    {
        const auto& arg_spec = arg_spec_of(arg);

        return get(arg_spec.as_full);
    }

    uint64_t get_uint(char arg) const
    {
        const auto& arg_spec = arg_spec_of(arg);

        return get_uint(arg_spec.as_full);
    }

    uint64_t get_int(char arg) const
    {
        const auto& arg_spec = arg_spec_of(arg);

        return get_int(arg_spec.as_full);
    }

    bool is_set(char arg) const
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

        [[nodiscard]] bool is_list()  const { return type == ArgType::list; }
        [[nodiscard]] bool is_param() const { return type == ArgType::param; }
        [[nodiscard]] bool is_flag()  const { return type == ArgType::flag; }
        [[nodiscard]] bool is_help()  const { return type == ArgType::help; }
    };

    void ensure_mandatory_args_are_satisfied() const
    {
        for (const auto& arg : m_args)
        {
            if (arg.is_optional)
                continue;

            if (!m_parsed_args.count(arg.as_full))
                throw std::runtime_error("expected a non-optional argument --" + arg.as_full);
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

    bool is_arg_parsed(std::string_view arg)  const
    {
        return m_parsed_args.count(std::string(arg));
    }

    void ensure_arg_is_parsed(std::string_view arg)  const
    {
        if (!is_arg_parsed(arg))
            throw std::runtime_error("couldn't find argument " + std::string(arg));
    }

    const ArgSpec& arg_spec_of(std::string_view arg) const
    {
        auto arg_itr = std::find_if(m_args.begin(), m_args.end(),
                                    [&](const ArgSpec& my_arg) {
                                       return my_arg.as_full == arg;
                                    });

        if (arg_itr == m_args.end())
            throw std::runtime_error("unknown argument " + std::string(arg));

        return *arg_itr;
    }

    const ArgSpec& arg_spec_of(char arg) const
    {
        auto arg_itr = std::find_if(m_args.begin(), m_args.end(),
                                    [&](const ArgSpec& my_arg) {
                                        return my_arg.as_short == arg;
                                    });

        if (arg_itr == m_args.end())
            throw std::runtime_error(std::string("unknown argument ") + arg);

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

private:
    help_callback m_help_callback;
    std::vector<ArgSpec> m_args;
    std::unordered_map<std::string, std::vector<std::string>> m_parsed_args;
};

inline std::string extract_main_value(std::string_view value)
{
    auto offset = value.find_first_of(",");

    return std::string(value.begin(), offset == std::string::npos ? value.end() : (value.begin() + offset));
}

inline bool interpret_boolean(std::string_view value)
{
    if (value == "yes" || value == "y" || value == "YES" || value == "Y" ||
        value == "true" || value == "TRUE" || value == "on" || value == "ON")
        return true;

    if (value == "no" || value == "n" || value == "NO" || value == "N" ||
        value == "false" || value == "FALSE" || value == "off" || value == "OFF")
        return false;

    throw std::runtime_error("couldn't interpret " + std::string(value) + " as boolean");
}

using additional_options_t = std::unordered_map<std::string, std::string>;

inline additional_options_t parse_options(std::string_view option)
{
    additional_options_t additional_options;
    size_t comma_offset = option.find_first_of(",");

    for (;;) {
        if (comma_offset == std::string::npos)
            break;

        auto* begin = option.data() + comma_offset + 1;
        size_t length = 0;

        size_t next_comma_offset = option.find_first_of(",", comma_offset + 1);
        if (next_comma_offset == std::string::npos) {
            length = option.size() - comma_offset - 1;
        } else {
            length = next_comma_offset - comma_offset - 1;
        }

        comma_offset = next_comma_offset;

        std::string_view option_view = { begin, length };
        auto equality_offset = option_view.find_first_of("=");

        if (equality_offset == std::string::npos || equality_offset == 0)
            throw std::runtime_error("malformed option value " + std::string(option));

        std::string key = std::string(option_view.begin(), option_view.begin() + equality_offset);
        std::string value;

        if (option_view[equality_offset + 1] == '"') {
            if (option_view.back() != '"')
                throw std::runtime_error("expected \" at the end of option, found " + std::string(option_view.end() - 1, option_view.end()));

            value = std::string(option_view.begin() + equality_offset + 2, option_view.end() - 1);
        } else {
            value = std::string(option_view.begin() + equality_offset + 1, option_view.end());
        }

        additional_options.emplace(std::move(key), std::move(value));
    }

    return additional_options;
}
