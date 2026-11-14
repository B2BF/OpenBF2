# [CHOSEN, not recovered] Standalone experiment; never linked into an engine image.
cmake_minimum_required(VERSION 3.21)
project(BF2ShaderPrototype LANGUAGES CXX OBJCXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OBJCXX_STANDARD 17)
get_filename_component(REPO "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(THIRD_PARTY "${REPO}/out/third_party" CACHE PATH "Local, already populated sources")
add_executable(shaderproto main.mm)
target_compile_definitions(shaderproto PRIVATE MOJOSHADER_EFFECT_SUPPORT)
target_include_directories(shaderproto PRIVATE "${THIRD_PARTY}/vkd3d/include" "${THIRD_PARTY}/mojoshader")
target_link_libraries(shaderproto PRIVATE
    "${THIRD_PARTY}/build/vkd3d/.libs/libvkd3d-shader.a"
    "${THIRD_PARTY}/build/vkd3d/.libs/libvkd3d-common.a"
    "${THIRD_PARTY}/build/mojoshader/libmojoshader.a"
    "-framework Foundation" "-framework Metal")
target_compile_options(shaderproto PRIVATE -Wall -Wextra -fobjc-arc)
