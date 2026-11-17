# [TEMP] [CHOSEN, not recovered] Linux disk-path and ZIP-open boundary checks, issue #36.
cmake_minimum_required(VERSION 3.21)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(BF2LinuxPaths LANGUAGES C CXX)
    enable_testing()
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Linux pathname checks require a Linux toolchain")
endif()
get_filename_component(BF2_PATH_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../BF2" ABSOLUTE)
set(BF2_PATH_TEST_ZLIB "${BF2_PATH_TEST_ROOT}/External/zlib-1.2.3")
find_package(ZLIB REQUIRED)
add_executable(bf2_linux_path_check main.cpp
    "${BF2_PATH_TEST_ROOT}/IO/File/ZipArchive.cpp"
    "${BF2_PATH_TEST_ROOT}/Core/StringUtil.cpp"
    "${BF2_PATH_TEST_ZLIB}/contrib/minizip/unzip.c"
    "${BF2_PATH_TEST_ZLIB}/contrib/minizip/ioapi.c")
target_compile_definitions(bf2_linux_path_check PRIVATE DEDICATED=0 BF2_LINUX_CLIENT=1 BF2_PLATFORM_POSIX=1)
target_compile_options(bf2_linux_path_check PRIVATE -ffunction-sections -fdata-sections
    "-include${BF2_PATH_TEST_ROOT}/Core/BuildConfig.h"
    "$<$<COMPILE_LANGUAGE:CXX>:-include${BF2_PATH_TEST_ROOT}/Core/PlatformCompat.h>")
target_include_directories(bf2_linux_path_check PRIVATE
    "${BF2_PATH_TEST_ZLIB}" "${BF2_PATH_TEST_ZLIB}/contrib/minizip")
target_link_libraries(bf2_linux_path_check PRIVATE ZLIB::ZLIB)
# The fixture invokes the production FileHandle boundary, not unused ZipArchive factories or
# stream vtables. Dead sections are discarded; no unresolved-symbol override or fake body is used.
target_link_options(bf2_linux_path_check PRIVATE -Wl,--gc-sections)
set_target_properties(bf2_linux_path_check PROPERTIES
    CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF
    C_STANDARD 99 C_STANDARD_REQUIRED YES)

# Synthetic content: retain the Windows filename casing that triggered the startup failure.
set(BF2_PATH_TEST_FIXTURE "${CMAKE_CURRENT_BINARY_DIR}/fixture/Mods/Bf2")
file(MAKE_DIRECTORY "${BF2_PATH_TEST_FIXTURE}")
file(WRITE "${BF2_PATH_TEST_FIXTURE}/StaticMesh.fx" "// Synthetic path fixture; not a retail shader.\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cf Shaders_client.zip --format=zip StaticMesh.fx
    WORKING_DIRECTORY "${BF2_PATH_TEST_FIXTURE}" RESULT_VARIABLE BF2_PATH_TEST_ZIP_RESULT)
if(NOT BF2_PATH_TEST_ZIP_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not create synthetic ZIP pathname fixture")
endif()
add_test(NAME linux_asset_paths COMMAND bf2_linux_path_check
    "${CMAKE_CURRENT_BINARY_DIR}/scratch"
    "${CMAKE_CURRENT_BINARY_DIR}/fixture/mods/bf2/shaders_client.zip")
