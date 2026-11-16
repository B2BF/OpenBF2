# [TEMP] macOS input review harness. A standalone project like
# Code/Tests/macos_host, so it builds without the client:
#
#   cmake -S Code/Tests/macos_input -B out/build/macos-input-test -DCMAKE_BUILD_TYPE=RelWithDebInfo
#   cmake --build out/build/macos-input-test
#   out/build/macos-input-test/macos_input_test
#
# It links the production host (Code/BF2/Platform/macOS) and input devices (Code/BF2/IO/Input/Native*)
# with the control-map fixture of macos_host, runs the host headless (no window, no Dock icon, the
# real cursor untouched), feeds it NSEvents built from CoreGraphics events and checks what the
# engine-facing device interfaces report. Exit status 0 when every check passes.
# [RETIRE-WHEN: macos-client-window] Fold into the client's test suite once bf2_client runs input.
cmake_minimum_required(VERSION 3.21)
# GCMouse (MacRawMouse.mm) needs macOS 11, the port's minimum.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
project(macos_input_test LANGUAGES C CXX OBJCXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(FATAL_ERROR "macos_input_test builds on macOS only")
endif()

set(BF2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../BF2")

add_executable(macos_input_test
    main.mm
    ../macos_host/control_map_fixture.cpp
    "${BF2_ROOT}/Platform/macOS/CocoaHost.mm"
    "${BF2_ROOT}/Platform/macOS/MacGamepad.mm"
    "${BF2_ROOT}/Platform/macOS/MacRawMouse.mm"
    "${BF2_ROOT}/Platform/macOS/MacInputScript.mm"
    "${BF2_ROOT}/Platform/InputScript.cpp"
    "${BF2_ROOT}/Platform/macOS/MacKeyMap.cpp"
    "${BF2_ROOT}/Platform/Host/KeyMap.cpp"
    "${BF2_ROOT}/IO/Input/NativeInputDeviceManager.cpp"
    "${BF2_ROOT}/IO/Input/NativeKeyboardDevice.cpp"
    "${BF2_ROOT}/IO/Input/NativeMouseDevice.cpp"
    "${BF2_ROOT}/IO/Input/NativeGameControllerDevice.cpp")

set_target_properties(macos_input_test PROPERTIES CXX_STANDARD 14 CXX_EXTENSIONS ON
                                                  OBJCXX_STANDARD 14 OBJCXX_EXTENSIONS ON)
target_compile_definitions(macos_input_test PRIVATE DEDICATED=0 BF2_PLATFORM_POSIX OPENBF2_HOST_TEST_SEAMS)
target_compile_options(macos_input_test PRIVATE
    "SHELL:-include ${BF2_ROOT}/Core/BuildConfig.h"
    "SHELL:-include ${BF2_ROOT}/Core/PlatformCompat.h"
    -Wall -Wextra -Wno-unused-parameter
    $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
target_link_libraries(macos_input_test PRIVATE
    "-framework AppKit" "-framework QuartzCore" "-framework Metal"
    "-framework GameController" "-framework Carbon" "-framework IOKit")
