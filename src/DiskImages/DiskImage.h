#pragma once

#include <stdexcept>
#include <string>
#include <cstdint>

class DiskImage
{
public:
    static constexpr size_t sector_size = 512;

    virtual void write(uint8_t* data, size_t size) = 0;

    virtual void finalize() = 0;

    virtual ~DiskImage() = default;
};
