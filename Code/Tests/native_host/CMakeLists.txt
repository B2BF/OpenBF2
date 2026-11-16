# [TEMP] Portable tests for the issue #36 native host boundary; no OS window or engine.
cmake_minimum_required(VERSION 3.22)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(bf2_native_host_checks LANGUAGES CXX)
    enable_testing()
endif()
get_filename_component(BF2_NATIVE_HOST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../BF2" ABSOLUTE)
add_executable(bf2_native_host_keymap keymap.cpp
    "${BF2_NATIVE_HOST_ROOT}/Platform/Host/KeyMap.cpp"
    "${BF2_NATIVE_HOST_ROOT}/Platform/macOS/MacKeyMap.cpp")
target_compile_features(bf2_native_host_keymap PRIVATE cxx_std_14)
target_compile_definitions(bf2_native_host_keymap PRIVATE DEDICATED=0 OPENBF2_HOST_TEST_SEAMS=1)
if(MSVC)
    target_compile_options(bf2_native_host_keymap PRIVATE "/FI${BF2_NATIVE_HOST_ROOT}/Core/BuildConfig.h")
else()
    target_compile_options(bf2_native_host_keymap PRIVATE "-include${BF2_NATIVE_HOST_ROOT}/Core/BuildConfig.h")
endif()
add_test(NAME native_host_keymap COMMAND bf2_native_host_keymap)
