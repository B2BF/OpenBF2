# [TEMP] [CHOSEN, not recovered] Engine-free GPU/lifetime check for G3.
cmake_minimum_required(VERSION 3.21)
project(macos_metal_device LANGUAGES CXX OBJCXX)
if(NOT APPLE)
    message(FATAL_ERROR "This harness needs AppKit and a Metal GPU")
endif()
set(BF2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../BF2")
# [CHOSEN, not recovered] Exercise the production native compiler; no oracle/cache reader.
# BF2_FX_NATIVE_DEPS defaults to the worktree's out/build/nativefx (tools/macos/nativefx/build.sh).
# It must be built from the current tools/macos/vkd3d-patches: closureHigherProfileChecks fails
# with E5017 against an older deps build (the Effect CMakeLists warns at configure time).
add_subdirectory("${BF2_ROOT}/Platform/Gfx/Effect" "${CMAKE_CURRENT_BINARY_DIR}/effect")
add_executable(macos_metal_device
    main.cpp window.mm
    "${BF2_ROOT}/Platform/Gfx/Metal/Device.cpp"
    "${BF2_ROOT}/Platform/Gfx/Metal/FFVertex.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../ff_reference/ff_cases.cpp"
    "${BF2_ROOT}/Platform/Gfx/Metal/Effect.cpp"
    "${BF2_ROOT}/Platform/macOS/Warmup.cpp"
    "${BF2_ROOT}/Platform/macOS/HitchTrace.cpp"
    "${BF2_ROOT}/Platform/Gfx/Metal/D3DXMath.cpp"
    "${BF2_ROOT}/Platform/Gfx/Metal/D3DXGeometry.cpp"
    "${BF2_ROOT}/Platform/Gfx/Metal/D3DXPixels.cpp"
    "${BF2_ROOT}/Platform/Gfx/Metal/D3DXImages.cpp"
    "${BF2_ROOT}/Platform/Gfx/Metal/D3DXTextures.cpp"
    "${BF2_ROOT}/Platform/Gfx/Metal/D3DXMesh.cpp"
    "${BF2_ROOT}/Platform/Gfx/Metal/MetalBridge.mm"
    "${BF2_ROOT}/Platform/macOS/CocoaHost.mm"
    "${BF2_ROOT}/Platform/macOS/MacRawMouse.mm"
    "${BF2_ROOT}/Platform/macOS/MacInputScript.mm"
    "${BF2_ROOT}/Platform/InputScript.cpp"
    "${BF2_ROOT}/Platform/macOS/MacKeyMap.cpp")
set_target_properties(macos_metal_device PROPERTIES CXX_STANDARD 17 OBJCXX_STANDARD 17)
target_compile_definitions(macos_metal_device PRIVATE DEDICATED=0 BF2_IMAGE_RENDDX9=1 BF2_PLATFORM_POSIX
    BF2_METAL_DEVICE_TESTING=1 BF2_FX_NATIVE=1)
target_compile_definitions(macos_metal_device PRIVATE BF2_MENU_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/../macos_d3dx_helpers/fixtures")
target_compile_options(macos_metal_device PRIVATE
    "SHELL:-include ${BF2_ROOT}/Core/BuildConfig.h"
    "SHELL:-include ${BF2_ROOT}/Core/PlatformCompat.h"
    -Wall -Wextra -Wno-unused-parameter
    $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
target_link_libraries(macos_metal_device PRIVATE
    bf2_fx_native_compiler
    z
    "-framework AppKit" "-framework QuartzCore" "-framework Metal"
    "-framework Carbon" "-framework IOKit" "-framework GameController" "-framework ImageIO" "-framework CoreGraphics")
