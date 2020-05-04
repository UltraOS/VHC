#include "DiskImages/DiskImage.h"

class FileSystem
{
public:
    virtual void write_into(DiskImage& image) = 0;

    virtual void store_file(const std::string& path) = 0;

    virtual ~FileSystem() = default;
};