#include <stdexcept>

#include <unistd.h>

#include "Utilities/AutoFile.h"

static int to_fd(void* handle) { return reinterpret_cast<int>(handle); }

void AutoFile::open(std::string_view path, Mode mode)
{
    int flags = 0;
    flags |= O_CREAT;
    flags |= (mode & Mode::TRUNCATE) ? O_TRUNC : 0;

    mode_t mode = 0;
    if ((mode & Mode::READ) && (mode & Mode::WRITE))
        mode = O_RDWR;
    else if (mode & Mode::READ)
        mode = O_RDONLY;
    else if (mode & Mode::WRITE)
        mode = O_WRONLY;

    m_platform_handle = reinterpret_cast<void*>(open(path.data(), flags, mode));

    if (to_fd(m_platform_handle) < 0)
        throw std::runtime_error("failed to open " + std::string(path));
}

void AutoFile::size()
{
    stat st;
    if (fstat(to_fd(m_platform_handle), &st) < 0)
        throw std::runtime_error("failed to get file size");

    return st.st_size;
}

void AutoFile::write(const uint8_t* data, size_t size)
{
    auto res = ::write(to_fd(m_platform_handle), data, size);

    if (res != size)
        throw std::runtime_error("failed to write all bytes to file");
}

void AutoFile::read(uint8_t* into, size_t size)
{
    auto res = ::read(to_fd(m_platform_handle), into, size);

    if (res != size)
        throw std::runtime_error("failed to read all bytes from file");
}

size_t AutoFile::set_offset(size_t offset)
{
    auto current_offset = lseek(to_fd(m_platform_handle), 0, SEEK_CUR);

    if (lseek(to_fd(m_platform_handle), offset, SEEK_SET) < 0)
        throw std::runtime_error("failed to set offset");

    return current_offset;
}

size_t AutoFile::skip(size_t bytes)
{
    auto current_offset = lseek(to_fd(m_platform_handle), bytes, SEEK_CUR);

    if (current_offset < 0)
        throw std::runtime_error("failed to set offset");

    return current_offset;
}


void AutoFile::set_size(size_t new_size)
{
    if (ftruncate(to_fd(m_platform_handle), offset) < 0)
        throw std::runtime_error("failed to set file size");
}

AutoFile::~AutoFile()
{
    if (to_fd(m_platform_handle))
        close(to_fd(m_platform_handle));
}
