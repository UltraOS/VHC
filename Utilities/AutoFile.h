#pragma once

#include <string_view>

class AutoFile
{
public:
    enum Mode {
        READ,
        WRITE,
        TRUNCATE
    };

    AutoFile()
        : m_platform_handle(nullptr)
    {
    }

    AutoFile(std::string_view path, Mode mode)
        : m_platform_handle(nullptr)
    {

    }

    AutoFile(const AutoFile& other_file) = delete;
    AutoFile& operator=(const AutoFile& other_file) = delete;

    AutoFile(AutoFile&& other_file) noexcept
        : m_platform_handle(other_file.m_platform_handle)
    {
        other_file.m_platform_handle = nullptr;
    }

    AutoFile& operator=(AutoFile&& other_file) noexcept
    {
        std::swap(m_platform_handle, other_file.m_platform_handle);

        return *this;
    }

    void open(std::string_view path, Mode mode);

    size_t size();

    void write(std::string_view data)
    {
        write(data.data(), data.size());
    }

    void write(const char* data, size_t size)
    {
        write(reinterpret_cast<const uint8_t*>(data), size);
    }

    void write(const uint8_t* data, size_t size);
    void read(uint8_t* into, size_t size);

    size_t set_offset(size_t offset);
    size_t skip(size_t bytes);
    void set_size(size_t new_size);

    ~AutoFile();

private:
    void* m_platform_handle;
};
