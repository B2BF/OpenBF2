# [CHOSEN, not recovered] Portable D3DX helper implementation and checks, issue #36.
cmake_minimum_required(VERSION 3.21)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(BF2LinuxD3DX LANGUAGES CXX)
    enable_testing()
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Linux D3DX helpers require a 64-bit Linux toolchain")
endif()
get_filename_component(BF2_D3DX_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../BF2" ABSOLUTE)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
find_package(ZLIB REQUIRED)
add_library(bf2_dxvk_d3dx STATIC
    "${BF2_D3DX_ROOT}/Platform/Gfx/Metal/D3DXMath.cpp"
    "${BF2_D3DX_ROOT}/Platform/Gfx/Metal/D3DXGeometry.cpp"
    "${BF2_D3DX_ROOT}/Platform/Gfx/Metal/D3DXPixels.cpp"
    "${BF2_D3DX_ROOT}/Platform/Gfx/Metal/D3DXImages.cpp"
    "${BF2_D3DX_ROOT}/Platform/Gfx/Metal/D3DXTextures.cpp"
    "${BF2_D3DX_ROOT}/Platform/Gfx/Metal/D3DXMesh.cpp"
    "${BF2_D3DX_ROOT}/Platform/Gfx/Metal/D3DXLine.cpp"
    "${BF2_D3DX_ROOT}/Platform/Gfx/D3D9Forward/Native/D3DXResources.cpp"
    "${BF2_D3DX_ROOT}/External/meshoptimizer/stripifier.cpp")
target_compile_definitions(bf2_dxvk_d3dx PRIVATE DEDICATED=0 BF2_IMAGE_RENDDX9=1 BF2_GFX_DXVK=1)
target_compile_options(bf2_dxvk_d3dx PRIVATE "-include${BF2_D3DX_ROOT}/Core/BuildConfig.h" -Wall -Wextra -Wno-unused-parameter
    $<$<CXX_COMPILER_ID:GNU>:-Wno-misleading-indentation>)
target_link_libraries(bf2_dxvk_d3dx PUBLIC JPEG::JPEG PNG::PNG ZLIB::ZLIB)
set_target_properties(bf2_dxvk_d3dx PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
if(TARGET bf2_fx_native)
    # The assembler exports a C++14-compatible interface; its own C++17
    # implementation requirements need not propagate into the shared helpers.
    target_link_libraries(bf2_dxvk_d3dx PUBLIC "$<LINK_ONLY:bf2_fx_native>")
endif()
add_executable(bf2_linux_d3dx_cpu ../macos_d3dx_helpers/main.cpp)
target_compile_definitions(bf2_linux_d3dx_cpu PRIVATE DEDICATED=0 BF2_IMAGE_RENDDX9=1 BF2_GFX_DXVK=1
    BF2_MENU_FIXTURES="${CMAKE_CURRENT_LIST_DIR}/../macos_d3dx_helpers/fixtures")
target_compile_options(bf2_linux_d3dx_cpu PRIVATE "-include${BF2_D3DX_ROOT}/Core/BuildConfig.h")
target_link_libraries(bf2_linux_d3dx_cpu PRIVATE bf2_dxvk_d3dx)
set_target_properties(bf2_linux_d3dx_cpu PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
add_test(NAME linux_d3dx_cpu COMMAND bf2_linux_d3dx_cpu)
add_executable(bf2_linux_d3dx_assembly assembly.cpp)
target_compile_definitions(bf2_linux_d3dx_assembly PRIVATE DEDICATED=0 BF2_IMAGE_RENDDX9=1 BF2_GFX_DXVK=1)
target_compile_options(bf2_linux_d3dx_assembly PRIVATE "-include${BF2_D3DX_ROOT}/Core/BuildConfig.h")
target_link_libraries(bf2_linux_d3dx_assembly PRIVATE bf2_dxvk_d3dx)
set_target_properties(bf2_linux_d3dx_assembly PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
add_test(NAME linux_d3dx_assembly COMMAND bf2_linux_d3dx_assembly)

option(BF2_LINUX_D3DX_DEVICE_CHECK "Build the manually run SDL/DXVK D3DX helper check" OFF)
if(BF2_LINUX_D3DX_DEVICE_CHECK)
    find_package(SDL3 3.2 REQUIRED CONFIG)
    if(NOT TARGET bf2_dxvk_forward)
        add_subdirectory(../linux_vulkan "${CMAKE_CURRENT_BINARY_DIR}/vulkan")
    endif()
    add_executable(bf2_linux_d3dx_device device.cpp)
    if(NOT TARGET bf2_fx_native)
        target_sources(bf2_linux_d3dx_device PRIVATE "${BF2_D3DX_ROOT}/Platform/Gfx/Effect/D3DAsm.cpp")
    endif()
    target_compile_definitions(bf2_linux_d3dx_device PRIVATE DEDICATED=0 BF2_IMAGE_RENDDX9=1 BF2_GFX_DXVK=1)
    target_compile_options(bf2_linux_d3dx_device PRIVATE "-include${BF2_D3DX_ROOT}/Core/BuildConfig.h" -Wall -Wextra)
    target_link_libraries(bf2_linux_d3dx_device PRIVATE bf2_dxvk_d3dx bf2_dxvk_forward SDL3::SDL3)
    set_target_properties(bf2_linux_d3dx_device PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
    get_filename_component(_d3dx_dxvk_libdir "${BF2_DXVK_LIBRARY}" DIRECTORY)
    set_target_properties(bf2_linux_d3dx_device PROPERTIES BUILD_RPATH "${_d3dx_dxvk_libdir}")
endif()
