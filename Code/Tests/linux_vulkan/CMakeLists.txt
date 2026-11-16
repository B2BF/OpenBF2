# [CHOSEN, not recovered] Native Linux graphics boundary, issue #36.
# Also usable as an isolated configure while client integration is in progress.
cmake_minimum_required(VERSION 3.21)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(BF2LinuxVulkan LANGUAGES CXX)
    enable_testing()
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The Linux Vulkan boundary requires a 64-bit Linux toolchain")
endif()
get_filename_component(BF2_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(BF2_GFX "${BF2_SOURCE_ROOT}/Code/BF2/Platform/Gfx")
set(BF2_DXVK_ROOT "" CACHE PATH "DXVK Native prefix containing include/dxvk and lib")
find_path(BF2_DXVK_INCLUDE d3d9.h HINTS "${BF2_DXVK_ROOT}/include/dxvk" REQUIRED NO_DEFAULT_PATH)
find_library(BF2_DXVK_LIBRARY NAMES dxvk_d3d9 HINTS "${BF2_DXVK_ROOT}/lib" "${BF2_DXVK_ROOT}/lib64"
    REQUIRED NO_DEFAULT_PATH)
find_package(SDL3 3.2 REQUIRED CONFIG)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_INCLUDES "${BF2_DXVK_INCLUDE}")
set(CMAKE_REQUIRED_LIBRARIES "${BF2_DXVK_LIBRARY}")
unset(BF2_DXVK_ARCH_MATCHES CACHE)
check_cxx_source_compiles("#include <d3d9.h>\nint main() { return Direct3DCreate9(D3D_SDK_VERSION) ? 0 : 1; }"
    BF2_DXVK_ARCH_MATCHES)
unset(CMAKE_REQUIRED_INCLUDES)
unset(CMAKE_REQUIRED_LIBRARIES)
if(NOT BF2_DXVK_ARCH_MATCHES)
    message(FATAL_ERROR "DXVK Native must link with this Linux toolchain. Supply an architecture-matched prefix; x86-64 releases cannot be used for ARM64.")
endif()

add_library(bf2_dxvk_forward STATIC
    "${BF2_GFX}/D3D9Forward/GeneratedNative/ForwardWrappers.cpp"
    "${BF2_GFX}/D3D9Forward/GeneratedNative/ForwardFunctions.cpp"
    "${BF2_GFX}/D3D9Forward/Native/Loader.cpp")
target_include_directories(bf2_dxvk_forward PRIVATE
    "${BF2_GFX}/D3D9Forward/Native" "${BF2_GFX}/D3D9Forward/GeneratedNative" "${BF2_DXVK_INCLUDE}")
target_compile_features(bf2_dxvk_forward PRIVATE cxx_std_14)
target_compile_definitions(bf2_dxvk_forward PRIVATE DEDICATED=0 BF2_IMAGE_RENDDX9=1 BF2_GFX_FORWARD_TU=1 BF2_GFX_DXVK=1)
target_compile_options(bf2_dxvk_forward PRIVATE
    "-include${BF2_SOURCE_ROOT}/Code/BF2/Core/BuildConfig.h" -fno-strict-aliasing -Wall -Wextra)
target_link_libraries(bf2_dxvk_forward PUBLIC ${CMAKE_DL_LIBS})

add_executable(bf2_linux_presentation presentation.cpp)
target_compile_features(bf2_linux_presentation PRIVATE cxx_std_14)
target_compile_definitions(bf2_linux_presentation PRIVATE DEDICATED=0)
target_compile_options(bf2_linux_presentation PRIVATE
    "-include${BF2_SOURCE_ROOT}/Code/BF2/Core/BuildConfig.h" -Wall -Wextra -Werror)
target_link_libraries(bf2_linux_presentation PRIVATE bf2_dxvk_forward SDL3::SDL3)
get_filename_component(BF2_DXVK_LIBDIR "${BF2_DXVK_LIBRARY}" DIRECTORY)
set_target_properties(bf2_linux_presentation PROPERTIES BUILD_RPATH "${BF2_DXVK_LIBDIR}")
add_test(NAME linux_dxvk_generated_current COMMAND "${Python3_EXECUTABLE}"
    "${BF2_SOURCE_ROOT}/tools/macos/gen_d3d9_forward.py" --d3d9-only
    --out-dir "${BF2_GFX}/D3D9Forward/GeneratedNative" --check)

add_executable(bf2_linux_forward_contract forward_contract.cpp)
target_compile_features(bf2_linux_forward_contract PRIVATE cxx_std_14)
target_include_directories(bf2_linux_forward_contract PRIVATE
    "${BF2_GFX}/D3D9Forward/Native" "${BF2_GFX}/D3D9Forward/GeneratedNative" "${BF2_DXVK_INCLUDE}")
target_compile_definitions(bf2_linux_forward_contract PRIVATE DEDICATED=0 BF2_GFX_DXVK=1)
target_compile_options(bf2_linux_forward_contract PRIVATE
    "-include${BF2_SOURCE_ROOT}/Code/BF2/Core/BuildConfig.h" -Wall -Wextra -Werror)
target_link_libraries(bf2_linux_forward_contract PRIVATE bf2_dxvk_forward)
add_test(NAME linux_dxvk_forward_contract COMMAND bf2_linux_forward_contract)
set_target_properties(bf2_dxvk_forward bf2_linux_presentation bf2_linux_forward_contract
    PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)

option(BF2_LINUX_CAPTURE_DEVICE_CHECK "Build the manually run opaque display capture regression" OFF)
if(BF2_LINUX_CAPTURE_DEVICE_CHECK)
    find_package(PNG REQUIRED)
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND NOT TARGET bf2_dxvk_d3dx)
        add_subdirectory(../linux_d3dx "${CMAKE_CURRENT_BINARY_DIR}/d3dx")
    endif()
    add_executable(bf2_linux_capture capture.cpp
        "${BF2_SOURCE_ROOT}/Code/BF2/Platform/Linux/FrameCapture.cpp"
        "${BF2_SOURCE_ROOT}/Code/BF2/Platform/InputScript.cpp"
        "${BF2_SOURCE_ROOT}/Code/BF2/Platform/Linux/SDLInputScript.cpp"
        "${BF2_SOURCE_ROOT}/Code/BF2/Platform/macOS/MacKeyMap.cpp"
        "${BF2_SOURCE_ROOT}/Code/BF2/Platform/macOS/HitchTrace.cpp")
    target_compile_definitions(bf2_linux_capture PRIVATE DEDICATED=0 BF2_IMAGE_RENDDX9=1
        BF2_GFX_DXVK=1 BF2_LINUX_CLIENT=1)
    target_compile_options(bf2_linux_capture PRIVATE
        "-include${BF2_SOURCE_ROOT}/Code/BF2/Core/BuildConfig.h" -Wall -Wextra -Werror)
    target_link_libraries(bf2_linux_capture PRIVATE bf2_dxvk_d3dx bf2_dxvk_forward SDL3::SDL3 PNG::PNG)
    set_target_properties(bf2_linux_capture PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS OFF BUILD_RPATH "${BF2_DXVK_LIBDIR}")
endif()
