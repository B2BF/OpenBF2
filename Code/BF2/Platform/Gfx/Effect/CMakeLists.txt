# [CHOSEN, not recovered] Engine-independent library; only the macOS build enters here.
get_filename_component(BF2_FX_REPO "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
add_library(bf2_fx_runtime STATIC Cache.cpp Runtime.cpp)
target_include_directories(bf2_fx_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(bf2_fx_runtime PUBLIC cxx_std_17)
target_compile_definitions(bf2_fx_runtime PRIVATE DEDICATED=0 BF2_IMAGE_RENDDX9=1)
target_compile_options(bf2_fx_runtime PRIVATE
    "SHELL:-include ${BF2_ROOT}/Core/BuildConfig.h" -Wall -Wextra -Werror -Wno-deprecated-declarations)
# [CHOSEN, not recovered] Native effect front end (no compiler dependency): D3DX effect source
# parser, Description builder and the asm-block assembler. The shader compiler (vkd3d-shader)
# and translator (MojoShader) plug in through bf2fx::ShaderBackend.
add_library(bf2_fx_native STATIC D3DAsm.cpp FxBuild.cpp FxParser.cpp FxStates.cpp)
target_include_directories(bf2_fx_native PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(bf2_fx_native PUBLIC cxx_std_17)
target_compile_definitions(bf2_fx_native PRIVATE DEDICATED=0 BF2_IMAGE_RENDDX9=1)
target_compile_options(bf2_fx_native PRIVATE
    "SHELL:-include ${BF2_ROOT}/Core/BuildConfig.h" -Wall -Wextra -Werror -Wno-deprecated-declarations)
# [CHOSEN, not recovered] Native effect compiler for the client:
# vkd3d-shader (patched, LGPL-2.1+) and MojoShader (s1c patches, zlib), both built offline by
# tools/macos/nativefx/build.sh into out/build/nativefx. They are required: there is
# no alternate effect compiler or offline-cache-only client configuration.
set(_native_default "${BF2_FX_REPO}/out/build/nativefx")
if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
    if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
        string(APPEND _native_default "-universal")
    else()
        string(APPEND _native_default "-arm64")
    endif()
endif()
set(BF2_FX_NATIVE_DEPS "${_native_default}" CACHE PATH "vkd3d/MojoShader build of tools/macos/nativefx/build.sh")
set(_nd "${BF2_FX_NATIVE_DEPS}")
set(BF2_FX_NATIVE_AVAILABLE OFF)
if(EXISTS "${_nd}/vkd3d/.libs/libvkd3d-shader.a" AND EXISTS "${_nd}/vkd3d/.libs/libvkd3d-common.a"
        AND EXISTS "${_nd}/vkd3d-src/include/vkd3d_shader.h" AND EXISTS "${_nd}/mojo/libmojoshader.a"
        AND EXISTS "${_nd}/mojo-src/mojoshader.h")
    set(BF2_FX_NATIVE_AVAILABLE ON)
    # [CHOSEN, not recovered] Check actual archive slices, including overridden paths.
    # A universal build needs every requested slice in all three static dependencies.
    set(_native_archs ${CMAKE_OSX_ARCHITECTURES})
    if(NOT _native_archs)
        set(_native_archs ${CMAKE_SYSTEM_PROCESSOR})
    endif()
    foreach(_lib vkd3d/.libs/libvkd3d-shader.a vkd3d/.libs/libvkd3d-common.a mojo/libmojoshader.a)
        execute_process(COMMAND lipo "${_nd}/${_lib}" -verify_arch ${_native_archs}
            RESULT_VARIABLE _slice_result OUTPUT_QUIET ERROR_VARIABLE _slice_error)
        if(NOT _slice_result EQUAL 0)
            set(BF2_FX_NATIVE_AVAILABLE OFF)
            message(STATUS "Native effect dependency ${_lib}: ${_slice_error}")
        endif()
    endforeach()
endif()
# [CHOSEN, not recovered] A deps build made before the current vkd3d patches still links, but its
# compiler refuses what the patches fixed (macos_metal_device closureHigherProfileChecks fails with
# E5017 against one without 0011). build-vkd3d.sh records the applied patch names; say so loudly
# when they are missing or differ. Rebuild with tools/macos/nativefx/build.sh into a new directory.
if(BF2_FX_NATIVE_AVAILABLE)
    file(GLOB _want_patches RELATIVE "${BF2_FX_REPO}/tools/macos/vkd3d-patches" "${BF2_FX_REPO}/tools/macos/vkd3d-patches/*.patch")
    list(SORT _want_patches)
    set(_have_patches)
    if(EXISTS "${_nd}/vkd3d-src/.bf2-patches")
        file(STRINGS "${_nd}/vkd3d-src/.bf2-patches" _have_patches)
        list(SORT _have_patches)
    endif()
    if(NOT _have_patches STREQUAL _want_patches)
        set(_missing ${_want_patches})
        if(_have_patches)
            list(REMOVE_ITEM _missing ${_have_patches})
        endif()
        message(WARNING "BF2_FX_NATIVE_DEPS=${_nd} was not built from the current tools/macos/vkd3d-patches "
            "(no vkd3d-src/.bf2-patches record, or missing: ${_missing}). Native effect builds can refuse what "
            "those patches fix. Rebuild with: sh tools/macos/nativefx/build.sh (into a new directory, keeping "
            "the existing one).")
    endif()
endif()
set(BF2_FX_NATIVE_AVAILABLE ${BF2_FX_NATIVE_AVAILABLE} PARENT_SCOPE)
# Only the renderer-linked client needs the compiler; server-only builds (bf2, bf2_aidll) do not.
if(NOT BF2_FX_NATIVE_AVAILABLE AND BF2_MACOS_LINK_RENDDX9)
    message(FATAL_ERROR
        "The macOS client requires native effect compilation (BF2_FX_NATIVE).\n"
        "Missing or incompatible ${CMAKE_OSX_ARCHITECTURES} vkd3d-shader/MojoShader dependencies at: ${_nd}\n"
        "Build offline with: sh tools/macos/nativefx/build.sh [x86_64|arm64|universal]\n"
        "This needs local sources in out/third_party/vkd3d and out/third_party/mojoshader; "
        "no downloads or installs are performed. See docs/building.md (macOS).\n"
        "Or configure with -DBF2_FX_NATIVE_DEPS=/absolute/path/to/nativefx containing "
        "vkd3d/.libs/libvkd3d-{shader,common}.a, vkd3d-src/include/vkd3d_shader.h, "
        "mojo/libmojoshader.a and mojo-src/mojoshader.h.\n"
        "All libraries must contain every requested architecture. There is no Wine cache fallback.")
endif()
if(BF2_FX_NATIVE_AVAILABLE)
    set(_inc "${CMAKE_CURRENT_BINARY_DIR}/native-generated")
    add_custom_command(OUTPUT "${_inc}/refusals.inc"
        COMMAND "${CMAKE_COMMAND}" -DIN="${CMAKE_CURRENT_SOURCE_DIR}/data/d3dx-refusals.txt" -DOUT="${_inc}/refusals.inc"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/refusals.cmake"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/data/d3dx-refusals.txt" "${CMAKE_CURRENT_SOURCE_DIR}/refusals.cmake"
        COMMENT "BF2FX D3DX refusal table")
    # Toolchain identity: every input that changes native output.
    file(GLOB _id_inputs "${BF2_FX_REPO}/tools/macos/vkd3d-patches/*.patch"
        "${BF2_FX_REPO}/tools/macos/d3dxfx/patches/*.patch" "${CMAKE_CURRENT_SOURCE_DIR}/Native*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Fx*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/D3DAsm.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/d3dx-refusals.txt")
    list(SORT _id_inputs)
    # [CHOSEN, not recovered] The linked archives are inputs too: the patch files above say what
    # the deps should contain, not what a given BF2_FX_NATIVE_DEPS build contains. A stale deps
    # build (fewer vkd3d/MojoShader patches) must not share cache keys with a current one, or
    # its refusals poison the cache for good builds. Rebuilt archives reconfigure.
    set(_id_libs "${_nd}/vkd3d/.libs/libvkd3d-shader.a" "${_nd}/vkd3d/.libs/libvkd3d-common.a"
        "${_nd}/mojo/libmojoshader.a")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_id_libs})
    set(_id "vkd3d-2.1")
    foreach(f IN LISTS _id_inputs _id_libs)
        file(SHA256 "${f}" _h)
        string(APPEND _id "${_h}")
    endforeach()
    string(SHA256 _id "${_id}")
    string(SUBSTRING "${_id}" 0 24 _id)
    add_library(bf2_fx_native_compiler STATIC Native.cpp NativeMojo.cpp NativeVkd3d.cpp "${_inc}/refusals.inc")
    target_include_directories(bf2_fx_native_compiler PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
        PRIVATE "${_inc}" "${_nd}/vkd3d-src/include" "${_nd}/mojo-src")
    target_compile_features(bf2_fx_native_compiler PUBLIC cxx_std_17)
    target_compile_definitions(bf2_fx_native_compiler PRIVATE DEDICATED=0 BF2_IMAGE_RENDDX9=1
        MOJOSHADER_EFFECT_SUPPORT BF2_FX_NATIVE_IDENTITY="${_id}")
    target_compile_options(bf2_fx_native_compiler PRIVATE
        "SHELL:-include ${BF2_ROOT}/Core/BuildConfig.h" -Wall -Wextra -Werror -Wno-deprecated-declarations)
    target_link_libraries(bf2_fx_native_compiler PUBLIC bf2_fx_native bf2_fx_runtime
        "${_nd}/vkd3d/.libs/libvkd3d-shader.a" "${_nd}/vkd3d/.libs/libvkd3d-common.a" "${_nd}/mojo/libmojoshader.a")
    message(STATUS "BF2FX native effect compiler: ${_id}")
endif()
option(BF2_FX_DEVELOPER_ORACLE "Enable optional developer oracle tools/tests, not used by the client" OFF)
if(BF2_FX_DEVELOPER_ORACLE)
set(BF2_FX_MOJOSHADER_SOURCE "" CACHE PATH "Local s1c-patched MojoShader source for the offline exporter")
set(BF2_FX_MOJOSHADER_LIBRARY "" CACHE FILEPATH "Matching locally built libmojoshader.a")
if(BF2_FX_MOJOSHADER_SOURCE AND BF2_FX_MOJOSHADER_LIBRARY)
    add_executable(bf2_fx_cache_tool EXCLUDE_FROM_ALL "${BF2_FX_REPO}/tools/macos/d3dxfx/cache-tool.cpp")
    target_include_directories(bf2_fx_cache_tool PRIVATE "${BF2_FX_MOJOSHADER_SOURCE}")
    target_compile_definitions(bf2_fx_cache_tool PRIVATE MOJOSHADER_EFFECT_SUPPORT DEDICATED=0 BF2_IMAGE_RENDDX9=1)
    target_compile_options(bf2_fx_cache_tool PRIVATE
        "SHELL:-include ${BF2_ROOT}/Core/BuildConfig.h" -Wall -Wextra -Werror)
    target_link_libraries(bf2_fx_cache_tool PRIVATE bf2_fx_runtime "${BF2_FX_MOJOSHADER_LIBRARY}")
endif()
add_subdirectory("${BF2_FX_REPO}/Code/Tests/macos_fx_runtime" "${CMAKE_CURRENT_BINARY_DIR}/tests")
endif()
