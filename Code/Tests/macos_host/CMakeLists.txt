# [TEMP] Scaffolding for package S2.4 (macOS Cocoa host and input). A standalone project, so it
# builds while the macOS client target (package S2.0) does not yet link:
#
#   cmake --preset macos-host-test            (from this directory)
#   cmake --build --preset macos-host-test
#   ../../../out/build/macos-host-test/macos_host_test --seconds 3
#
# It links the production host (Code/BF2/Platform/macOS) and the production input devices
# (Code/BF2/IO/Input/Mac*) with a control-map fixture. [RETIRE-WHEN: macos-client-window]
# Remove once the bf2_client target opens its window through the host on macOS.
cmake_minimum_required(VERSION 3.21)
# GCMouse (MacRawMouse.mm) needs macOS 11, the port's minimum.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
project(macos_host_test LANGUAGES C CXX OBJCXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(FATAL_ERROR "macos_host_test builds on macOS only")
endif()

set(BF2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../BF2")

add_executable(macos_host_test
    main.mm
    control_map_fixture.cpp
    "${BF2_ROOT}/Platform/macOS/CocoaHost.mm"
    "${BF2_ROOT}/Platform/macOS/MacGamepad.mm"
    "${BF2_ROOT}/Platform/macOS/MacRawMouse.mm"
    "${BF2_ROOT}/Platform/macOS/MacInputScript.mm"
    "${BF2_ROOT}/Platform/InputScript.cpp"
    "${BF2_ROOT}/Platform/macOS/MacKeyMap.cpp"
    "${BF2_ROOT}/IO/Input/MacInputDeviceManager.cpp"
    "${BF2_ROOT}/IO/Input/MacKeyboardDevice.cpp"
    "${BF2_ROOT}/IO/Input/MacMouseDevice.cpp"
    "${BF2_ROOT}/IO/Input/MacGameControllerDevice.cpp")

set_target_properties(macos_host_test PROPERTIES CXX_STANDARD 14 CXX_EXTENSIONS ON
                                                 OBJCXX_STANDARD 14 OBJCXX_EXTENSIONS ON)
# The client image switches, as the engine build passes them (CMakeLists.txt bf2_set_image).
target_compile_definitions(macos_host_test PRIVATE DEDICATED=0 BF2_PLATFORM_POSIX OPENBF2_HOST_TEST_SEAMS)
target_compile_options(macos_host_test PRIVATE
    "SHELL:-include ${BF2_ROOT}/Core/BuildConfig.h"
    "SHELL:-include ${BF2_ROOT}/Core/PlatformCompat.h"
    -Wall -Wextra -Wno-unused-parameter
    $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
target_link_libraries(macos_host_test PRIVATE
    "-framework AppKit" "-framework QuartzCore" "-framework Metal"
    "-framework GameController" "-framework Carbon" "-framework IOKit")
