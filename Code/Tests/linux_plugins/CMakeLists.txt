# [TEMP] [CHOSEN, not recovered] Static ELF plugin isolation contracts, issue #36.
cmake_minimum_required(VERSION 3.21)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(BF2LinuxPlugins LANGUAGES CXX)
    enable_testing()
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "The plugin isolation fixture requires ELF on Linux")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_filename_component(_plugin_repo "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
add_library(bf2_plugin_fixture_a STATIC image.cpp)
add_library(bf2_plugin_fixture_b STATIC image.cpp)
target_compile_definitions(bf2_plugin_fixture_a PRIVATE FIXTURE_ENTRY=pluginA FIXTURE_FACTOR=10 FIXTURE_INITIAL=1000)
target_compile_definitions(bf2_plugin_fixture_b PRIVATE FIXTURE_ENTRY=pluginB FIXTURE_FACTOR=100 FIXTURE_INITIAL=10000)
foreach(_image a b)
    set_target_properties(bf2_plugin_fixture_${_image} PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
    target_compile_options(bf2_plugin_fixture_${_image} PRIVATE -fno-lto)
    set(_isolated "${CMAKE_CURRENT_BINARY_DIR}/plugin_${_image}.o")
    add_custom_command(OUTPUT "${_isolated}"
        COMMAND "${Python3_EXECUTABLE}" "${_plugin_repo}/tools/linux/prelink-plugin.py"
            --linker "${CMAKE_LINKER}" --objcopy "${CMAKE_OBJCOPY}" --nm "${CMAKE_NM}"
            --symbols "${CMAKE_CURRENT_SOURCE_DIR}/private.syms" --image "fixture_${_image}"
            --output "${_isolated}" "$<TARGET_FILE:bf2_plugin_fixture_${_image}>"
        DEPENDS bf2_plugin_fixture_${_image} "${CMAKE_CURRENT_SOURCE_DIR}/private.syms"
            "${_plugin_repo}/tools/linux/prelink-plugin.py" VERBATIM)
    list(APPEND _isolated_plugins "${_isolated}")
endforeach()
add_executable(bf2_linux_plugin_contract main.cpp image.cpp ${_isolated_plugins})
target_compile_definitions(bf2_linux_plugin_contract PRIVATE FIXTURE_ENTRY=hostImage FIXTURE_FACTOR=1 FIXTURE_INITIAL=100)
target_compile_options(bf2_linux_plugin_contract PRIVATE -fno-lto)
set_target_properties(bf2_linux_plugin_contract PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
add_test(NAME linux_plugin_isolation COMMAND bf2_linux_plugin_contract)
