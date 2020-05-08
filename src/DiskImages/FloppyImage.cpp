#include "Utility.h"
#include "FloppyImage.h"

FloppyImage::FloppyImage(const std::string& path)
    : m_floppy_file(path, "wb"), m_bytes_written(0)
{
}

void FloppyImage::write(uint8_t* data, size_t size)
{
    m_floppy_file.write(data, size);
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
