#pragma once

#include <cstddef>

struct DiskGeometry
{
    size_t total_sector_count;
    size_t cylinders;
    size_t heads;
    size_t sectors;

    bool within_chs_limit() const noexcept
    {
        if (heads > ((1 << 8) - 1))
            return false;
        if (sectors > ((1 << 6) - 1))
            return false;
        if (cylinders > ((1 << 10) - 1))
            return false;

        return true;
    }
};

struct CHS
{
    size_t cylinder;
    size_t head;
    size_t sector;
};

inline CHS to_chs(size_t lba, const DiskGeometry& geometry)
{
    CHS chs;

    chs.head = (lba / geometry.sectors) % geometry.heads;
    chs.cylinder = (lba / geometry.sectors) / geometry.heads;
    chs.sector = (lba % geometry.sectors) + 1;

    return chs;
}

#define KB  1024ull
#define MB (1024ull * KB)
#define GB (1024ull * MB)
#define TB (1024ull * GB)
