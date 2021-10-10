cmake_minimum_required(VERSION 3.6)

set(CMAKE_CXX_STANDARD 17)

project(VHC)

if (WIN32)
    set(PLATFORM_DIRECTORY "Windows")
elseif (UNIX AND NOT APPLE)
    set(PLATFORM_DIRECTORY "GenericUnix")
else ()
    message(FATAL_ERROR "Unknown platform")
endif ()

set(SRC_DIRECTORY "${PROJECT_SOURCE_DIR}/src")

FILE(GLOB VHC_SOURCES "${SRC_DIRECTORY}/*cpp"                                 "${SRC_DIRECTORY}/*h"
                      "${SRC_DIRECTORY}/Utilities/*cpp"                       "${SRC_DIRECTORY}/Utilities/*h"
                      "${SRC_DIRECTORY}/DiskImages/*cpp"                      "${SRC_DIRECTORY}/DiskImages/*h"
                      "${SRC_DIRECTORY}/FileSystems/*cpp"                     "${SRC_DIRECTORY}/FileSystems/*h"
                      "${SRC_DIRECTORY}/FileSystems/FAT32/*cpp"               "${SRC_DIRECTORY}/FileSystems/FAT32/*h"
                      "${SRC_DIRECTORY}/Platform/${PLATFORM_DIRECTORY}/*cpp"  "${SRC_DIRECTORY}/Platform/${PLATFORM_DIRECTORY}/*h")

add_executable(VHC ${VHC_SOURCES})
include_directories(src)
set_target_properties(VHC PROPERTIES OUTPUT_NAME "vhc")

# A bunch of MSVC related stuff because it's annoying
if (MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT VHC)
endif ()
