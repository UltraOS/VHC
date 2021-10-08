#pragma once

#include <stdexcept>
#include <string>
#include <cstdint>

#include "Utilities/Common.h"
#include "Utilities/AutoFile.h"

class DiskImage
{
public:
    // this should technically be unique to a specific
    // disk image type, to do later
    static constexpr size_t sector_size = 512;
    static constexpr size_t partition_alignment = 8;

    DiskImage(const DiskGeometry&);

    static std::shared_ptr<DiskImage> create(std::string_view type, std::string_view out_directory, std::string_view out_name, size_t out_size);

    virtual void write_at(const void* data, size_t size, size_t offset) = 0;
    virtual void write(const void* data, size_t size) = 0;
    virtual void set_offset(size_t) = 0;
    virtual void skip(size_t) = 0;

    const DiskGeometry& geometry() { return m_geometry; }

    virtual void finalize() = 0;

    virtual ~DiskImage() = default;

private:
    DiskGeometry m_geometry;
};
