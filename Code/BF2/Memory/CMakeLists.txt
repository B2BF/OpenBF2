# bf2_memory - the rebuilt Memory.dll.
#
# Builds on its own (cmake -S Code/BF2/Memory) or from the top-level tree, where it is
# EXCLUDE_FROM_ALL. No game image links it: the game allocates through the shared CRT
# (https://github.com/B2BF/OpenBF2/issues/7). Output name Memory (Memory.dll, Memory.dylib,
# Memory.so).

cmake_minimum_required(VERSION 3.16)

set(BF2_MEMORY_STANDALONE OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(BF2Memory LANGUAGES CXX)
    set(BF2_MEMORY_STANDALONE ON)
endif()

# [CHOSEN, not recovered] AUTO enables pass-through (no chunk pools) when the compiler reports
# AddressSanitizer; ON and OFF force it.
set(BF2_MEMORY_PASSTHROUGH "AUTO" CACHE STRING "Memory.dll pass-through to the CRT heap: AUTO, ON or OFF")
set_property(CACHE BF2_MEMORY_PASSTHROUGH PROPERTY STRINGS AUTO ON OFF)
option(BF2_MEMORY_ASAN "Build bf2_memory and its tests with AddressSanitizer (Clang/GCC)" OFF)

set(BF2_MEMORY_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/AllocInfo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Callstack.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ChunkPools.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Memory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Histogram.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Threading.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/MemoryTrace.cpp"
)

function(bf2_memory_configure target)
    target_compile_features(${target} PRIVATE cxx_std_14)
    target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(${target} PRIVATE BF2_MEMORY_BUILD)
    if(BF2_MEMORY_PASSTHROUGH STREQUAL "ON")
        target_compile_definitions(${target} PRIVATE BF2_MEMORY_PASSTHROUGH=1)
    elseif(BF2_MEMORY_PASSTHROUGH STREQUAL "OFF")
        target_compile_definitions(${target} PRIVATE BF2_MEMORY_PASSTHROUGH=0)
    endif()
    if(MSVC)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
        # The shipped DLL uses the dynamic CRT (MSVCR71/MSVCP71).
        set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    else()
        # The shipped code formats with sprintf; keep it and silence the SDK deprecation.
        target_compile_options(${target} PRIVATE -Wall -Wno-deprecated-declarations)
        if(BF2_MEMORY_ASAN)
            target_compile_options(${target} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address)
        endif()
    endif()
endfunction()

if(BF2_MEMORY_STANDALONE)
    add_library(bf2_memory SHARED ${BF2_MEMORY_SOURCES})
else()
    add_library(bf2_memory SHARED EXCLUDE_FROM_ALL ${BF2_MEMORY_SOURCES})
endif()
bf2_memory_configure(bf2_memory)
set_target_properties(bf2_memory PROPERTIES
    OUTPUT_NAME Memory
    PREFIX ""
    CXX_VISIBILITY_PRESET hidden
    FOLDER "BF2"
)
if(WIN32)
    target_link_libraries(bf2_memory PRIVATE kernel32 user32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(bf2_memory PRIVATE Threads::Threads)
endif()

# The same sources as a static object set, for the white-box tests of the internal report formats.
add_library(bf2_memory_internal STATIC EXCLUDE_FROM_ALL ${BF2_MEMORY_SOURCES})
bf2_memory_configure(bf2_memory_internal)
if(WIN32)
    target_link_libraries(bf2_memory_internal PUBLIC kernel32 user32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(bf2_memory_internal PUBLIC Threads::Threads)
endif()

option(BF2_MEMORY_TESTS "Build the bf2_memory tests" ${BF2_MEMORY_STANDALONE})
if(BF2_MEMORY_TESTS)
    if(BF2_MEMORY_STANDALONE)
        enable_testing()
    endif()
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../Tests/memory" "${CMAKE_CURRENT_BINARY_DIR}/tests")
endif()
