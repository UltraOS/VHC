#pragma once

#include "DiskImage.h"

class FloppyImage final : public DiskImage
{
private:
    static constexpr size_t floppy_size = 1474560;

    AutoFile m_floppy_file;
    size_t m_bytes_written;
public:
    FloppyImage(const std::string& path);

    void write(uint8_t* data, size_t size) override;

    void finalize() override;
};
