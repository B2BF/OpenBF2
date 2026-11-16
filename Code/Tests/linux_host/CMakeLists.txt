cmake_minimum_required(VERSION 3.22)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(bf2_linux_host_check LANGUAGES CXX)
    enable_testing()
endif()
find_package(SDL3 CONFIG REQUIRED)
get_filename_component(BF2_HOST_TEST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../BF2" ABSOLUTE)
set(BF2_HOST_TEST_SOURCES
    "${BF2_HOST_TEST_ROOT}/Platform/InputScript.cpp"
    "${BF2_HOST_TEST_ROOT}/Platform/Linux/SDLInputScript.cpp"
    "${BF2_HOST_TEST_ROOT}/Platform/Linux/SDLHost.cpp"
    "${BF2_HOST_TEST_ROOT}/Platform/macOS/MacKeyMap.cpp"
    "${BF2_HOST_TEST_ROOT}/Platform/macOS/MacSystem.cpp")
add_executable(bf2_linux_host_check main.cpp ${BF2_HOST_TEST_SOURCES})
add_executable(bf2_linux_cursor_check cursor.cpp ${BF2_HOST_TEST_SOURCES})
foreach(target bf2_linux_host_check bf2_linux_cursor_check)
    target_compile_features(${target} PRIVATE cxx_std_14)
    target_compile_definitions(${target} PRIVATE DEDICATED=0 BF2_LINUX_CLIENT=1)
    target_compile_options(${target} PRIVATE "-include${BF2_HOST_TEST_ROOT}/Core/BuildConfig.h")
    target_link_libraries(${target} PRIVATE SDL3::SDL3)
endforeach()
target_link_options(bf2_linux_cursor_check PRIVATE
    "-Wl,--wrap=SDL_CreateColorCursor" "-Wl,--wrap=SDL_CreateSystemCursor"
    "-Wl,--wrap=SDL_DestroyCursor" "-Wl,--wrap=SDL_SetCursor"
    "-Wl,--wrap=SDL_ShowCursor" "-Wl,--wrap=SDL_HideCursor")
add_test(NAME linux_host_cursor COMMAND bf2_linux_cursor_check)
set_tests_properties(linux_host_cursor PROPERTIES ENVIRONMENT
    "SDL_VIDEODRIVER=dummy;BF2_INPUT_SCRIPT=;BF2_CAPTURE_DIR=")
add_test(NAME linux_host_contract COMMAND bf2_linux_host_check)
set_tests_properties(linux_host_contract PROPERTIES ENVIRONMENT
    "SDL_VIDEODRIVER=dummy;BF2_INPUT_SCRIPT=;BF2_CAPTURE_DIR=")
add_test(NAME linux_host_script COMMAND bf2_linux_host_check --script)
set_tests_properties(linux_host_script PROPERTIES ENVIRONMENT
    "SDL_VIDEODRIVER=dummy;BF2_INPUT_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/script.input;BF2_INPUT_SCRIPT_REAL_FOCUS=")
