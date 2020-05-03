#include <iostream>

#include "VMDKDiskImage.h"
#include "MBR.h"
#include "Utility.h"

int main(int argc, char** argv)
{
    ArgParser args;
    args.add_param("mbr", 'm', "Path to an MBR (Master Boot Record)", false)
        .add_param("vbr", 'v', "Path to a VBR (Volume Boot Record)", false)
        .add_list("files", 'f', "Paths to files to be put on disk")
        .add_param("size", 's', "Hard drive size to be generated (in megabytes)", false)
        .add_param("image-dir", 'i', "Path to a directory to output image files", false)
        .add_param("image-name", 'n', "Name of the image to be generated", false)
        .add_help("help", 'h', "Display this menu and exit",
                  [&]() { std::cout << "How to use VirtualHDDCreator:\n" << args; exit(1); });

    try {
        args.parse(argc, argv);

        auto image_size = args.get_uint("size");

        // align for sector size
        image_size = image_size - (image_size % DiskImage::sector_size);

        auto image_sector_count = image_size / DiskImage::sector_size;

        VMDKDiskImage image(args.get("image-dir"), args.get("image-name"), image_size);

        MBR mbr(args.get("mbr"), image.geometry());

        // This partition takes the entire disk space - 1
        // the -1 being the MBR size
        MBR::Partition partition_1(image_sector_count - 1);

        mbr.add_partition(partition_1);

        mbr.write_into(image);

        // TODO:
        // FAT32 data(lba_offset, lba_size, vbr)
        // data.add_file(...)
        // data.write_into(image)

        image.finalize();
    }
    catch (const std::exception & ex)
    {
        std::cout << "ERROR: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
