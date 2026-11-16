# [TEMP] [CHOSEN, not recovered] Native Linux profile encryption contracts, issue #36.
cmake_minimum_required(VERSION 3.21)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(BF2LinuxProfiles LANGUAGES CXX)
    enable_testing()
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Linux profile checks require a Linux toolchain")
endif()
get_filename_component(BF2_PROFILE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../BF2" ABSOLUTE)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)
add_executable(bf2_linux_profile_check
    ../macos_protected_data/protected_data_check.cpp
    "${BF2_PROFILE_ROOT}/Game/PasswordCodec.cpp"
    "${BF2_PROFILE_ROOT}/Game/MatchMaking/Client/CdKeyStore.cpp"
    "${BF2_PROFILE_ROOT}/Core/Registry.cpp"
    "${BF2_PROFILE_ROOT}/Core/StringUtil.cpp"
    "${BF2_PROFILE_ROOT}/Platform/macOS/ProtectedData.cpp"
    "${BF2_PROFILE_ROOT}/Platform/macOS/ClientPaths.cpp")
target_compile_definitions(bf2_linux_profile_check PRIVATE DEDICATED=0 BF2_LINUX_CLIENT=1 BF2_PLATFORM_POSIX=1)
target_compile_options(bf2_linux_profile_check PRIVATE
    "-include${BF2_PROFILE_ROOT}/Core/BuildConfig.h"
    "-include${BF2_PROFILE_ROOT}/Core/PlatformCompat.h")
target_link_libraries(bf2_linux_profile_check PRIVATE OpenSSL::Crypto)
target_link_options(bf2_linux_profile_check PRIVATE "-Wl,--wrap=RAND_bytes" "-Wl,--wrap=EVP_CipherFinal_ex")
set_target_properties(bf2_linux_profile_check PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
add_test(NAME linux_profile_protection COMMAND bf2_linux_profile_check "${CMAKE_CURRENT_BINARY_DIR}")
