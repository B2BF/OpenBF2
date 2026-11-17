# [TEMP] [CHOSEN, not recovered] Native audio lifecycle checks for issue #36.
cmake_minimum_required(VERSION 3.21)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(BF2LinuxAudioChecks LANGUAGES C CXX)
    enable_testing()
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Linux audio checks require a 64-bit Linux toolchain")
endif()
find_package(SDL3 3.2 REQUIRED CONFIG)
get_filename_component(BF2_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../BF2" ABSOLUTE)
get_filename_component(BF2_ABI "${CMAKE_CURRENT_LIST_DIR}/../../RendDX9Abi" ABSOLUTE)
set(GS "${BF2_ROOT}/External/GameSpy")
set(FF "${BF2_ROOT}/External/ffmpeg-bink")
add_library(linux_voice STATIC
    "${GS}/Voice2/gvMain.c" "${GS}/Voice2/gvSource.c" "${GS}/Voice2/gvFrame.c"
    "${GS}/Voice2/gvCodec.c" "${GS}/Voice2/gvSpeex.c" "${GS}/Voice2/gvDevice.c"
    "${GS}/Voice2/gvUtil.c" "${GS}/Voice2/gvCustomDevice.c" "${GS}/Voice2/gvSDLAudio.c"
    "${GS}/darray.c" "${GS}/common/gsMemory.c")
foreach(unit bits cb_search exc_10_16_table exc_10_32_table exc_20_32_table exc_5_256_table
    exc_5_64_table exc_8_128_table filters gain_table gain_table_lbr hexc_10_32_table hexc_table
    high_lsp_tables lpc lsp lsp_tables_nb ltp misc modes nb_celp quant_lsp sb_celp speex_callbacks vbr vq)
    target_sources(linux_voice PRIVATE "${BF2_ROOT}/External/speex-1.0.5/libspeex/${unit}.c")
endforeach()
target_include_directories(linux_voice PRIVATE "${BF2_ROOT}/External/speex-1.0.5/include"
    "${BF2_ROOT}/External/speex-1.0.5/include/speex")
target_link_libraries(linux_voice PUBLIC SDL3::SDL3 m)
add_library(linux_bink_decoder STATIC "${FF}/libavcodec/bink.c" "${FF}/libavcodec/binkdsp.c"
    "${FF}/libavcodec/binkaudio.c" "${FF}/libavcodec/wma_freqs.c" "${FF}/shim/ffbink_shim.c")
target_include_directories(linux_bink_decoder PUBLIC "${FF}/shim")
target_compile_options(linux_bink_decoder PRIVATE -O2 -fno-strict-aliasing -fno-math-errno -Wno-pointer-sign)
add_executable(bf2_linux_audio_check main.cpp "${BF2_ABI}/BinkAudioSDL.cpp")
target_include_directories(bf2_linux_audio_check PRIVATE "${FF}/shim")
target_link_libraries(bf2_linux_audio_check PRIVATE linux_voice SDL3::SDL3)
add_executable(bf2_linux_bink_decode ../macos_bink_decode.cpp "${BF2_ABI}/BinkFFmpeg.cpp" "${BF2_ABI}/BinkAudioSDL.cpp")
target_link_libraries(bf2_linux_bink_decode PRIVATE linux_bink_decoder SDL3::SDL3)
# The explicit movie argument keeps copyrighted game content outside automated fixtures.
add_executable(bf2_linux_bink_path_check EXCLUDE_FROM_ALL bink_paths.cpp
    "${BF2_ABI}/BinkFFmpeg.cpp" "${BF2_ABI}/BinkAudioSDL.cpp")
target_link_libraries(bf2_linux_bink_path_check PRIVATE linux_bink_decoder SDL3::SDL3)
add_executable(bf2_linux_openal_check openal.cpp "${BF2_ROOT}/Sound/SoundEngine/BfOpenAL.cpp"
    "${BF2_ROOT}/Sound/SoundEngine/Win32System.cpp")
target_compile_definitions(bf2_linux_openal_check PRIVATE BF2_IMAGE_BF2AUDIO=1)
target_compile_options(bf2_linux_openal_check PRIVATE "SHELL:-include ${BF2_ROOT}/Core/PlatformCompat.h")
target_link_libraries(bf2_linux_openal_check PRIVATE dl pthread)
add_executable(bf2_linux_audio_output_probe EXCLUDE_FROM_ALL output.cpp)
target_link_libraries(bf2_linux_audio_output_probe PRIVATE SDL3::SDL3)
set_target_properties(bf2_linux_audio_output_probe PROPERTIES CXX_STANDARD 14)
foreach(target linux_voice linux_bink_decoder bf2_linux_audio_check bf2_linux_bink_decode bf2_linux_bink_path_check bf2_linux_openal_check)
    target_compile_definitions(${target} PRIVATE DEDICATED=0 BF2_LINUX_CLIENT=1 _LINUX _GNU_SOURCE)
    target_compile_options(${target} PRIVATE "SHELL:-include ${BF2_ROOT}/Core/BuildConfig.h")
    set_target_properties(${target} PROPERTIES CXX_STANDARD 14 C_STANDARD 11)
endforeach()
add_test(NAME linux_audio_lifecycle COMMAND bf2_linux_audio_check)
set_tests_properties(linux_audio_lifecycle PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
add_test(NAME linux_audio_unavailable COMMAND bf2_linux_audio_check --unavailable)
set_tests_properties(linux_audio_unavailable PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=unavailable-test-driver")
add_test(NAME linux_openal_lifecycle COMMAND bf2_linux_openal_check)
set_tests_properties(linux_openal_lifecycle PROPERTIES ENVIRONMENT "ALSOFT_DRIVERS=null")
add_test(NAME linux_openal_unavailable COMMAND bf2_linux_openal_check --unavailable)
set_tests_properties(linux_openal_unavailable PROPERTIES ENVIRONMENT "ALSOFT_DRIVERS=unavailable-test-driver")
