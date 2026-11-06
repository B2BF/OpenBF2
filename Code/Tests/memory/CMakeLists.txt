# [TEMP] Test scaffolding for bf2_memory.
#
# memory_unit      black-box checks through the 32 exports of the shared library
# memory_internal  white-box checks of the internal report formats (static object set)
# memory_diff      Windows only: the differential harness run against the retail Memory.dll and
#                  the rebuilt MSVC x86 DLL (memory_diff.cmd)

add_executable(memory_unit memory_unit.cpp)
target_compile_features(memory_unit PRIVATE cxx_std_14)
target_link_libraries(memory_unit PRIVATE bf2_memory)
target_include_directories(memory_unit PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../BF2/Memory")

add_executable(memory_internal memory_internal.cpp)
target_compile_features(memory_internal PRIVATE cxx_std_14)
target_link_libraries(memory_internal PRIVATE bf2_memory_internal)
target_compile_definitions(memory_internal PRIVATE BF2_MEMORY_BUILD)

foreach(test_target memory_unit memory_internal)
    if(BF2_MEMORY_PASSTHROUGH STREQUAL "ON")
        target_compile_definitions(${test_target} PRIVATE BF2_MEMORY_PASSTHROUGH=1)
    elseif(BF2_MEMORY_PASSTHROUGH STREQUAL "OFF")
        target_compile_definitions(${test_target} PRIVATE BF2_MEMORY_PASSTHROUGH=0)
    endif()
    if(MSVC)
        target_compile_definitions(${test_target} PRIVATE _CRT_SECURE_NO_WARNINGS)
        set_property(TARGET ${test_target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    elseif(BF2_MEMORY_ASAN)
        target_compile_options(${test_target} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(${test_target} PRIVATE -fsanitize=address)
    endif()
    if(NOT WIN32)
        find_package(Threads REQUIRED)
        target_link_libraries(${test_target} PRIVATE Threads::Threads)
    endif()
    add_test(NAME ${test_target} COMMAND ${test_target} WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endforeach()

if(WIN32)
    add_executable(memory_diff memory_diff.cpp)
    target_compile_definitions(memory_diff PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
