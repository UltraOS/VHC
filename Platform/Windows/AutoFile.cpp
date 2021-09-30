#include <stdexcept>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "Utilities/AutoFile.h"

void AutoFile::open(std::string_view path, Mode mode)
{
    if (m_platform_handle)
        CloseHandle(m_platform_handle);

    DWORD access = 0;
    access |= (mode & Mode::READ) ? GENERIC_READ : 0;
    access |= (mode & Mode::WRITE) ? GENERIC_WRITE : 0;

    DWORD disposition = 0;
    disposition |= OPEN_ALWAYS;
    disposition |= (mode & Mode::TRUNCATE) ? TRUNCATE_EXISTING : 0;

    m_platform_handle = CreateFileA(path.data(), access, 0, NULL, disposition, FILE_ATTRIBUTE_NORMAL, NULL);

    if (m_platform_handle == INVALID_HANDLE_VALUE)
        throw std::runtime_error("failed to open " + std::string(path));
}

size_t AutoFile::size()
{
    DWORD upper = 0;
    DWORD lower = GetFileSize(m_platform_handle, &upper);

    if (lower == INVALID_FILE_SIZE)
        throw std::runtime_error("failed to get file size");

    // Assert for safety
    static_assert(sizeof(size_t) == 8);

    return lower | static_cast<uint64_t>(upper) << 32;
}

void AutoFile::write(const uint8_t* data, size_t size)
{
    DWORD bytes_written = 0;

    if (!WriteFile(m_platform_handle, data, size, &bytes_written, NULL))
        throw std::runtime_error("failed to write file");

    if (bytes_written != size)
        throw std::runtime_error("failed to write all bytes to file");
}

void AutoFile::read(uint8_t* data, size_t size)
{
    DWORD bytes_read = 0;

    if (!ReadFile(m_platform_handle, data, size, &bytes_read, NULL))
        throw std::runtime_error("failed to read file");

    if (bytes_read != size)
        throw std::runtime_error("failed to read all bytes from file");
}

size_t AutoFile::set_offset(size_t offset)
{
    size_t current_offset = SetFilePointer(m_platform_handle, 0, NULL, FILE_CURRENT);

    LONG upper_offset = offset >> 32;
    if (SetFilePointer(m_platform_handle, offset, &upper_offset, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
        throw std::runtime_error("couldn't set file offset");

    return current_offset;
}

size_t AutoFile::skip(size_t bytes)
{
    LONG upper_bytes = bytes >> 32;
    bytes = SetFilePointer(m_platform_handle, bytes, &upper_bytes, FILE_CURRENT);

    if (bytes == INVALID_SET_FILE_POINTER)
        throw std::runtime_error("couldn't set file offset");

    return bytes;
}

void AutoFile::set_size(size_t new_size)
{
    auto old_size = set_offset(new_size);

    if (!SetEndOfFile(m_platform_handle))
        throw std::runtime_error("couldn't set end of file");

    set_offset(old_size);
}

AutoFile::~AutoFile()
{
    if (m_platform_handle)
        CloseHandle(m_platform_handle);
}
