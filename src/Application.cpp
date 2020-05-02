#include <iostream>

#include "VMDKDiskImage.h"
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
    }
    catch (const std::exception& ex)
    {
        std::cout << "ERROR: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
