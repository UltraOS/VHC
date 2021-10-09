#include <memory>
#include <stdexcept>
#include <string_view>

#include "FileSystem.h"
#include "FAT32/FAT32.h"

std::shared_ptr<FileSystem> FileSystem::create(DiskImage& image, size_t lba_offset, size_t sector_count, const ArgParser& args)
{
    auto raw = args.get_or("filesystem", "FAT32");
    auto type = extract_main_value(raw);
    auto options = parse_options(raw);

    if (type == "FAT32" || type == "fat32") {
        return std::make_shared<FAT::FAT32>(image, lba_offset, sector_count, options);
    }

    throw std::runtime_error("unknown filesystem type " + std::string(type));
}

FileSystem::FileSystem(DiskImage& image, size_t lba_offset, size_t sector_count)
    : m_image(image)
    , m_lba_offset(lba_offset)
    , m_sector_count(sector_count)
{
}
