#include <iostream>

#include "Utility.h"
#include "FileSystems/FAT32.h"
#include "DiskImages/VMDKDiskImage.h"
#include "MBR.h"

int main(int argc, char** argv)
{
    ArgParser args;
    args.add_param("mbr", 'm', "Path to an MBR (Master Boot Record)", false)
        .add_param("vbr", 'v', "Path to a VBR (Volume Boot Record)", false)
        .add_list("files", 'f', "Paths to files to be put on disk")
        .add_param("size", 's', "Hard disk size to be generated (in megabytes)", false)
        .add_param("image-dir", 'i', "Path to a directory to output image files", false)
        .add_param("image-name", 'n', "Name of the image to be generated", false)
        .add_help("help", 'h', "Display this menu and exit",
                  [&]() { std::cout << "How to use VirtualHDDCreator:\n" << args; exit(1); });

    try {
        args.parse(argc, argv);

        auto image_size = args.get_uint("size") * MB;

        // align for sector size
        image_size = image_size - (image_size % DiskImage::sector_size);

        auto image_sector_count = image_size / DiskImage::sector_size;

        VMDKDiskImage image(args.get("image-dir"), args.get("image-name"), image_size);

        MBR mbr(args.get("mbr"), image.geometry(), DiskImage::partition_alignment);

        MBR::Partition partition_1(image_sector_count - DiskImage::partition_alignment);

        auto partition_offset = mbr.add_partition(partition_1);

        mbr.write_into(image);

        FAT32 fs(args.get("vbr"), partition_offset, partition_1.sector_count(), image.geometry());

        for (const auto& file : args.get_list("files"))
            fs.store_file(file);

        fs.write_into(image);

        image.finalize();
    }
    catch (const std::exception& ex)
    {
        std::cout << "ERROR: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
