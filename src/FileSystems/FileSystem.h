#pragma once

#include <memory>
#include <string_view>

#include "Utilities/Common.h"
#include "DiskImages/DiskImage.h"

struct FSObject {
    enum Type {
        INVALID,
        FILE,
        DIRECTORY
    } type { INVALID };

    // path where to store the file on the filesystem
    std::string path;

    // in-memory file data to store
    // empty for directories and empty files
    std::vector<uint8_t> data;
};

class FileSystem
{
public:
    static std::shared_ptr<FileSystem> create(DiskImage&, size_t lba_offset, size_t sector_count, const ArgParser& args);

    FileSystem(DiskImage&, size_t lba_offset, size_t sector_count);

    virtual void store(const FSObject&) = 0;
    virtual void finalize() = 0;

    [[nodiscard]] size_t lba_offset() const { return m_lba_offset; }
    [[nodiscard]] size_t sector_count() const { return m_sector_count; }
    [[nodiscard]] DiskImage& image() const { return m_image; }

    virtual ~FileSystem() = default;

private:
    DiskImage& m_image;

    size_t m_lba_offset { 0 };
    size_t m_sector_count { 0 };
};
