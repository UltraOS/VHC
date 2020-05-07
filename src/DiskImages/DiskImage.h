#pragma once

#include <stdexcept>
#include <string>
#include <cstdint>

class DiskImage
{
public:
    // this should technically be unique to a specific
    // disk image type, to do later
    static constexpr size_t sector_size = 512;

    virtual void write(uint8_t* data, size_t size) = 0;

    virtual void finalize() = 0;

    virtual ~DiskImage() = default;
};
