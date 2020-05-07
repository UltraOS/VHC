#include "Utility.h"
#include "FloppyImage.h"

FloppyImage::FloppyImage(const std::string& path)
    : m_file(nullptr), m_bytes_written(0)
{
    m_file = fopen(path.c_str(), "wb");
    if (!m_file)
        throw std::runtime_error("Failed to create file - " + path);
}

void FloppyImage::write(uint8_t* buffer, size_t size)
{
    WRITE_EXACTLY(m_file, buffer, size);
    m_bytes_written += size;
}

void FloppyImage::finalize()
{
    if (m_bytes_written >= floppy_size)
        return;

    size_t to_pad = floppy_size - m_bytes_written;

    uint8_t* padding = new uint8_t[to_pad]();

    write(padding, to_pad);
}

FloppyImage::~FloppyImage()
{
    if (m_file)
        fclose(m_file);
}
