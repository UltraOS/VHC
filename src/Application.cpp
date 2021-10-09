#include <iostream>
#include <filesystem>

#include "Utilities/Common.h"
#include "FileSystems/FAT32/FAT32.h"
#include "DiskImages/VMDKDiskImage.h"
#include "MBR.h"

int main(int argc, char** argv)
{
    ArgParser args;
    args.add_param("mbr", 'm', "Path to an MBR (Master Boot Record)", false)
        .add_param("filesystem", 'x', "Filesystem to use followed by <,option=value>")
        .add_list("files", 'f', "Paths to additional files to be put inside root directory")
        .add_list("store", 't', "List of <file>,<sector> to store outside of the filesystem")
        .add_param("directory", 'd', "Path to the root directory for this disk (copied recursively)")
        .add_param("size", 's', "Hard disk size to be generated (in megabytes)")
        .add_param("image-format", 'g', "generated image format, currently only valid is vmdk")
        .add_param("image-directory", 'i', "Path to a directory to output image files")
        .add_param("image-name", 'n', "Name of the image to be generated")
        .add_param("part-align", 'p', "Partition alignment (in 512 byte sectors)")
        .add_flag("verbose", 'v', "Enable verbose logging")
        .add_help("help", 'h', "Display this menu and exit",
                  [&]() { std::cout << "How to use VirtualHDDCreator:\n" << args; exit(1); });

    try {
        args.parse(argc, argv);

        if (args.is_set("verbose"))
            Logger::the().set_level(Logger::Level::INFO);

        auto image_size = args.get_uint_or("size", 64) * MB;

         // align for sector size
        image_size = image_size - (image_size % DiskImage::sector_size);
        auto image_sector_count = image_size / DiskImage::sector_size;

        auto image_dir = std::filesystem::current_path().string();
        if (args.is_set("image-directory"))
            image_dir = args.get("image-directory");
        auto image_name = args.get_or("image-name", "MyHDD");

        auto image_format = args.get_or("image-format", "vmdk");
        auto image = DiskImage::create(image_format, image_dir, image_name, image_size);

        auto partition_alignment = args.get_uint_or("part-align", DiskImage::partition_alignment);
        MBR mbr(args.get("mbr"), image->geometry(), partition_alignment);

        MBR::Partition partition_1(image_sector_count - partition_alignment);
        auto partition_offset = mbr.add_partition(partition_1);
        mbr.write_into(*image);

        auto fs = FileSystem::create(*image, partition_offset, partition_1.sector_count(), args);

        FSObject obj {};

        auto path_rooted_at = [](const std::string& root, const std::string& sub_directory_path) -> std::string
        {
            auto rooted_path = std::filesystem::path("/");
            rooted_path = rooted_path / std::filesystem::path(std::string(sub_directory_path.data() + root.size()));

            return rooted_path.string();
        };

        auto directory = args.get_or("directory", "");
        if (!directory.empty()) {
            auto path_to_root = std::filesystem::path(directory);

            for (auto& file : std::filesystem::recursive_directory_iterator(path_to_root)) {
                auto path_on_disk_image = path_rooted_at(directory, file.path().string());
                obj.path = path_on_disk_image;

                Logger::the().info("storing file ", file.path().string());

                if (file.is_directory()) {
                    obj.type = FSObject::Type::DIRECTORY;
                    fs->store(obj);
                    continue;
                }

                if (file.is_regular_file()) {
                    obj.type = FSObject::Type::FILE;
                    obj.data = read_entire(file.path().string());
                    fs->store(obj);
                    continue;
                }

                Logger::the().warning("Not going to store unknown file type at ", file.path().string());
            }
        }

        obj.type = FSObject::Type::FILE;

        for (const auto& file : args.get_list_or("files", {})) {
            auto path_on_disk = "/" + std::filesystem::path(file).filename().string();
            obj.path = path_on_disk;

            Logger::the().info("storing file ", file);

            obj.data = read_entire(file);
            fs->store(obj);
        }

        for (auto& arg : args.get_list_or("store", {})) {
            auto comma = arg.find(',');

            if (comma == std::string::npos)
                throw std::runtime_error("invalid store argument format: " + arg);

            std::string_view file_path(arg.data(), comma);

            auto sector = atoll(arg.data() + comma + 1);
            if (sector <= 0 || sector >= image->geometry().total_sector_count)
                throw std::runtime_error("invalid sector value " + std::to_string(sector));

            auto entire_file = read_entire(file_path);
            image->write_at(entire_file.data(), entire_file.size(), sector * DiskImage::sector_size);
        }
    }
    catch (const std::exception& ex)
    {
        Logger::the().error(ex.what());
        return 1;
    }

    return 0;
}
