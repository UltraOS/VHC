cmake_minimum_required(VERSION 3.6)
project (VHC)
file(GLOB_RECURSE VHC_SOURCES "src/*.h" "src/*.cpp")
add_executable(VHC ${VHC_SOURCES})
include_directories(src)
set_target_properties(VHC PROPERTIES OUTPUT_NAME "vhc")

# A bunch of MSVC related stuff because it's annyoning
if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT VHC)