# [TEMP] [CHOSEN, not recovered] Native runtime-image regression for issue #36.
cmake_minimum_required(VERSION 3.21)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(BF2LinuxSwiffCheck LANGUAGES CXX)
    enable_testing()
    set(BF2_SWIFF_BUILD_DIR "" CACHE PATH "Completed Linux client build containing production archives")
    foreach(library bf2_swiffplayer_static bf2_plugin_kit_static bf2_expat_static)
        add_library(${library} STATIC IMPORTED)
        set_target_properties(${library} PROPERTIES
            IMPORTED_LOCATION "${BF2_SWIFF_BUILD_DIR}/lib${library}.a")
    endforeach()
endif()
get_filename_component(BF2_SWIFF_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../BF2" ABSOLUTE)
find_package(ZLIB REQUIRED)
add_executable(bf2_linux_swiff_image_check runtime_image.cpp)
target_compile_definitions(bf2_linux_swiff_image_check PRIVATE
    DEDICATED=0 BF2_LINUX_CLIENT=1 BF2_NATIVE_X87=1 BF2_PLATFORM_POSIX _LINUX NDEBUG)
target_compile_options(bf2_linux_swiff_image_check PRIVATE
    "-include${BF2_SWIFF_ROOT}/Core/BuildConfig.h"
    "-include${BF2_SWIFF_ROOT}/Core/PlatformCompat.h" -fno-strict-aliasing
    $<$<CXX_COMPILER_ID:GNU>:-fno-devirtualize;-fno-devirtualize-speculatively>)
target_include_directories(bf2_linux_swiff_image_check PRIVATE "${BF2_SWIFF_ROOT}/External")
target_link_libraries(bf2_linux_swiff_image_check PRIVATE
    "-Wl,--start-group" bf2_swiffplayer_static bf2_plugin_kit_static bf2_expat_static "-Wl,--end-group" ZLIB::ZLIB)
set_target_properties(bf2_linux_swiff_image_check PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES)
add_test(NAME linux_swiff_runtime_image COMMAND bf2_linux_swiff_image_check)
