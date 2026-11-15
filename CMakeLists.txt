cmake_minimum_required(VERSION 3.21)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()

project(BF2Rebuilt LANGUAGES C CXX)
# [CHOSEN, not recovered] Header-selection preparation for the G2w forwarding backend.
option(BF2_GFX_SEAM "Compile the renderer against the D3D9/D3DX subset headers" OFF)
# [CHOSEN, not recovered] Release packaging (owner decision 2026-11-14): CI's release configure steps
# turn this ON so the plugin kit's frontier trap (Code/RendDX9Abi/Frontier.cpp) logs and continues
# when its env var is unset. OFF - dev builds and every gate - keeps the armed (abort) default. An
# explicit env value (continue / abort) wins in both. Reaches the .cmd image builds as the env var
# BF2_FRONTIER_DEFAULT_CONTINUE=ON|OFF (always passed, so a stray shell variable cannot flip it).
option(BF2_FRONTIER_DEFAULT_CONTINUE "Frontier trap defaults to continue (release packages)" OFF)
if(BF2_FRONTIER_DEFAULT_CONTINUE)
    set(BF2_FRONTIER_DEFAULT_ENV "${CMAKE_COMMAND}" -E env BF2_FRONTIER_DEFAULT_CONTINUE=ON)
else()
    set(BF2_FRONTIER_DEFAULT_ENV "${CMAKE_COMMAND}" -E env BF2_FRONTIER_DEFAULT_CONTINUE=OFF)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # [CHOSEN, not recovered] The macOS client's Cocoa host is Objective-C++ (docs/macos-s2-4.md).
    enable_language(OBJCXX)
endif()

# =================================================================================================
# TOOLCHAINS (2026-09-29). Three configurations, one source tree:
#   * MSVC x86            - the retail-fidelity reconstruction: bf2_w32ded.exe, strict link.
#   * MSVC x64            - the native Windows dedicated port: bf2_w64ded.exe, strict link.
#   * GCC/Clang, Linux    - the native Linux dedicated port: `bf2` + `libdice_py.so`. It is the x64
#     x86-64                port's pointer-bearing layouts compiled by the platform family the ELF
#                           oracle was built for.
#   * AppleClang, macOS   - the native macOS dedicated port: `bf2` + `libdice_py.dylib`, the Linux
#                           port's sources and layouts built for Darwin (docs/macos-port.md).
# Linux and macOS form the POSIX family (BF2_BUILD_POSIX); what they share sits under that switch,
# and only what genuinely differs sits under BF2_BUILD_LINUX or BF2_BUILD_MACOS. The MSVC
# configurations are untouched by any of them. Both POSIX builds are native ports
# (`[CHOSEN, not recovered]` layouts, like x64), not reproductions of the shipped 32-bit ELF's
# object layouts. The macOS configuration is an intentional port change requested by the project
# owner, `[CHOSEN, not recovered]` throughout.
# =================================================================================================
set(BF2_BUILD_LINUX OFF)
set(BF2_BUILD_MACOS OFF)
set(BF2_BUILD_POSIX OFF)
if(MSVC)
    # The two Windows configurations, selected by pointer width below.
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(BF2_BUILD_LINUX ON)
    set(BF2_BUILD_POSIX ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # [CHOSEN, not recovered] AppleClang (or upstream Clang) on macOS: the macos-x64 preset.
    set(BF2_BUILD_MACOS ON)
    set(BF2_BUILD_POSIX ON)
else()
    message(FATAL_ERROR
        "BF2Rebuilt builds with MSVC on Windows (msvc-x86 / msvc-x64 presets), with GCC/Clang on "
        "Linux x86-64 (linux-x64 preset) or with Clang on macOS (macos-x64 preset). This "
        "toolchain is ${CMAKE_CXX_COMPILER_ID} on ${CMAKE_SYSTEM_NAME}.")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND NOT BF2_BUILD_POSIX)
    set(BF2_BUILD_X64 OFF)
    set(BF2_NATIVE_ARCH "x86")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    # [CHOSEN, not recovered] The shipped PE/ELF layouts remain the x86 fidelity oracle. This
    # configuration is a native portability target and must acquire its own pointer-bearing
    # layouts while preserving the shipped wire/file widths.
    set(BF2_BUILD_X64 ON)
    if(BF2_BUILD_LINUX)
        set(BF2_NATIVE_ARCH "linux-x64")
        message(STATUS
            "Configuring the native Linux x86-64 dedicated-server port (bf2 + libdice_py.so). "
            "See docs/building.md for toolchain and runtime setup.")
    elseif(BF2_BUILD_MACOS)
        set(BF2_NATIVE_ARCH "macos-${CMAKE_SYSTEM_PROCESSOR}")
        message(STATUS
            "Configuring the native macOS ${CMAKE_SYSTEM_PROCESSOR} dedicated-server port "
            "(bf2 + libdice_py.dylib). See docs/building.md and docs/macos-port.md.")
    else()
        set(BF2_NATIVE_ARCH "x64")
        message(STATUS
            "Configuring the native x64 dedicated-server port with strict linking (no /FORCE). "
            "See docs/building.md for toolchain and runtime setup.")
    endif()
else()
    message(FATAL_ERROR
        "BF2Rebuilt supports only MSVC x86 (retail-fidelity), MSVC x64 (native dedicated port) "
        "and 64-bit Linux/macOS (native dedicated port) configurations; "
        "CMAKE_SIZEOF_VOID_P=${CMAKE_SIZEOF_VOID_P}.")
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ⚠ /RTC1 IS STRIPPED FROM THE DEBUG CONFIG, deliberately, added 2026-08-15.
#
# CMake injects /Od /RTC1 into Debug builds. Neither appears in the project's own build scripts:
# Code/Build/build.cmd compiles the engine with no /O or /RTC flag at all, and the
# Code/Tests/*.cmd check scripts compile with /O2. So a CMake Debug build was not reproducing
# either gate - and worse, /RTC1 is INCOMPATIBLE with the /O2 the check targets need (cl error
# D8016), which broke every check target in CLion's default Debug profile.
#
# Stripping /RTC1 here makes the Debug config match build.cmd's flags and lets the check targets
# keep the /O2 their .cmd counterparts use, so CLion reproduces the SAME RESULTS as the
# command-line gates. That is the point of this file.
#
# ⚠ AND THE INVESTIGATION TURNED UP A REAL FINDING, ISOLATED 2026-08-15:
#   /O2                  -> bf2_runtime_pair passes 412/412
#   /Od         (alone)  -> FAILS 1: "Object::disableCachedObject preserves child policy, map
#                           order, raw resets, and tail order"
#   /Od /RTC1            -> fails the same one check, and ONLY that one
# So the divergence is the OPTIMISATION LEVEL, not the runtime checks - /RTC1 is exonerated, and
# the stripping above is purely about the D8016 conflict.
#
# That check compares an exact EVENT-ORDER vector. Program-observable ordering should not move
# with /O2, so this is very likely unspecified-evaluation-order in either the harness or the
# recovered Object::disableCachedObject: MSVC may evaluate the arguments of one call in either
# order, and two side-effecting calls in a single expression will then emit their trace events in
# an order that flips with the optimiser. The ELF has ONE fixed order, so if the culprit is in
# recovered code, our source is not pinning the order the binary actually has - which matters,
# because this project has already found emission/iteration order to be wire-observable
# elsewhere. [UNVERIFIED HYPOTHESIS - the divergence is measured, the cause is not yet pinned.]
# Build bf2_runtime_pair with /Od to reproduce it in one step.
string(REGEX REPLACE "/RTC[1csu]+" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
string(REGEX REPLACE "/RTC[1csu]+" "" CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}")

set(BF2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Code/BF2")
set(BF2_EXTERNAL_ROOT "${BF2_ROOT}/External")
set(BF2_STUB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Code/Tests")

# build.cmd recursively discovers every recovered .cpp below Code/BF2 except
# External. Discover the same component directories without walking the very
# large vendored Python and GameSpy trees.
file(GLOB bf2_component_paths
    LIST_DIRECTORIES true
    CONFIGURE_DEPENDS
    "${BF2_ROOT}/*"
)

set(bf2_engine_sources)
set(bf2_engine_headers)
foreach(component_path IN LISTS bf2_component_paths)
    get_filename_component(component_name "${component_path}" NAME)
    # Memory is the recovered Memory.dll, its own image (bf2_memory, Code/BF2/Memory/CMakeLists.txt);
    # the engine keeps its own memory:: stubs, so its units must not join the engine source list.
    if(IS_DIRECTORY "${component_path}" AND NOT component_name STREQUAL "External"
       AND NOT component_name STREQUAL "Memory")
        file(GLOB_RECURSE component_sources
            CONFIGURE_DEPENDS
            "${component_path}/*.cpp"
        )
        file(GLOB_RECURSE component_headers
            CONFIGURE_DEPENDS
            "${component_path}/*.h"
            "${component_path}/*.hpp"
            "${component_path}/*.inl"
        )
        list(APPEND bf2_engine_sources ${component_sources})
        list(APPEND bf2_engine_headers ${component_headers})
    endif()
endforeach()
list(SORT bf2_engine_sources)
# [CHOSEN, not recovered] The CPU effect library has its own image and source manifest.
list(REMOVE_ITEM bf2_engine_sources
    "${BF2_ROOT}/Platform/Gfx/Effect/Cache.cpp"
    "${BF2_ROOT}/Platform/Gfx/Effect/Runtime.cpp"
    "${BF2_ROOT}/Platform/Gfx/Effect/D3DAsm.cpp"
    "${BF2_ROOT}/Platform/Gfx/Effect/FxBuild.cpp"
    "${BF2_ROOT}/Platform/Gfx/Effect/FxParser.cpp"
    "${BF2_ROOT}/Platform/Gfx/Effect/FxStates.cpp"
    "${BF2_ROOT}/Platform/Gfx/Effect/Native.cpp"
    "${BF2_ROOT}/Platform/Gfx/Effect/NativeMojo.cpp"
    "${BF2_ROOT}/Platform/Gfx/Effect/NativeVkd3d.cpp")
list(SORT bf2_engine_headers)

# These translation units belong to AIDLL_w32ded.dll, not bf2_w32ded.exe.  The source tree is
# intentionally broader than one image, so a recursive glob is only the discovery input; this
# manifest is the image boundary.  CommonSingletonServer.cpp STAYS in the engine link: its class is
# present in both shipped images (bf2 0x841B3C0, libaidll 0x2EB410) and engine code consumes it.
# Its AIDLL-only half WAS SPLIT OUT on 2026-09-08 into AI/AIDLL/AI/Main/AISingletonServer.cpp, which
# this list excludes - `AISingletonServer` exists only in libaidll.so.  The side-DLL target that the
# old note was waiting for is Code/Build/aidll.cmd plus bf2_aidll below.
# AIDLL_w32ded.dll / libaidll owns every TU under AI/AIDLL except the shared singleton bridge
# (the same directory rule as Code/Build/build.cmd), plus three units outside it. This list is an
# exclusion set for the engine image only, so its order is irrelevant; the DLL's own link order is
# the explicit bf2_aidll_sources manifest below.
file(GLOB_RECURSE bf2_aidll_only_sources CONFIGURE_DEPENDS "${BF2_ROOT}/AI/AIDLL/*.cpp")
list(REMOVE_ITEM bf2_aidll_only_sources "${BF2_ROOT}/AI/AIDLL/AI/Main/CommonSingletonServer.cpp")
list(APPEND bf2_aidll_only_sources
    "${BF2_ROOT}/Core/DllModule.cpp"
    "${BF2_ROOT}/Core/DllGlobals.cpp"                        # the side-DLL image's globals TU
    "${BF2_ROOT}/Math/DoubleVec3.cpp"
)
list(REMOVE_ITEM bf2_engine_sources ${bf2_aidll_only_sources})

# These translation units belong to BF2VoipServer_w32ded.dll, not bf2_w32ded.exe. The whole
# Code/BF2/VoipServer subtree is a THIRD server-side image (the bf2_voipserver target, below); the
# glob above discovers its units but they never link into the engine. Code/Build/build.cmd mirrors
# this with a directory rule. Add each VoipServer file here as it lands.
set(bf2_voipserver_only_sources
    "${BF2_ROOT}/VoipServer/VoipServer.cpp"
    "${BF2_ROOT}/VoipServer/VoipServerInstance.cpp"
    "${BF2_ROOT}/VoipServer/VoipRouter.cpp"
    "${BF2_ROOT}/VoipServer/VoipClients.cpp"
    "${BF2_ROOT}/VoipServer/VoipOutgoingChannels.cpp"
    "${BF2_ROOT}/VoipServer/VoipDebug.cpp"
)
list(REMOVE_ITEM bf2_engine_sources ${bf2_voipserver_only_sources})

# BF2VoipServerShell.cpp carries the standalone exe's main; it belongs only to bf2_voipserver_standalone.
list(REMOVE_ITEM bf2_engine_sources "${BF2_ROOT}/VoipServer/BF2VoipServerShell.cpp")

# CLIENT-ONLY translation units. A TU that exists only in BF2.exe belongs to the client image and must not
# enter bf2_w32ded - list it here, the same discipline as the AIDLL list above (Code/Build/build.cmd
# carries the mirror list). Client-only REGIONS of shared TUs are not a manifest matter: they sit
# under #if !DEDICATED, and bf2_set_image() below is what selects them per target.
set(bf2_client_only_sources
    "${BF2_ROOT}/Game/PunkBuster/PbClient.cpp" # BF2.exe 0x435900..0x436434; the ELF has only the server half
    "${BF2_ROOT}/Game/GameLogic/PlayerVOHelp.cpp" # BF2.exe client tutorial owner; placement inferred
    "${BF2_ROOT}/Math/Mat4Inverse.cpp" # client PE inverse; explicitly reused by x86 bf2_aidll below
    "${BF2_ROOT}/Game/Main/BF2Window.cpp"    # the game window (2026-09-02); its body is #if !DEDICATED as well
    "${BF2_ROOT}/Game/Main/BF2FrameInterpolator.cpp"    # attested client-only TU; ctor recovered 2026-09-03
    "${BF2_ROOT}/Game/Main/BF2Render.cpp"    # attested client-only renderer host; bootstrap recovered 2026-09-04
    "${BF2_ROOT}/Game/Main/BF2RenderConsole.cpp"    # [CHOSEN] file: BF2.exe's two renderer.* console bindings (2026-09-27)
    "${BF2_ROOT}/Game/SettingsConsole.cpp"    # [CHOSEN] file: the five profile-file console objects (2026-09-27)
    "${BF2_ROOT}/Menu/BetweenRoundsLogic.cpp" # attested client-only loading-screen menu logic
    "${BF2_ROOT}/Menu/BF2HudManager.cpp"    # attested client-only HUD construction; publication still gated
    "${BF2_ROOT}/Menu/BF2HudManagerConfiguration.cpp" # inferred split, recovered pointer/menu configuration
    "${BF2_ROOT}/Menu/BF2HudManagerState.cpp" # inferred split, recovered typed widget/state accessors
    "${BF2_ROOT}/Menu/BF2HudManagerChatInput.cpp" # inferred split, recovered edit/chat controls
    "${BF2_ROOT}/Menu/BF2HudManagerCamera.cpp" # commander camera/event wrappers; map owner still open
    "${BF2_ROOT}/Menu/BF2HudManagerOrders.cpp" # recovered order/request dispatch and replies
    "${BF2_ROOT}/Menu/BF2HudManagerRadio.cpp" # recovered radio opening and target classification
    "${BF2_ROOT}/Menu/BF2HudManagerAbilities.cpp" # recovered commander ability actions
    "${BF2_ROOT}/Menu/BF2HudManagerLifecycle.cpp" # recovered client HUD primary caller
    "${BF2_ROOT}/Menu/BF2HudManagerUpdate.cpp" # recovered client HUD primary caller
    "${BF2_ROOT}/Menu/BF2HudManagerPointer.cpp" # recovered client HUD primary caller
    "${BF2_ROOT}/Menu/BF2HudManagerPlayerFilter.cpp" # recovered client HUD primary caller
    "${BF2_ROOT}/Menu/BF2HudManagerGameFilter.cpp" # recovered client HUD primary caller
    "${BF2_ROOT}/Menu/BF2HudManagerInit.cpp" # recovered client HUD primary caller
    "${BF2_ROOT}/Menu/BF2HudManagerEvents.cpp"
    "${BF2_ROOT}/Menu/BF2HudManagerHudEvents.cpp"
    "${BF2_ROOT}/Menu/BF2HudManagerFrontiers.cpp"
    "${BF2_ROOT}/Menu/ClientConsoleModules.cpp"
    "${BF2_ROOT}/Menu/HudConsoleBindings.cpp"
    "${BF2_ROOT}/Menu/ProfileConsoleBindings.cpp"
    "${BF2_ROOT}/Menu/ClientUiFrontiers.cpp"
    "${BF2_ROOT}/Game/LocalProfileManager.cpp"
    "${BF2_ROOT}/Game/LocalProfileManagerInit.cpp"
    "${BF2_ROOT}/Game/LocalProfileManagerOperations.cpp"
    "${BF2_ROOT}/Game/LocalProfileManagerSelection.cpp"
    "${BF2_ROOT}/Game/LocalProfileManagerDiscovery.cpp"
    "${BF2_ROOT}/Game/LocalProfileManagerFields.cpp"
    "${BF2_ROOT}/Game/DistributedLightRenderer/DistributedLightRenderer.cpp"
    "${BF2_ROOT}/Game/VoipClient/BF2VoipClient.cpp"     # BF2.exe 0x4510F0..0x45235C
    "${BF2_ROOT}/Game/VoipClient/VoipClient.cpp"        # BF2.exe 0x736C30..0x737D4E
    "${BF2_ROOT}/Game/VoipClient/VoipMixer.cpp"         # BF2.exe 0x7356B0..0x736BA0
    "${BF2_ROOT}/Game/VoipClient/VoipMixerCoreAudio.cpp" # [PLATFORM] macOS backend; empty elsewhere
    "${BF2_ROOT}/Game/VoipClient/VoipFakeDevice.cpp"    # [CHOSEN] BF2_VOIP_CAPTURE=fake test device
    "${BF2_ROOT}/Game/LocalProfileManagerSuffix.cpp"
    "${BF2_ROOT}/Game/ProfileManager.cpp" # native online-profile lifetime/status owner
    "${BF2_ROOT}/Game/PasswordCodec.cpp" # inferred path; the profile password's CryptProtectData wrapper
    "${BF2_ROOT}/Game/AudioSettings.cpp"
    "${BF2_ROOT}/Menu/BF2HudManagerGameLogicEvents.cpp"
    "${BF2_ROOT}/Menu/HudAwardCatalogue.cpp"
    "${BF2_ROOT}/Menu/BfOccupiedVehicleData.cpp"
    "${BF2_ROOT}/Menu/BF2HudManagerSpotted.cpp" # spotted events and order coloring
    "${BF2_ROOT}/Menu/BF2HudManagerSpottedTarget.cpp" # terrain ray and native frustum target query
    "${BF2_ROOT}/Menu/BF2HudManagerCommander.cpp" # commander resignation and mutiny vote
    "${BF2_ROOT}/Menu/BF2HudManagerLists.cpp" # owned HUD list selection
    "${BF2_ROOT}/Menu/BF2HudManagerProjection.cpp" # screen/world transforms
    "${BF2_ROOT}/Menu/BF2HudManagerInput.cpp" # native input copy and pointer movement
    "${BF2_ROOT}/Menu/HudScoreboard.cpp" # scoreboard lifetime, lists and mute events
    "${BF2_ROOT}/Menu/Bf2MenuMapNode.cpp"     # native map storage and bounded helpers, unpublished
    "${BF2_ROOT}/Menu/HudBlinker.cpp"       # inferred path; recovered HUD blink controller and owner
    "${BF2_ROOT}/Menu/Bf2HudBuilder.cpp"    # attested path; split-node commands and lifetime
    "${BF2_ROOT}/Menu/Bf2SplitNode.cpp"
    "${BF2_ROOT}/Menu/Bf2TransformNode.cpp" # inferred path; constructible base of the HUD map node
    "${BF2_ROOT}/Menu/Bf2CullNode.cpp"
    "${BF2_ROOT}/Menu/Bf2PictureNode.cpp" # inferred path; recovered picture family trunk
    "${BF2_ROOT}/Menu/Bf2BarNode.cpp"
    "${BF2_ROOT}/Menu/Bf2CompassNode.cpp"
    "${BF2_ROOT}/Menu/Bf2EffectNode.cpp"
    "${BF2_ROOT}/Menu/Bf2BlendFuncEffect.cpp"
    "${BF2_ROOT}/Menu/Bf2ObjectMarkerNode.cpp"
    "${BF2_ROOT}/Menu/BfOccupiedVehicleNode.cpp"
    "${BF2_ROOT}/Menu/Bf2ObjectSelectionNode.cpp"
    "${BF2_ROOT}/Menu/Bf2HoverNode.cpp"
    "${BF2_ROOT}/Menu/Bf2ButtonNode.cpp"
    "${BF2_ROOT}/Menu/Bf2NewListBoxNode.cpp"
    "${BF2_ROOT}/Menu/Bf2EditNode.cpp"
    "${BF2_ROOT}/Menu/Bf2TileNode.cpp"
    "${BF2_ROOT}/Menu/Bf2MiniMenuMapNode.cpp"
    "${BF2_ROOT}/Menu/Bf2TransformListNode.cpp"
    "${BF2_ROOT}/External/BFMeme/SliderNode.cpp"
    "${BF2_ROOT}/Menu/BfFixedSliderNode.cpp"
    "${BF2_ROOT}/Menu/Bf2FixedSliderNode.cpp"
    "${BF2_ROOT}/Menu/Bf2CallFunctionAction.cpp"
    "${BF2_ROOT}/Menu/Bf2ActionListAction.cpp"
    "${BF2_ROOT}/External/BFMeme/CallFunctionAction.cpp"
    "${BF2_ROOT}/Menu/Bf2MoveEffect.cpp"
    "${BF2_ROOT}/Menu/Bf2ColorEffect.cpp"
    "${BF2_ROOT}/Menu/Bf2TextNode.cpp"
    "${BF2_ROOT}/Menu/Bf2Style.cpp"
    "${BF2_ROOT}/Menu/BfTransformNode.cpp"
    "${BF2_ROOT}/Menu/HudLoadingScreen.cpp" # inferred path; nonvirtual loading-screen data record
    "${BF2_ROOT}/Menu/HudRadio.cpp"         # inferred path; shared five-owner radio widget family
    "${BF2_ROOT}/Menu/HudChat.cpp"          # inferred path; two chat owners, complete declared interface
    "${BF2_ROOT}/Menu/HudColors.cpp"        # recovered mutable chat palettes
    "${BF2_ROOT}/Menu/HudKitSelection.cpp"  # recovered nonvirtual kit-selection record
    "${BF2_ROOT}/Menu/HudQuestionManager.cpp" # recovered question storage/queue; dispatch frontier open
    "${BF2_ROOT}/Menu/HudVoteMap.cpp"       # complete map-vote primary/event tables and lifetime
    "${BF2_ROOT}/Menu/HudVoip.cpp"          # the HUD-side VoIP list owner (voice client not built)
    "${BF2_ROOT}/Menu/HudMessages.cpp"      # complete item-selection message family
    "${BF2_ROOT}/Menu/HudCommander.cpp"     # commander selections/bindings/abilities; row helper open
    "${BF2_ROOT}/Menu/HudSquad.cpp"
    "${BF2_ROOT}/Menu/HudAmmo.cpp"
    "${BF2_ROOT}/Menu/HudVehicleInformation.cpp"
    "${BF2_ROOT}/Menu/HudVehicleFrontiers.cpp" # named diagnostic stops for that class's unrecovered leaves
    "${BF2_ROOT}/Menu/BfEditData.cpp"       # complete edit-data family and descriptor
    "${BF2_ROOT}/Menu/BfEditNode.cpp"       # recovered edit-node prefix; publication withheld
    "${BF2_ROOT}/Menu/BfNewListBoxNode.cpp" # recovered list-node storage; publication withheld
    "${BF2_ROOT}/Menu/BfListBoxData.cpp"    # descriptor-attested shared chat/list data; node hooks open
    "${BF2_ROOT}/Menu/Ref2System.cpp"       # recovered UI root construction; publication still gated
    "${BF2_ROOT}/Menu/HudWidescreen.cpp"    # issue #33: opt-in native vanilla HUD policy
    "${BF2_ROOT}/Menu/Font.cpp"
    "${BF2_ROOT}/Menu/DifFontMetadata.cpp"
    "${BF2_ROOT}/Menu/FontLoader.cpp"
    "${BF2_ROOT}/Menu/DifFontMetadataFile.cpp"
    "${BF2_ROOT}/Menu/HudInformationAdapter.cpp"
    "${BF2_ROOT}/External/BFMeme/System.cpp" # explicit external UI component; excluded from general discovery
    "${BF2_ROOT}/External/BFMeme/Object.cpp"
    "${BF2_ROOT}/External/BFMeme/ObjectCollection.cpp"
    "${BF2_ROOT}/External/BFMeme/Event.cpp"
    "${BF2_ROOT}/External/BFMeme/Action.cpp"
    "${BF2_ROOT}/External/BFMeme/Function.cpp"
    "${BF2_ROOT}/External/BFMeme/Tree.cpp"
    "${BF2_ROOT}/External/BFMeme/TypeDescriptor.cpp"
    "${BF2_ROOT}/External/BFMeme/NamedObjectRegistry.cpp"
    "${BF2_ROOT}/External/BFMeme/Data.cpp"
    "${BF2_ROOT}/External/BFMeme/Style.cpp"
    "${BF2_ROOT}/External/BFMeme/Effect.cpp"
    "${BF2_ROOT}/External/BFMeme/BlendFuncEffect.cpp"
    "${BF2_ROOT}/External/BFMeme/MoveEffect.cpp"
    "${BF2_ROOT}/External/BFMeme/ColorEffect.cpp"
    "${BF2_ROOT}/External/BFMeme/AlphaFadeEffect.cpp"
    "${BF2_ROOT}/External/BFMeme/VariableColorEffect.cpp"
    "${BF2_ROOT}/External/BFMeme/EffectNode.cpp"
    "${BF2_ROOT}/External/BFMeme/ShowEffectNode.cpp"
    "${BF2_ROOT}/External/BFMeme/Quad.cpp"
    "${BF2_ROOT}/External/BFMeme/Stream.cpp"
    "${BF2_ROOT}/External/BFMeme/OStream.cpp"
    "${BF2_ROOT}/External/BFMeme/IStream.cpp"
    "${BF2_ROOT}/External/BFMeme/FileIStream.cpp"
    "${BF2_ROOT}/External/BFMeme/FileOStream.cpp"
    "${BF2_ROOT}/External/BFMeme/ClassOStream.cpp"
    "${BF2_ROOT}/External/BFMeme/ClassIStream.cpp"
    "${BF2_ROOT}/External/BFMeme/NamedInputStream.cpp"
    "${BF2_ROOT}/External/BFMeme/Node.cpp"
    "${BF2_ROOT}/External/BFMeme/NameNode.cpp"
    "${BF2_ROOT}/External/BFMeme/NodeState.cpp"
    "${BF2_ROOT}/External/BFMeme/PictureNode.cpp"
    "${BF2_ROOT}/External/BFMeme/TransformNode.cpp"
    "${BF2_ROOT}/External/BFMeme/CullNode.cpp"
    "${BF2_ROOT}/External/BFMeme/ActionNode.cpp"
    "${BF2_ROOT}/External/BFMeme/CullVariableActionNode.cpp"
    "${BF2_ROOT}/External/BFMeme/BoolData.cpp"
    "${BF2_ROOT}/External/BFMeme/LogicalData.cpp"
    "${BF2_ROOT}/External/BFMeme/NotData.cpp"
    "${BF2_ROOT}/External/BFMeme/AndData.cpp"
    "${BF2_ROOT}/External/BFMeme/EqualData.cpp"
    "${BF2_ROOT}/External/BFMeme/OrData.cpp"
    "${BF2_ROOT}/External/BFMeme/FloatData.cpp"
    "${BF2_ROOT}/External/BFMeme/IntData.cpp"
    "${BF2_ROOT}/External/BFMeme/TextNode.cpp"
    "${BF2_ROOT}/External/BFMeme/ToggleData.cpp"
    "${BF2_ROOT}/External/BFMeme/SetVariableAction.cpp"
    "${BF2_ROOT}/External/BFMeme/SetVariableSoftAction.cpp"
    "${BF2_ROOT}/External/BFMeme/SetVariableSineAction.cpp"
    "${BF2_ROOT}/External/BFMeme/ObjectList.cpp"
    "${BF2_ROOT}/External/BFMeme/ActionListAction.cpp"
    "${BF2_ROOT}/External/BFMeme/SplitNode.cpp"
    "${BF2_ROOT}/External/BFMeme/Pointer.cpp"
    "${BF2_ROOT}/External/BFMeme/MovePointer.cpp"
    "${BF2_ROOT}/External/BFMeme/DynamicIndexPointer.cpp"
    "${BF2_ROOT}/External/BFMeme/EventQueue.cpp"
    "${BF2_ROOT}/External/BFMeme/InputNodeVisitors.cpp"
    "${BF2_ROOT}/Game/HudInformationLayer.cpp"
    "${BF2_ROOT}/IO/Input/KeyboardDevice.cpp"
    "${BF2_ROOT}/IO/Input/MouseDevice.cpp"
    "${BF2_ROOT}/IO/Input/GameControllerDevice.cpp"
    "${BF2_ROOT}/IO/Input/DX8InputDeviceManager.cpp"
    "${BF2_ROOT}/Sound/BfSound/MusicPlayer.cpp"    # attested client-only TU (2026-09-02); body #if !DEDICATED as well
    "${BF2_ROOT}/Menu/MenuTeamManager.cpp"
    "${BF2_ROOT}/Menu/MenuTeamManagerData.cpp"
    "${BF2_ROOT}/Menu/MenuTeamManagerConsole.cpp"
    "${BF2_ROOT}/Menu/SwiffHostConsole.cpp"
)
if(bf2_client_only_sources)
    list(REMOVE_ITEM bf2_engine_sources ${bf2_client_only_sources})
endif()

# WIN32-ONLY translation units (2026-09-29, the Linux port). The retail tree had one implementation
# per platform for these, selected by the project configuration: the PE attests
# Game/Main/StatusMonitorImpl_win32.cpp and IO/File/Win32FileSystem.cpp, the ELF attests
# IO/File/UnixFileSystem.cpp and StatusMonitorImpl_posix. The POSIX pair already compiles to an empty
# TU under MSVC by its own guards; the Win32 pair is whole-file Win32 API with no guard, so it leaves
# the manifest on Linux instead. Same discipline as the AIDLL and client lists above. macOS takes
# the POSIX pair as well ([CHOSEN, not recovered]: there was no retail macOS server).
set(bf2_win32_only_sources
    "${BF2_ROOT}/IO/File/Win32FileSystem.cpp"
    "${BF2_ROOT}/Game/Main/StatusMonitorImpl_win32.cpp"
)
if(BF2_BUILD_POSIX)
    list(REMOVE_ITEM bf2_engine_sources ${bf2_win32_only_sources})
endif()

# EndOfRoundInfo's shipped serializer uses compress2, so the server image needs the same bounded
# deflate/trees/compress closure already used by embedded Python, plus minizip's reader.
# Deliberately do not glob External or add minizip/zip.c.
set(bf2_zlib_sources
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3/adler32.c"
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3/crc32.c"
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3/inffast.c"
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3/inflate.c"
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3/inftrees.c"
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3/zutil.c"
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3/deflate.c"
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3/trees.c"
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3/compress.c"
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3/uncompr.c"
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3/contrib/minizip/ioapi.c"
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3/contrib/minizip/unzip.c"
)

list(LENGTH bf2_engine_sources bf2_engine_source_count)
list(LENGTH bf2_zlib_sources bf2_zlib_source_count)
math(EXPR bf2_total_source_count
    "${bf2_engine_source_count} + ${bf2_zlib_source_count}")
message(STATUS
    "BF2 engine-image sources: ${bf2_engine_source_count} C++ + "
    "${bf2_zlib_source_count} C = ${bf2_total_source_count}")

# ⚠⚠ /MD, NOT /MT - CORRECTED 2026-08-22, AND THE NAME IS NOW WRONG ON PURPOSE (kept so no call
# site churns). dice_py.dll is a real DLL built against the DYNAMIC CRT, and Code/Build/build.cmd
# has always compiled the whole image /MD for exactly that reason: ONE CRT per image, shared with
# the DLL. A /MT image gets a SECOND, private CRT - separate heap, separate FILE table - so a
# PyObject allocated inside dice_py and freed by the engine crosses heaps. This build had /MT.
function(bf2_use_static_msvc_runtime target)
    if(MSVC)
        set_property(TARGET "${target}" PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    endif()
endfunction()

function(bf2_add_compile_debug_info target)
    if(NOT MSVC)
        # GCC/Clang: DWARF in the objects, kept in the executable so a Linux crash backtrace names
        # the recovered function. Stripping for deployment is the deployer's choice, not the build's.
        target_compile_options("${target}" PRIVATE -g)
    elseif(BF2_BUILD_X64)
        # The x64 port uses embedded debug information so parallel Ninja compiles do not depend on
        # a shared mspdbsrv/PDB writer. Link-time /DEBUG still emits the image PDB.
        set_property(TARGET "${target}" PROPERTY MSVC_DEBUG_INFORMATION_FORMAT Embedded)
    else()
        target_compile_options("${target}" PRIVATE /Zi /FS)
    endif()
endfunction()

# =================================================================================================
# THE GCC/CLANG COMPILE CONTRACT FOR RECOVERED C++. Every flag
# below is [CHOSEN, not recovered]; each exists to keep GCC from exploiting undefined behaviour that
# MSVC never exploited, so that a body transcribed from the binaries keeps doing what the binary did:
#   -fno-strict-aliasing            recovered bodies type-pun through raw memory constantly;
#   -fwrapv                         hashes and counters rely on two's-complement wrap-around;
#   -fno-delete-null-pointer-checks retail null guards after a dereference must stay (AGENTS.md:
#                                   guards are preserved exactly, including ones GCC would fold);
#   -ffp-contract=off               no fused multiply-add: the x64 port's SSE2 evaluation is the
#                                   reference the Linux port is compared against;
#   -fno-omit-frame-pointer         usable crash backtraces on a headless server;
#   -fms-extensions                 the anonymous struct/union members the recovered layouts use;
#   -fpermissive, -Wno-narrowing    the tree was written under MSVC /permissive; GCC reports the
#                                   same constructs as errors. Kept as a build setting rather than
#                                   churning a thousand recovered files - see the handoff.
# =================================================================================================
function(bf2_configure_cpp_target target)
    target_compile_features("${target}" PRIVATE cxx_std_14)
    if(MSVC)
        target_compile_options("${target}" PRIVATE
            /permissive
            /EHsc
            /W3
            /wd4996
        )
    else()
        target_compile_options("${target}" PRIVATE
            -fno-strict-aliasing
            -fwrapv
            -fno-delete-null-pointer-checks
            -ffp-contract=off
            -fno-omit-frame-pointer
            -fms-extensions
            -fpermissive
            -Wno-narrowing
            -Wno-unknown-pragmas
            -Wno-multichar
            -Wno-deprecated-declarations
            -Wno-write-strings
            -pthread
        )
    endif()
    bf2_add_compile_debug_info("${target}")
    bf2_use_static_msvc_runtime("${target}")
endfunction()

# The vendored C (zlib, GameSpy): MSVC's /W3 /wd4996, or the equivalent tolerance for 2005-era C
# under a 2026 GCC. GCC 14 promotes implicit declarations and pointer-type mismatches to errors by
# default; that is the toolchain moving, not the SDK, so they go back to warnings here. GameSpy's
# own portability layer selects its Linux branch from _LINUX (gsPlatform.h, "Linux: _LINUX + _UNIX").
function(bf2_configure_c_target target)
    if(MSVC)
        # The SDK code stays as it ships. C5287: GameSpy compares two different enum types. On x64,
        # C4244/C4267 are GameSpy's string, packet and buffer lengths (size_t, pointer differences)
        # held in int or DWORD, and C4311/C4312 are buddy and block indices passed through a
        # callback's void* user data and back (gpiBuddy.c, gpiProfile.c). Both round-trip losslessly at
        # the sizes GameSpy handles, with the same 32-bit semantics as the x86 build.
        target_compile_options("${target}" PRIVATE /W3 /wd4996 /wd5287 /wd4244 /wd4267 /wd4311 /wd4312)
    else()
        target_compile_options("${target}" PRIVATE
            -fno-strict-aliasing
            -fwrapv
            -Wno-implicit-function-declaration
            -Wno-incompatible-pointer-types
            -Wno-int-conversion
            -Wno-implicit-int
            -Wno-deprecated-declarations
            -Wno-unknown-pragmas
            -pthread
        )
        if(BF2_BUILD_LINUX)
            target_compile_definitions("${target}" PRIVATE _LINUX _GNU_SOURCE)
        elseif(BF2_BUILD_MACOS)
            # [CHOSEN, not recovered] The SDK's own Darwin branch ("MacOSX: _MACOSX + _UNIX").
            target_compile_definitions("${target}" PRIVATE _MACOSX)
        endif()
    endif()
    bf2_add_compile_debug_info("${target}")
    bf2_use_static_msvc_runtime("${target}")
endfunction()

# The recovered C++'s MSVC spellings, mapped onto GCC/glibc (Core/PlatformCompat.h, a no-op under
# MSVC). Force-included like BuildConfig.h so no recovered file has to include it by name.
function(bf2_set_platform_compat target)
    if(NOT MSVC)
        target_compile_options("${target}" PRIVATE "-include${BF2_ROOT}/Core/PlatformCompat.h")
    endif()
endfunction()

# THE IMAGE DEFINE (2026-09-02). Every C++ compile of Code/BF2 states which image it is building:
# DEDICATED=1 for bf2_w32ded.exe and every server-side check, DEDICATED=0 for BF2.exe and the
# client-side DLL images. Code/BF2/Core/BuildConfig.h is force-included so that a target which forgets
# the define fails at once instead of silently compiling client regions (#if !DEDICATED) into the
# server. The retail client also keeps a RUNTIME isDedicated() check for its own `+dedicated 1` mode;
# the define only decides what is compiled at all. Code/Build/build.cmd and every
# Code/Tests/*.cmd gate pass the same two switches.
function(bf2_set_image target dedicated)
    target_compile_definitions("${target}" PRIVATE "DEDICATED=${dedicated}")
    if(MSVC)
        target_compile_options("${target}" PRIVATE "/FI${BF2_ROOT}/Core/BuildConfig.h")
    else()
        target_compile_options("${target}" PRIVATE "-include${BF2_ROOT}/Core/BuildConfig.h")
    endif()
endfunction()

# The embedded CPython's headers: Include/ plus the platform pyconfig.h directory. Windows uses the
# vendored PC/pyconfig.h; Linux uses the configure-generated pyconfig.h vendored under
# External/python-embedded/linux/ (a chosen path - see that directory's README); macOS uses the
# Darwin configure output vendored under External/python-embedded/macos/ ([CHOSEN, not recovered]).
if(BF2_BUILD_LINUX)
    set(BF2_PYTHON_INCLUDE_DIRS
        "${BF2_EXTERNAL_ROOT}/python-embedded/linux"
        "${BF2_EXTERNAL_ROOT}/python-embedded/py-src-2.3.4/Include"
    )
elseif(BF2_BUILD_MACOS)
    set(BF2_PYTHON_INCLUDE_DIRS
        "${BF2_EXTERNAL_ROOT}/python-embedded/macos"
        "${BF2_EXTERNAL_ROOT}/python-embedded/py-src-2.3.4/Include"
    )
else()
    set(BF2_PYTHON_INCLUDE_DIRS
        "${BF2_EXTERNAL_ROOT}/python-embedded/py-src-2.3.4/Include"
        "${BF2_EXTERNAL_ROOT}/python-embedded/py-src-2.3.4/PC"
    )
endif()

# The Linux port's system dependencies. ncurses is the retail dependency (the ELF imports initscr,
# newwin, wgetch, keypad and the rest for Game/Main/StatusMonitorImpl/posix.cpp); pthreads and dl
# are what Core/Thread.cpp, Core/Mutex.cpp and the POSIX plugin loader call.
if(BF2_BUILD_LINUX)
    find_package(Threads REQUIRED)
    # The wide-character ncurses (libncursesw) is the one every distribution installs by default;
    # the narrow libncurses.so.6 is a separate package on Debian-family systems and its dev symlink
    # is a linker script that fails when only the wide runtime exists. The status screen calls the
    # same functions in both.
    set(CURSES_NEED_NCURSES TRUE)
    set(CURSES_NEED_WIDE TRUE)
    find_package(Curses REQUIRED)
elseif(BF2_BUILD_MACOS)
    # [CHOSEN, not recovered] The macOS SDK's system ncurses (libncurses.tbd) carries the same
    # entry points; the SDK has no separate libncursesw, so the wide variant is not requested.
    find_package(Threads REQUIRED)
    set(CURSES_NEED_NCURSES TRUE)
    find_package(Curses REQUIRED)
endif()

add_library(bf2_engine_objects OBJECT
    ${bf2_engine_sources}
    ${bf2_engine_headers}
)
bf2_configure_cpp_target(bf2_engine_objects)
bf2_set_image(bf2_engine_objects 1)
bf2_set_platform_compat(bf2_engine_objects)
if(BF2_BUILD_X64 AND MSVC)
    # [CHOSEN, not recovered] Native-port diagnostics: make implicit narrowing a
    # build failure. Explicit fixed-width wire conversions remain reviewable in source.
    target_compile_options(bf2_engine_objects PRIVATE /Gy /we4244 /we4267 /we4311 /we4312)
endif()
if(BF2_BUILD_POSIX)
    target_include_directories(bf2_engine_objects PRIVATE ${CURSES_INCLUDE_DIRS})
    # The engine's own GameSpy-facing TUs (persistence, match-making, HTTP) include the SDK headers,
    # whose platform switch is _LINUX or _MACOSX (gsPlatform.h) - the same define
    # bf2_configure_c_target gives the SDK's C objects. BF2_PLATFORM_POSIX selects the engine's
    # POSIX implementations (UnixFileSystem, the ncurses status monitor) on every POSIX family
    # member, including macOS, whose compilers do not define __unix__.
    target_compile_definitions(bf2_engine_objects PRIVATE BF2_PLATFORM_POSIX)
    if(BF2_BUILD_LINUX)
        target_compile_definitions(bf2_engine_objects PRIVATE _LINUX)
    else()
        target_compile_definitions(bf2_engine_objects PRIVATE _MACOSX)
    endif()
endif()

# The client is a second compilation of the shared engine sources, not a relink of
# bf2_engine_objects: the latter has already had every #if !DEDICATED region compiled out.
# EXCLUDE_FROM_ALL keeps the normal server/archive build from compiling the tree twice; requesting
# bf2_client below pulls this object set in explicitly.
set(bf2_client_sources
    ${bf2_engine_sources}
    ${bf2_client_only_sources}
)
list(REMOVE_DUPLICATES bf2_client_sources)
list(SORT bf2_client_sources)
if(BF2_BUILD_MACOS)
    # [CHOSEN, not recovered] The macOS client replaces the DirectInput 8 devices with
    # MacInputDeviceManager and the Mac*Device TUs (docs/macos-s2-4.md). The Linux and MSVC client
    # manifests are unchanged.
    list(REMOVE_ITEM bf2_client_sources
        "${BF2_ROOT}/IO/Input/DX8InputDeviceManager.cpp"
        "${BF2_ROOT}/IO/Input/KeyboardDevice.cpp"
        "${BF2_ROOT}/IO/Input/MouseDevice.cpp"
        "${BF2_ROOT}/IO/Input/GameControllerDevice.cpp")
endif()
add_library(bf2_client_objects OBJECT EXCLUDE_FROM_ALL
    ${bf2_client_sources}
    ${bf2_engine_headers}
)
bf2_configure_cpp_target(bf2_client_objects)
bf2_set_image(bf2_client_objects 0)
bf2_set_platform_compat(bf2_client_objects)
# This is the native rebuilt-client side of the plugin ABI.  The DLL and executable must use the
# same MSVC STL layout even when this CMake tree is configured as Debug; iterator debugging changes
# container layout and cannot cross the image boundary.  PluginAbi.h enforces the matching contract.
target_compile_definitions(bf2_client_objects PRIVATE
    _ITERATOR_DEBUG_LEVEL=0
)
# [CHOSEN, not recovered] The native x64 client (https://github.com/B2BF/OpenBF2/issues/7): a pointer
# truncated into a 32-bit integer fails the build, as in the plugin scripts' x64 mode. The editor's
# own objects and its second compilation of shared TUs take the same list.
set(BF2_CLIENT_X64_COMPILE_OPTIONS)
if(BF2_BUILD_X64 AND MSVC)
    set(BF2_CLIENT_X64_COMPILE_OPTIONS /Gy /we4311 /we4312)
endif()
target_compile_options(bf2_client_objects PRIVATE ${BF2_CLIENT_X64_COMPILE_OPTIONS})
target_include_directories(bf2_client_objects PRIVATE
    ${BF2_PYTHON_INCLUDE_DIRS}
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3"
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3/contrib/minizip"
)
if(BF2_BUILD_MACOS)
    # [CHOSEN, not recovered] The macOS client (docs/macos-port.md, "Client triage") compiles the
    # shared engine sources with the platform switches the macOS server objects use:
    # BF2_PLATFORM_POSIX selects the POSIX file system and status monitor, _MACOSX the GameSpy
    # SDK's Darwin branch, BF2_STATIC_PLUGINS the statically linked plugin images
    # (Code/StaticPlugins). The Linux and MSVC client objects are unchanged.
    target_include_directories(bf2_client_objects PRIVATE ${CURSES_INCLUDE_DIRS})
    target_compile_definitions(bf2_client_objects PRIVATE BF2_PLATFORM_POSIX _MACOSX BF2_STATIC_PLUGINS=1)
endif()
source_group(TREE "${BF2_ROOT}" PREFIX "BF2" FILES
    ${bf2_client_sources}
    ${bf2_engine_headers}
)
# ⚠⚠ Py_NO_ENABLE_SHARED IS DELIBERATELY NOT DEFINED - REMOVED 2026-08-22. It used to be, and it
# was the reason python::sgl_setParam crashed in this build while the same source ran under
# Code/Build/build.cmd, which has never defined it.
#
# ★ WHY IT BREAKS ONLY *DATA*: with the macro defined, PyAPI_FUNC/PyAPI_DATA drop
# __declspec(dllimport). Function references survive that, because an import library also supplies
# a short-name thunk for every function - so PyErr_SetString still linked. DATA has no thunk, so
# PyExc_RuntimeError and _Py_NoneStruct went unresolved and /FORCE gave them garbage addresses.
# The result is a PyErr_SetString called with a junk exception object (which reads like a random
# string, because that is whatever the junk address happens to point at) and a Py_INCREF(Py_None)
# writing into unrelated memory - the latter in all fifty remaining host-module stubs too.
#
# `dumpbin /IMPORTS:dice_py.dll` is the check: the image MUST import PyExc_RuntimeError and
# _Py_NoneStruct, not just PyErr_SetString.
# ⚠ It must also match how dice_py itself was built - see Code/Build/python/build_python.cmd.
# The embedded CPython 2.3.4 sources live under External/python-embedded/py-src-2.3.4 (the layout
# dice_py.dll's PDB attests - Code/Build/python/build_python.cmd); build.cmd's PYINC names the
# same two directories. (This used to say External/Python-2.3.4, a path that does not exist.)
target_include_directories(bf2_engine_objects PRIVATE
    ${BF2_PYTHON_INCLUDE_DIRS}
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3"
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3/contrib/minizip"
)
source_group(TREE "${BF2_ROOT}" PREFIX "BF2" FILES
    ${bf2_engine_sources}
    ${bf2_engine_headers}
)

# =================================================================================================
# TinyXML - ADDED 2026-08-22, and it was a BUILD-PARITY BUG, not a new dependency. Code/Build/
# build.cmd has compiled these four since ModInfo's two .desc readers were written; this file never
# did, so the CMake bf2_w32ded.exe /FORCE-resolved every TiXml call to RVA 0 and died inside
# Game/Utility/XMLDocument.cpp the moment ModInfo::parseModDesc touched it - while the build.cmd
# image walked straight past into the level load. Two build systems, two different first faults.
#
# ⚠ TIXML_USE_STL IS DELIBERATELY NOT DEFINED, exactly as build.cmd documents: the binary uses
# TiXmlString/StringToBuffer rather than std::string, which is the non-STL build. Defining it
# changes every signature and the recovered wrapper stops matching.
# ⚠ xmltest.cpp is upstream's TEST DRIVER and carries its own main(); it is not part of the image.
set(bf2_tinyxml_sources
    "${BF2_EXTERNAL_ROOT}/TinyXML/tinystr.cpp"
    "${BF2_EXTERNAL_ROOT}/TinyXML/tinyxml.cpp"
    "${BF2_EXTERNAL_ROOT}/TinyXML/tinyxmlerror.cpp"
    "${BF2_EXTERNAL_ROOT}/TinyXML/tinyxmlparser.cpp"
)
add_library(bf2_tinyxml_objects OBJECT ${bf2_tinyxml_sources})
bf2_configure_cpp_target(bf2_tinyxml_objects)
bf2_set_image(bf2_tinyxml_objects 1)
target_include_directories(bf2_tinyxml_objects PRIVATE "${BF2_EXTERNAL_ROOT}/TinyXML")
source_group(TREE "${BF2_EXTERNAL_ROOT}/TinyXML"
    PREFIX "External/TinyXML"
    FILES ${bf2_tinyxml_sources}
)

add_library(bf2_zlib_objects OBJECT ${bf2_zlib_sources})
bf2_set_image(bf2_zlib_objects 1)
bf2_configure_c_target(bf2_zlib_objects)
target_include_directories(bf2_zlib_objects PRIVATE
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3"
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3/contrib/minizip"
)
source_group(TREE "${BF2_EXTERNAL_ROOT}/zlib-1.2.3"
    PREFIX "External/zlib-1.2.3"
    FILES ${bf2_zlib_sources}
)

# =================================================================================================
# bf2_gamespy_objects - the GameSpy SDK modules the recovered MatchMaking / CD-key backend links
# against (2026-08-24). Stock upstream C, LINKED not reversed (AGENTS.md) - the SAME file set
# Code/Build/build.cmd compiles (build.cmd:146): gcdkey + qr2 + ghttp + gsAvailable + the common
# runtime + natneg. Without these, GameSpyBackend's ctor calls (GSIStartAvailableCheckA, qr2_initA,
# gcd_init_qr2) resolve to RVA 0 under /FORCE and the server DEP-faults the instant
# MatchMakingServer::initializeServices stands the backend up. build.cmd already links these; only
# the CMake executable was missing them. ⚠ /MD (MultiThreadedDLL) to match the engine's one CRT per
# image - the global default is /MT, so it MUST be overridden here as it is for the C++ targets.
# ⚠ The .c files use relative quote-includes into their own SDK tree, so no -I is needed (build.cmd's
# GameSpy cl line carries none either).
# =================================================================================================
set(bf2_gamespy_sources
    "${BF2_EXTERNAL_ROOT}/GameSpy/gcdkey/gcdkeys.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/darray.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/common/gsAvailable.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/common/gsPlatformUtil.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/common/gsCore.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/common/gsMemory.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/common/gsDebug.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/common/gsAssert.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/common/gsPlatform.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/common/gsPlatformSocket.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/common/gsCrypt.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/common/gsRC4.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/common/gsSHA1.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/common/gsLargeInt.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/common/gsPlatformThread.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/common/gsXML.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/common/gsSSL.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/common/gsStringUtil.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/md5c.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gstats/gstats.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gstats/gbucket.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/qr2/qr2.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/qr2/qr2regkeys.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/hashtable.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/ghttp/ghttpBuffer.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/ghttp/ghttpCallbacks.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/ghttp/ghttpCommon.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/ghttp/ghttpConnection.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/ghttp/ghttpEncryption.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/ghttp/ghttpMain.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/ghttp/ghttpPost.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/ghttp/ghttpProcess.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/natneg/natneg.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/natneg/NATify.c"
)
add_library(bf2_gamespy_objects OBJECT ${bf2_gamespy_sources})
bf2_set_image(bf2_gamespy_objects 1)
bf2_configure_c_target(bf2_gamespy_objects)
source_group(TREE "${BF2_EXTERNAL_ROOT}/GameSpy"
    PREFIX "External/GameSpy"
    FILES ${bf2_gamespy_sources}
)

# =================================================================================================
# bf2_gamespy_sb_objects - the GameSpy server-browsing SDK the CLIENT's ServerBrowser class wraps
# (Menu/ServerBrowser.cpp, 2026-09-08): ServerBrowserNew / Update / LANUpdate / AuxUpdate / Sort /
# Think and the SBServer key readers. Stock upstream C, LINKED not reversed (AGENTS.md), the same
# options as bf2_gamespy_objects. Client images only: the dedicated server never browses, and the
# PE that ships these functions is BF2.exe. natneg (the master list's NAT negotiation), darray,
# hashtable, qr2regkeys and gsAvailable come from bf2_gamespy_objects.
# =================================================================================================
set(bf2_gamespy_sb_sources
    "${BF2_EXTERNAL_ROOT}/GameSpy/serverbrowsing/sb_serverbrowsing.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/serverbrowsing/sb_serverlist.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/serverbrowsing/sb_queryengine.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/serverbrowsing/sb_server.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/serverbrowsing/sb_crypt.c"
)
add_library(bf2_gamespy_sb_objects OBJECT ${bf2_gamespy_sb_sources})
bf2_configure_c_target(bf2_gamespy_sb_objects)
source_group(TREE "${BF2_EXTERNAL_ROOT}/GameSpy"
    PREFIX "External/GameSpy"
    FILES ${bf2_gamespy_sb_sources}
)

# =================================================================================================
# bf2_gamespy_cdkeyc_objects - the CLIENT half of the GameSpy CD-key SDK (2026-09-14). One file, one
# function: gcd_compute_response, which GameClient::handleChallenge (PE 4D1CF0 -> 5C0050) calls to
# build the 73-byte answer to a server's ChallengeEvent. Stock upstream C, LINKED not reversed
# (AGENTS.md) - the same rule gcdkeys.c (the SERVER half, in bf2_gamespy_objects) already follows.
#
# ⚠ ITS OWN TARGET RATHER THAN A ROW IN bf2_gamespy_sources, because the split is real: BF2.exe has
# gcd_compute_response and bf2_w32ded.exe does not. Client images only, exactly like the sb and gp
# sets below/above. MD5Digest comes from md5c.c in bf2_gamespy_objects.
# =================================================================================================
set(bf2_gamespy_cdkeyc_sources
    "${BF2_EXTERNAL_ROOT}/GameSpy/gcdkey/gcdkeyc.c"
)
add_library(bf2_gamespy_cdkeyc_objects OBJECT ${bf2_gamespy_cdkeyc_sources})
bf2_configure_c_target(bf2_gamespy_cdkeyc_objects)
source_group(TREE "${BF2_EXTERNAL_ROOT}/GameSpy"
    PREFIX "External/GameSpy"
    FILES ${bf2_gamespy_cdkeyc_sources}
)

# =================================================================================================
# bf2_gamespy_voice_objects - GameSpy Voice 2 and its Speex 1.0.5 codec, statically linked in BF2.exe
# (the client voice client, Game/VoipClient). Stock upstream C, LINKED not reversed: the Voice2 core,
# the platform's hardware layer (DirectSound on Windows, as retail; the SDK's CoreAudio layer on macOS)
# and the custom-device file the test device builds on. Speex: only the libspeex files the narrowband
# and wideband codecs need - no SLOW_TRIG, no tools (External/speex-1.0.5/LOCAL_CHANGES.md).
# Client images only.
# =================================================================================================
set(bf2_gamespy_voice_sources
    "${BF2_EXTERNAL_ROOT}/GameSpy/Voice2/gvMain.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/Voice2/gvSource.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/Voice2/gvFrame.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/Voice2/gvCodec.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/Voice2/gvSpeex.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/Voice2/gvDevice.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/Voice2/gvUtil.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/Voice2/gvCustomDevice.c"
)
if(WIN32)
    list(APPEND bf2_gamespy_voice_sources "${BF2_EXTERNAL_ROOT}/GameSpy/Voice2/gvDirectSound.c")
elseif(APPLE)
    list(APPEND bf2_gamespy_voice_sources "${BF2_EXTERNAL_ROOT}/GameSpy/Voice2/gvOSXAudio.c")
endif()
foreach(speex_unit bits cb_search exc_10_16_table exc_10_32_table exc_20_32_table exc_5_256_table
        exc_5_64_table exc_8_128_table filters gain_table gain_table_lbr hexc_10_32_table hexc_table
        high_lsp_tables lpc lsp lsp_tables_nb ltp misc modes nb_celp quant_lsp sb_celp speex_callbacks
        vbr vq)
    list(APPEND bf2_gamespy_voice_sources "${BF2_EXTERNAL_ROOT}/speex-1.0.5/libspeex/${speex_unit}.c")
endforeach()
# Client/test dependencies opt in; Linux dedicated builds have no Voice2 hardware backend.
add_library(bf2_gamespy_voice_objects OBJECT EXCLUDE_FROM_ALL ${bf2_gamespy_voice_sources})
bf2_configure_c_target(bf2_gamespy_voice_objects)
target_include_directories(bf2_gamespy_voice_objects PRIVATE
    "${BF2_EXTERNAL_ROOT}/speex-1.0.5/include"
    "${BF2_EXTERNAL_ROOT}/speex-1.0.5/include/speex")    # gvSpeex.c includes <speex.h>
source_group(TREE "${BF2_EXTERNAL_ROOT}"
    PREFIX "External"
    FILES ${bf2_gamespy_voice_sources}
)

# Client ProfileManager uses the stock presence SDK. Source set follows the
# upstream gp/gptestc project, excluding its sample and already shared common TUs.
set(bf2_gamespy_gp_sources
    "${BF2_EXTERNAL_ROOT}/GameSpy/gp/gp.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gp/gpi.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gp/gpiBuddy.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gp/gpiBuffer.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gp/gpiCallback.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gp/gpiConnect.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gp/gpiInfo.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gp/gpiKeys.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gp/gpiOperation.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gp/gpiPeer.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gp/gpiProfile.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gp/gpiSearch.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gp/gpiTransfer.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gp/gpiUnique.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gp/gpiUtility.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/common/gsUdpEngine.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gt2/gt2Auth.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gt2/gt2Buffer.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gt2/gt2Callback.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gt2/gt2Connection.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gt2/gt2Encode.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gt2/gt2Filter.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gt2/gt2Main.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gt2/gt2Message.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gt2/gt2Socket.c"
    "${BF2_EXTERNAL_ROOT}/GameSpy/gt2/gt2Utility.c"
)
add_library(bf2_gamespy_gp_objects OBJECT ${bf2_gamespy_gp_sources})
bf2_set_image(bf2_gamespy_gp_objects 0)
bf2_configure_c_target(bf2_gamespy_gp_objects)
source_group(TREE "${BF2_EXTERNAL_ROOT}/GameSpy"
    PREFIX "External/GameSpy"
    FILES ${bf2_gamespy_gp_sources}
)

# The final executable is intentionally not link-complete yet. A static archive
# gives CLion a normal green build while retaining every unresolved reference.
add_library(bf2_recovered STATIC
    $<TARGET_OBJECTS:bf2_engine_objects>
    $<TARGET_OBJECTS:bf2_zlib_objects>
    $<TARGET_OBJECTS:bf2_tinyxml_objects>
    $<TARGET_OBJECTS:bf2_gamespy_objects>
)
set_target_properties(bf2_recovered PROPERTIES
    LINKER_LANGUAGE CXX
    MSVC_RUNTIME_LIBRARY MultiThreaded
    OUTPUT_NAME bf2_recovered
    FOLDER "BF2"
)
set_target_properties(bf2_engine_objects bf2_zlib_objects bf2_tinyxml_objects
    PROPERTIES FOLDER "BF2")

# =================================================================================================
# bf2_w32ded / bf2_w64ded - THE DEDICATED SERVER EXECUTABLES. CMake architecture is selected for
# the whole build tree, so the msvc-x86 configure exposes bf2_w32ded and the separate msvc-x64
# configure exposes bf2_w64ded. They never consume one another's object files or dice_py binaries.
# The x86 image remains the retail-fidelity target. The x64 image is a native portability target;
# it links strictly and rejects unresolved symbols.
#
# ★ THE ENTRY POINT IS THE PRODUCTION `main` IN Game/Main/BF2.cpp (Win32 branch, 2026-08-24), NOT the
# bootmain harness. It runs the real BF2::init -> `while (bf2->run())` loop UNBOUNDED and then
# `delete bf2` for the clean shutdown, with no frame bound and no probes - so this exe boots the
# server as production, whether or not the working directory is the game folder. The diagnostic
# harness (bounded frames, object probes, RVA-printing fault handler) lives in Code/Build/bootmain.cpp
# and is reached only through Code/Build/bootrun.cmd; it is deliberately NOT linked here.
#
# Both Windows executables link strictly: zero unresolved symbols were measured in
# the x86 companion with MSVC 14.44 and the matching dice_py.lib.
# Side-DLL link contracts and runtime frontiers remain independent of the executable's link.
#
# OUTPUT: <BF2_DEDICATED_OUTPUT_DIR>/<selected target>.exe. The default is inside that architecture's
# build tree, NEVER the game folder: D:/B2BF Launcher/Battlefield2/bf2_w32ded.exe is the ORIGINAL
# reference PE and must not be overwritten. To run against game content, use the selected target's
# `_run` target or checked-in `(game dir)` configuration; those change only the working directory.
# The production exe takes the SAME command line the retail server does (forward it via
# BF2_RUN_ARGS, e.g. "+modPath;mods/bf2;+config;<...>;+mapList;<...>"); the harness-only env knobs
# (BF2_FRAMES, BF2_HARNESS_PROBES, BF2_NET_TX_TEST, ...) are read by bootmain, not by this exe.
# =================================================================================================
if(WIN32)
    set(BF2_GAME_DIR_DEFAULT "D:/B2BF Launcher/Battlefield2")
elseif(DEFINED ENV{BF2_GAME_DIR})
    set(BF2_GAME_DIR_DEFAULT "$ENV{BF2_GAME_DIR}")
else()
    # The owner's WSL view of the same install; any other Linux box passes -DBF2_GAME_DIR=<root>.
    set(BF2_GAME_DIR_DEFAULT "/mnt/d/B2BF Launcher/Battlefield2")
endif()
set(BF2_GAME_DIR "${BF2_GAME_DIR_DEFAULT}" CACHE PATH
    "The Battlefield 2 install the dedicated server runs against (mods/, Levels/ ...)")
if(BF2_BUILD_LINUX)
    # The Linux port. `bf2` is the retail Linux server's executable name; libdice_py.so is built
    # from the same CPython 2.3.4 tree by External/python-embedded/linux/CMakeLists.txt into the
    # same directory, where the executable's $ORIGIN rpath finds it.
    set(BF2_DEDICATED_TARGET "bf2")
    set(BF2_DEDICATED_OUTPUT_NAME "bf2")
    set(BF2_DEDICATED_DEFAULT_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin_linux")
    set(BF2_DICE_PY_DEFAULT_DIR "")
    set(BF2_DICE_PY_BUILD_HINT "libdice_py.so is a CMake target (dice_py) on Linux")
elseif(BF2_BUILD_MACOS)
    # [CHOSEN, not recovered] The macOS port keeps the Linux names: `bf2` beside libdice_py.dylib,
    # found through an @loader_path rpath.
    set(BF2_DEDICATED_TARGET "bf2")
    set(BF2_DEDICATED_OUTPUT_NAME "bf2")
    set(BF2_DEDICATED_DEFAULT_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin_macos")
    set(BF2_DICE_PY_DEFAULT_DIR "")
    set(BF2_DICE_PY_BUILD_HINT "libdice_py.dylib is a CMake target (dice_py) on macOS")
elseif(BF2_BUILD_X64)
    set(BF2_DEDICATED_TARGET "bf2_w64ded")
    set(BF2_DEDICATED_OUTPUT_NAME "bf2_w64ded")
    set(BF2_DEDICATED_DEFAULT_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin_w64ded")
    # [CHOSEN, not recovered] Separate from the attested x86 OBJ/python/release directory so an x64
    # link can never silently consume the working x86 import library or stage its DLL.
    set(BF2_DICE_PY_DEFAULT_DIR
        "${BF2_EXTERNAL_ROOT}/python-embedded/OBJ/python/x64/release")
    set(BF2_DICE_PY_BUILD_HINT
        "Build the native x64 dice_py pair described in docs/building.md")
else()
    set(BF2_DEDICATED_TARGET "bf2_w32ded")
    set(BF2_DEDICATED_OUTPUT_NAME "bf2_w32ded")
    set(BF2_DEDICATED_DEFAULT_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
    set(BF2_DICE_PY_DEFAULT_DIR
        "${BF2_EXTERNAL_ROOT}/python-embedded/OBJ/python/release")
    set(BF2_DICE_PY_BUILD_HINT "Run Code/Build/python/build_python.cmd")
endif()
set(BF2_DEDICATED_OUTPUT_DIR "${BF2_DEDICATED_DEFAULT_OUTPUT_DIR}" CACHE PATH
    "Where ${BF2_DEDICATED_OUTPUT_NAME}.exe is written (never the retail game directory)")
if(BF2_BUILD_POSIX)
    # libdice_py.so/.dylib - the embedded CPython 2.3.4 as a shared library, built beside `bf2`.
    # One build description serves both POSIX ports; it selects the platform's pyconfig.h itself.
    set(BF2_DICE_PY_OUTPUT_DIR "${BF2_DEDICATED_OUTPUT_DIR}")
    if(EXISTS "${BF2_EXTERNAL_ROOT}/python-embedded/linux/CMakeLists.txt")
        add_subdirectory("${BF2_EXTERNAL_ROOT}/python-embedded/linux" "${CMAKE_BINARY_DIR}/dice_py")
    else()
        message(WARNING
            "External/python-embedded/linux/CMakeLists.txt is missing: the dice_py target is not "
            "defined and `bf2` cannot link. See docs/building.md.")
    endif()
else()
    set(BF2_DICE_PY_LIB "${BF2_DICE_PY_DEFAULT_DIR}/dice_py.lib" CACHE FILEPATH
        "Architecture-matched dice_py import library")
    set(BF2_DICE_PY_DLL "${BF2_DICE_PY_DEFAULT_DIR}/dice_py.dll" CACHE FILEPATH
        "Architecture-matched dice_py runtime DLL")
endif()

add_executable("${BF2_DEDICATED_TARGET}"
    $<TARGET_OBJECTS:bf2_engine_objects>
    $<TARGET_OBJECTS:bf2_zlib_objects>
    $<TARGET_OBJECTS:bf2_tinyxml_objects>
    $<TARGET_OBJECTS:bf2_gamespy_objects>
)
# ★ THE ENTRY POINT IS Game/Main/BF2.cpp's PRODUCTION `main` (2026-08-24), inside bf2_engine_objects -
# NOT Code/Build/bootmain.cpp. bootmain is the diagnostic HARNESS (bounded frames, probes, fault
# handler) and stays with bootrun.cmd; the CMake executable boots the real server unbounded via
# BF2::init -> `while (bf2->run())`, exactly as production. See the block over that main in BF2.cpp.
# The target is built purely from object libraries, so its linker language is stated explicitly.
set_target_properties("${BF2_DEDICATED_TARGET}" PROPERTIES LINKER_LANGUAGE CXX)
bf2_configure_cpp_target("${BF2_DEDICATED_TARGET}")
set_target_properties("${BF2_DEDICATED_TARGET}" PROPERTIES
    OUTPUT_NAME "${BF2_DEDICATED_OUTPUT_NAME}"
    RUNTIME_OUTPUT_DIRECTORY "${BF2_DEDICATED_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${BF2_DEDICATED_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${BF2_DEDICATED_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RECONSTRUCTION "${BF2_DEDICATED_OUTPUT_DIR}"
    PDB_OUTPUT_DIRECTORY "${BF2_DEDICATED_OUTPUT_DIR}"
    FOLDER "BF2"
)
if(BF2_BUILD_POSIX)
    if(TARGET dice_py)
        target_link_libraries("${BF2_DEDICATED_TARGET}" PRIVATE dice_py)
    endif()
    # The Linux image statically links the voice-server module (the win32 image is the side DLL).
    # Its MD5Digest dependency arrives through bf2_gamespy_objects, which md5c.c is already part of.
    target_sources("${BF2_DEDICATED_TARGET}" PRIVATE
        ${bf2_voipserver_only_sources}
    )
elseif(EXISTS "${BF2_DICE_PY_LIB}")
    target_link_libraries("${BF2_DEDICATED_TARGET}" PRIVATE "${BF2_DICE_PY_LIB}")
else()
    message(WARNING
        "${BF2_DEDICATED_TARGET}: ${BF2_DICE_PY_LIB} is missing. "
        "${BF2_DICE_PY_BUILD_HINT}.")
endif()
if(BF2_BUILD_X64 AND NOT BF2_BUILD_LINUX)
    # [CHOSEN, not recovered] The native Windows x64 port links the voice-server module statically,
    # like the Linux image (which adds the same sources above). The BF2VoipServer_w32ded.dll side
    # image (bf2_voipserver, below) is x86-only: it is defined under NOT BF2_BUILD_X64 and its .def
    # pins x86 decorated names. Without these sources the strict x64 link has no CBF2VoipServer
    # definitions for VoipServerInternal.
    target_sources("${BF2_DEDICATED_TARGET}" PRIVATE
        ${bf2_voipserver_only_sources}
    )
endif()
# dice_py IS A REAL DLL, so linking dice_py.lib is only half the job - the DLL has to be FOUND at
# run time. Windows searches the executable's own directory before the working directory, and the
# run target deliberately sets the working directory to the GAME folder, so the only place that
# works for both is beside the exe. Code/Build/bootrun.cmd has staged it since it was written;
# this target did not, which is why a CMake/CLion build produced a bf2_w32ded.exe that died on a
# missing dice_py before reaching main.
#
# copy_if_different so a rebuild does not touch the file when nothing changed.
# NOTE: the guard is evaluated at CONFIGURE time, exactly like the .lib check below it. If the DLL
# is built later by Code/Build/python/build_python.cmd, re-run CMake once so this rule appears.
if(BF2_BUILD_POSIX)
    # Nothing to stage: the dice_py target writes libdice_py.so into BF2_DEDICATED_OUTPUT_DIR, and
    # the executable carries an $ORIGIN rpath (below) so the pair can be copied anywhere together.
elseif(EXISTS "${BF2_DICE_PY_DLL}")
    add_custom_command(TARGET "${BF2_DEDICATED_TARGET}" POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${BF2_DICE_PY_DLL}" "${BF2_DEDICATED_OUTPUT_DIR}/dice_py.dll"
        COMMENT "Staging the ${BF2_NATIVE_ARCH} dice_py.dll beside ${BF2_DEDICATED_OUTPUT_NAME}.exe"
        VERBATIM
    )
else()
    message(WARNING
        "${BF2_DEDICATED_TARGET}: ${BF2_DICE_PY_DLL} is missing - no runtime DLL will be staged. "
        "${BF2_DICE_PY_BUILD_HINT}, then re-run CMake.")
endif()

if(BF2_BUILD_LINUX)
    # [CHOSEN, not recovered] The Linux image's system libraries - the families the retail ELF's
    # DT_NEEDED list names (libpthread, libdl, libm, libncurses, libdice_py), spelled through CMake.
    target_link_libraries("${BF2_DEDICATED_TARGET}" PRIVATE
        Threads::Threads ${CMAKE_DL_LIBS} m ${CURSES_LIBRARIES})
    # libdice_py.so is resolved beside the executable ($ORIGIN), wherever the pair is copied - the
    # Linux counterpart of staging dice_py.dll beside the exe above.
    set_target_properties("${BF2_DEDICATED_TARGET}" PROPERTIES
        BUILD_RPATH "\$ORIGIN"
        INSTALL_RPATH "\$ORIGIN"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
    # libstdc++/libgcc are linked statically by default so a server built on a newer distribution
    # still runs on an older one; glibc itself stays dynamic (it cannot sensibly be static).
    option(BF2_LINUX_STATIC_RUNTIME
        "Link libstdc++ and libgcc statically into bf2 (portability across distributions)" ON)
    if(BF2_LINUX_STATIC_RUNTIME)
        target_link_options("${BF2_DEDICATED_TARGET}" PRIVATE -static-libstdc++ -static-libgcc)
    endif()
    target_link_options("${BF2_DEDICATED_TARGET}" PRIVATE
        -pthread
        "-Wl,-Map=${BF2_DEDICATED_OUTPUT_DIR}/${BF2_DEDICATED_OUTPUT_NAME}.map"
    )
elseif(BF2_BUILD_MACOS)
    # [CHOSEN, not recovered] The macOS counterparts of the Linux link contract above: libSystem
    # supplies pthreads, dl and libm; ncurses comes from the SDK. libdice_py.dylib is resolved
    # beside the executable through @loader_path, the Mach-O spelling of $ORIGIN. ld64 rejects
    # undefined symbols by default, so this link is strict like the Linux one. There is no
    # static-runtime option: libc++ is part of the OS on macOS.
    target_link_libraries("${BF2_DEDICATED_TARGET}" PRIVATE
        Threads::Threads ${CMAKE_DL_LIBS} ${CURSES_LIBRARIES})
    set_target_properties("${BF2_DEDICATED_TARGET}" PROPERTIES
        BUILD_RPATH "@loader_path"
        INSTALL_RPATH "@loader_path"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
    target_link_options("${BF2_DEDICATED_TARGET}" PRIVATE
        "-Wl,-map,${BF2_DEDICATED_OUTPUT_DIR}/${BF2_DEDICATED_OUTPUT_NAME}.map"
    )
else()
target_link_libraries("${BF2_DEDICATED_TARGET}" PRIVATE advapi32 user32 shell32 dbghelp ws2_32)
target_link_options("${BF2_DEDICATED_TARGET}" PRIVATE
    /DEBUG
    /SUBSYSTEM:CONSOLE
    # ⚠ THE OTHER HALF OF DROPPING Py_NO_ENABLE_SHARED (2026-08-22). PC/pyconfig.h emits
    # `#pragma comment(lib, "python23.lib")` into every object that includes Python.h once the
    # shared build is enabled - a library this tree does not have and does not want, because the
    # real import library is dice_py.lib. Both batch builds have carried this switch since they
    # were written (build.cmd:180, bootrun.cmd:94); this one had never needed it before.
    /NODEFAULTLIB:python23.lib
    "/MAP:${BF2_DEDICATED_OUTPUT_DIR}/${BF2_DEDICATED_OUTPUT_NAME}.map"
    /IGNORE:4006,4088,4098
)
if(BF2_BUILD_X64)
    target_link_options("${BF2_DEDICATED_TARGET}" PRIVATE /OPT:REF /OPT:ICF /INCREMENTAL:NO)
endif()
endif() # BF2_BUILD_LINUX / BF2_BUILD_MACOS / MSVC link contracts

# =================================================================================================
# bf2_aidll - THE BOT-AI SIDE-DLL IMAGE.
# Native 64-bit ports build the same manifest with their host ABI; they cannot load x86 AI.
#
# AIDLL_w32ded.dll is a THIRD server-side image built from this same tree: bf2_w32ded.exe loads it
# through ai::AIConnection::init -> LoadLibraryBf -> the exported initDll, and reaches it afterwards
# only through the ClassManager simple-singleton registry. Its ABI is two undecorated exports and
# nothing else (Code/BF2/Core/DllModule.h); Code/AidllAbi/AIDLL_w32ded.def pins them to the shipped
# ordinals.
#
# NOT a relink of bf2_engine_objects, and it cannot be: this image compiles with BF2_IMAGE_AIDLL,
# which removes the engine-only main-ConsoleObjects instance from Core/ConsoleObjects.cpp (in
# libaidll.so that pointer is plain .bss, filled by initDll - see Core/DllGlobals.cpp). The manifest
# below is therefore its own compilation, and it MIRRORS Code/Build/aidll.cmd. Keep the two lists
# in step; aidll.cmd is the one the harness and every gate use.
#
# EXCLUDE_FROM_ALL; x86 Windows retains its diagnostic /FORCE link. Native 64-bit images link
# strictly. The executable's link does not establish the side-DLL's completeness.
#
# x86 OUTPUT: <BF2_AIDLL_OUTPUT_DIR>/AIDLL_w32ded.dll - the build tree's bin/, NEVER the game folder.
# "D:/B2BF Launcher/Battlefield2/AIDLL_w32ded.dll" is the REFERENCE binary. The server reaches the
# rebuilt DLL through a RELATIVE `AIDLL` setting (LoadLibraryBf builds ".\\" + name against the
# current directory); Code/Tests/aidll/run.py does exactly that and copies nothing.
# =================================================================================================
set(BF2_AIDLL_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin" CACHE PATH
    "Where AIDLL_w32ded.dll is written (NOT the game folder - the original lives there)")

# The AI side-DLL manifest, in link order (= static-initialisation order), shared with aidll.cmd.
set(bf2_aidll_manifest "${CMAKE_CURRENT_SOURCE_DIR}/Code/Build/aidll_sources.txt")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${bf2_aidll_manifest}")
file(STRINGS "${bf2_aidll_manifest}" bf2_aidll_manifest_lines)
set(bf2_aidll_sources)
foreach(bf2_aidll_line IN LISTS bf2_aidll_manifest_lines)
    string(STRIP "${bf2_aidll_line}" bf2_aidll_line)
    if(bf2_aidll_line AND NOT bf2_aidll_line MATCHES "^#")
        list(APPEND bf2_aidll_sources "${BF2_ROOT}/${bf2_aidll_line}")
    endif()
endforeach()
add_library(bf2_aidll SHARED EXCLUDE_FROM_ALL ${bf2_aidll_sources})
if(BF2_BUILD_POSIX)
    # [CHOSEN, not recovered] Native POSIX AI image (Linux libaidll.so, macOS libaidll.dylib; a
    # native-port design, not retail); same host toolchain and STL ABI as bf2. Never substitute the
    # retail 32-bit libaidll.so.
    bf2_configure_cpp_target(bf2_aidll)
    bf2_set_image(bf2_aidll 1)
    bf2_set_platform_compat(bf2_aidll)
    target_compile_definitions(bf2_aidll PRIVATE BF2_IMAGE_AIDLL=1 BF2_PLATFORM_POSIX)
    target_include_directories(bf2_aidll PRIVATE ${BF2_PYTHON_INCLUDE_DIRS})
    target_link_libraries(bf2_aidll PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
    set_target_properties(bf2_aidll PROPERTIES
        OUTPUT_NAME aidll
        LIBRARY_OUTPUT_DIRECTORY "${BF2_DEDICATED_OUTPUT_DIR}"
        FOLDER "BF2")
    # Keep module globals private, including names also defined by the engine. The only
    # cross-image entry points are the existing ClassManager handshake exports.
    if(BF2_BUILD_MACOS)
        target_link_options(bf2_aidll PRIVATE
            "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/Code/AidllAbi/aidll-macos.exports")
        set_property(TARGET bf2_aidll APPEND PROPERTY LINK_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/Code/AidllAbi/aidll-macos.exports")
    else()
        target_link_options(bf2_aidll PRIVATE
            -Wl,--no-undefined
            "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/Code/AidllAbi/aidll-posix.map")
        set_property(TARGET bf2_aidll APPEND PROPERTY LINK_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/Code/AidllAbi/aidll-posix.map")
    endif()
    add_dependencies("${BF2_DEDICATED_TARGET}" bf2_aidll)

    # [CHOSEN, not recovered] Engine-free check of the real loader and export lookup.
    add_executable(bf2_aidll_loader_check EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_SOURCE_DIR}/Code/Tests/aidll/posix_loader.cpp"
        "${BF2_ROOT}/Core/PluginDll.cpp")
    bf2_configure_cpp_target(bf2_aidll_loader_check)
    bf2_set_image(bf2_aidll_loader_check 1)
    bf2_set_platform_compat(bf2_aidll_loader_check)
    target_link_libraries(bf2_aidll_loader_check PRIVATE ${CMAKE_DL_LIBS})
    set_target_properties(bf2_aidll_loader_check PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${BF2_DEDICATED_OUTPUT_DIR}")
    if(BF2_BUILD_MACOS)
        set_target_properties(bf2_aidll_loader_check PROPERTIES BUILD_RPATH "@loader_path")
    else()
        set_target_properties(bf2_aidll_loader_check PROPERTIES BUILD_RPATH "\$ORIGIN")
    endif()
    add_dependencies(bf2_aidll_loader_check bf2_aidll)
elseif(BF2_BUILD_X64)
    # [CHOSEN, not recovered] Windows x64 AI for the native client and dedicated server.
    # https://github.com/kr4uzi/OpenBF2/issues/11. Keep the host's /MD, release STL layout,
    # embedded debug information and pointer-truncation errors. The x64 link is strict.
    bf2_configure_cpp_target(bf2_aidll)
    bf2_set_image(bf2_aidll 1)
    target_compile_definitions(bf2_aidll PRIVATE BF2_IMAGE_AIDLL=1 _ITERATOR_DEBUG_LEVEL=0)
    target_compile_options(bf2_aidll PRIVATE ${BF2_CLIENT_X64_COMPILE_OPTIONS})
    target_include_directories(bf2_aidll PRIVATE ${BF2_PYTHON_INCLUDE_DIRS})
    set(bf2_aidll_x64_def "${CMAKE_CURRENT_SOURCE_DIR}/Code/AidllAbi/aidll-x64.def")
    set_target_properties(bf2_aidll PROPERTIES
        OUTPUT_NAME aidll
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin_client"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin_client"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin_client"
        RUNTIME_OUTPUT_DIRECTORY_RECONSTRUCTION "${CMAKE_BINARY_DIR}/bin_client"
        PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin_client"
        FOLDER "BF2")
    target_link_libraries(bf2_aidll PRIVATE kernel32 user32 advapi32 ws2_32)
    target_link_options(bf2_aidll PRIVATE
        /DEBUG /OPT:REF /OPT:ICF /INCREMENTAL:NO /NODEFAULTLIB:python23.lib
        "/DEF:${bf2_aidll_x64_def}"
        "/MAP:${CMAKE_BINARY_DIR}/bin_client/aidll.map")
    set_property(TARGET bf2_aidll APPEND PROPERTY LINK_DEPENDS "${bf2_aidll_x64_def}")

    # Stage on every server build, even if only the DLL changed since the last executable link.
    add_custom_target(bf2_aidll_server_stage
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${BF2_DEDICATED_OUTPUT_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:bf2_aidll>" "${BF2_DEDICATED_OUTPUT_DIR}/aidll.dll"
        DEPENDS bf2_aidll
        VERBATIM)
    add_dependencies("${BF2_DEDICATED_TARGET}" bf2_aidll_server_stage)
else()
set_target_properties(bf2_aidll PROPERTIES
    LINKER_LANGUAGE CXX
    OUTPUT_NAME AIDLL_w32ded
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${BF2_AIDLL_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${BF2_AIDLL_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${BF2_AIDLL_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RECONSTRUCTION "${BF2_AIDLL_OUTPUT_DIR}"
    PDB_OUTPUT_DIRECTORY "${BF2_AIDLL_OUTPUT_DIR}"
    FOLDER "BF2"
)
bf2_configure_cpp_target(bf2_aidll)
bf2_set_image(bf2_aidll 1)
target_compile_definitions(bf2_aidll PRIVATE BF2_IMAGE_AIDLL=1)
target_include_directories(bf2_aidll PRIVATE
    "${BF2_EXTERNAL_ROOT}/python-embedded/py-src-2.3.4/Include"
    "${BF2_EXTERNAL_ROOT}/python-embedded/py-src-2.3.4/PC"
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3"
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3/contrib/minizip"
)
target_link_libraries(bf2_aidll PRIVATE kernel32 user32 advapi32 ws2_32)
target_link_options(bf2_aidll PRIVATE
    /FORCE
    /DEBUG
    # The shipped image's preferred base. Cosmetic under ASLR; it keeps an address diff against
    # AIDLL_w32ded.dll.i64 readable, which is chunk 12's whole job.
    /BASE:0x10000000
    /NODEFAULTLIB:python23.lib
    "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/Code/AidllAbi/AIDLL_w32ded.def"
    "/MAP:${BF2_AIDLL_OUTPUT_DIR}/AIDLL_w32ded.map"
    /IGNORE:4006,4088,4098
)

endif() # POSIX / Windows AIDLL

if(NOT BF2_BUILD_X64)

# =================================================================================================
# bf2_voipserver - THE VOICE-SERVER SIDE-DLL IMAGE.
#
# BF2VoipServer_w32ded.dll is a THIRD server-side image built from this tree (the AIDLL precedent
# above). bf2_w32ded.exe imports eleven CBF2VoipServer members by decorated name;
# Code/VoipServerAbi/BF2VoipServer_w32ded.def pins them to the shipped ordinals. The routing library
# underneath (VOIP::CVoipRouterInstance and its containers) is what the instance drives.
#
# EXCLUDE_FROM_ALL, and /FORCE for the same reason as bf2_aidll: unwritten members link at RVA 0 and
# fault BY NAME on the path that reaches them. The facade bodies are Phase 0 [FRONTIER] stops until
# the facade phase replaces them (Code/BF2/VoipServer/VoipServer.cpp). OUTPUT stays in the build
# tree's bin/, NEVER the game folder - the original reference binary lives there.
# =================================================================================================
# The module sources plus the engine support the win32 image statically links (the same support
# exists in libdice; compiling it here keeps the side image self-contained). Shared by the dedicated
# and client voice DLL targets.
set(bf2_voipserver_sources
    "${BF2_ROOT}/Core/Timer.cpp"
    "${BF2_ROOT}/Core/Mutex.cpp"        # the facade's instance-registry lock
    "${BF2_ROOT}/Core/Debug.cpp"        # UDPSocket's diagnostics dependency
    "${BF2_ROOT}/Core/StringUtil.cpp"   # Debug's StringHashFunc dependency
    "${BF2_ROOT}/Core/SystemInfo.cpp"   # InitChallengeString's systemInfo()
    "${BF2_ROOT}/IO/Network/NetworkLayer/Socket/UDPSocket.cpp"
    "${BF2_ROOT}/IO/Network/SocketManager.cpp"
    "${BF2_ROOT}/External/GameSpy/md5c.c"   # InitChallengeString's MD5Digest
    "${BF2_ROOT}/VoipServer/VoipServer.cpp"
    "${BF2_ROOT}/VoipServer/VoipServerInstance.cpp"
    "${BF2_ROOT}/VoipServer/VoipDebug.cpp"
    "${BF2_ROOT}/VoipServer/VoipRouter.cpp"
    "${BF2_ROOT}/VoipServer/VoipClients.cpp"
    "${BF2_ROOT}/VoipServer/VoipOutgoingChannels.cpp"
)

add_library(bf2_voipserver SHARED EXCLUDE_FROM_ALL
    ${bf2_voipserver_sources}
)
set_target_properties(bf2_voipserver PROPERTIES
    LINKER_LANGUAGE CXX
    OUTPUT_NAME BF2VoipServer_w32ded
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RECONSTRUCTION "${CMAKE_BINARY_DIR}/bin"
    PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    FOLDER "BF2"
)
bf2_configure_cpp_target(bf2_voipserver)
bf2_set_image(bf2_voipserver 1)
target_link_libraries(bf2_voipserver PRIVATE kernel32 user32 advapi32 ws2_32)
target_link_options(bf2_voipserver PRIVATE
    /FORCE
    /DEBUG
    # The shipped image's preferred base; cosmetic under ASLR, kept for a readable RVA diff.
    /BASE:0x10000000
    "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/Code/VoipServerAbi/BF2VoipServer_w32ded.def"
    "/MAP:${CMAKE_BINARY_DIR}/bin/BF2VoipServer_w32ded.map"
    /IGNORE:4006,4088,4098
)

# The dedicated server imports the CBF2VoipServer members from BF2VoipServer_w32ded.dll (the six
# decorated names in Code/VoipServerAbi/BF2VoipServer_w32ded.def). Linking the DLL target gives the
# exe its import library and forces the DLL to build first; both land in bin/.
if(TARGET bf2_voipserver)
    target_link_libraries("${BF2_DEDICATED_TARGET}" PRIVATE bf2_voipserver)
endif()

# [CHOSEN, not recovered] Compatibility build alias: the production x86 server now links strictly.
# Like bf2_client_linkcheck, this emits only the production executable, not a second check image.
if(NOT BF2_BUILD_POSIX AND NOT BF2_BUILD_X64)
    add_custom_target(bf2_w32ded_linkcheck DEPENDS bf2_w32ded)
    set_property(TARGET bf2_w32ded_linkcheck PROPERTY FOLDER "BF2")
endif()

# =================================================================================================
# bf2_voipserver_client - THE CLIENT LISTEN-SERVER VOICE DLL.
#
# BF2.exe can host a listen server and loads BF2VoipServer.dll. Its export set is identical to the
# dedicated image - the same eleven CBF2VoipServer members in the same ordinals - so it is the same
# module source compiled as the client image (/DDEDICATED=0). It stages beside BF2.exe in bin_client/.
# =================================================================================================
add_library(bf2_voipserver_client SHARED EXCLUDE_FROM_ALL
    ${bf2_voipserver_sources}
)
set_target_properties(bf2_voipserver_client PROPERTIES
    LINKER_LANGUAGE CXX
    OUTPUT_NAME BF2VoipServer
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin_client"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin_client"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin_client"
    RUNTIME_OUTPUT_DIRECTORY_RECONSTRUCTION "${CMAKE_BINARY_DIR}/bin_client"
    PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin_client"
    FOLDER "BF2"
)
bf2_configure_cpp_target(bf2_voipserver_client)
bf2_set_image(bf2_voipserver_client 0)
target_link_libraries(bf2_voipserver_client PRIVATE kernel32 user32 advapi32 ws2_32)
target_link_options(bf2_voipserver_client PRIVATE
    /FORCE
    /DEBUG
    /BASE:0x10000000
    # Same export names; the image name comes from OUTPUT_NAME (no LIBRARY line in the .def).
    "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/Code/VoipServerAbi/BF2VoipServer_w32ded.def"
    "/MAP:${CMAKE_BINARY_DIR}/bin_client/BF2VoipServer.map"
    /IGNORE:4006,4088,4098
)

# =================================================================================================
# bf2_voipserver_standalone - THE VOICE SERVER AS ITS OWN PROCESS.
#
# The shipped BF2VoipServer.exe shell (Patch_1_50 BF2VoipServerShell.cpp): read Voip.con, create one
# instance per port, tick the facade. It drives the recovered DLL through its import library, so a
# retail game server can point at it with sv.voipServerRemote 1 and only UDP packets cross the
# boundary - avoiding the MSVCP71 std::string ABI that rules out driving this DLL from the retail
# shell. Run it beside BF2VoipServer_w32ded.dll in bin/.
# =================================================================================================
add_executable(bf2_voipserver_standalone EXCLUDE_FROM_ALL
    Code/BF2/VoipServer/BF2VoipServerShell.cpp
)
set_target_properties(bf2_voipserver_standalone PROPERTIES
    OUTPUT_NAME BF2VoipServerStandalone
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RECONSTRUCTION "${CMAKE_BINARY_DIR}/bin"
    PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    FOLDER "BF2"
)
bf2_configure_cpp_target(bf2_voipserver_standalone)
bf2_set_image(bf2_voipserver_standalone 1)
target_link_libraries(bf2_voipserver_standalone PRIVATE bf2_voipserver kernel32 user32 advapi32 ws2_32)
endif() # Windows x86 voice images


# The client and the editor build in both Windows configurations: x86, and the native x64 client
# ([CHOSEN, not recovered], https://github.com/B2BF/OpenBF2/issues/7). The x64 images keep the
# shipped file names (BF2.exe and its plugin DLLs, which BF2.exe loads by name) and are written only
# inside the x64 build tree.
if(NOT BF2_BUILD_POSIX)

# =================================================================================================
# bf2_client - THE STRICTLY LINKED WINDOWS CLIENT.
#
# This /DDEDICATED=0 image links without /FORCE on x86 and x64 when the matching
# dice_py import library is supplied (MSVC 14.44 checks, 2026-11-08).
# Four-way runs and the x64 movie check establish runtime reach; remaining engine
# frontiers still limit individual paths. The target stays EXCLUDE_FROM_ALL.
# Native plugins are staged beside the host by bf2_client_plugins below.
# Output stays in this build tree and can never overwrite the retail BF2.exe used as
# reverse-engineering evidence.
# =================================================================================================
# Fixed inside this build tree on purpose: unlike the server's opt-in deployment directory, a
# client target must never be redirected onto the retail BF2.exe (CMake clean would then
# own and could remove it).
set(BF2_CLIENT_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin_client")

add_executable(bf2_client EXCLUDE_FROM_ALL
    $<TARGET_OBJECTS:bf2_client_objects>
    $<TARGET_OBJECTS:bf2_zlib_objects>
    $<TARGET_OBJECTS:bf2_tinyxml_objects>
    $<TARGET_OBJECTS:bf2_gamespy_objects>
    $<TARGET_OBJECTS:bf2_gamespy_sb_objects>
    $<TARGET_OBJECTS:bf2_gamespy_gp_objects>
    $<TARGET_OBJECTS:bf2_gamespy_cdkeyc_objects>
    $<TARGET_OBJECTS:bf2_gamespy_voice_objects>
)
set_target_properties(bf2_client PROPERTIES
    LINKER_LANGUAGE CXX
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME BF2
    RUNTIME_OUTPUT_DIRECTORY "${BF2_CLIENT_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${BF2_CLIENT_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${BF2_CLIENT_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RECONSTRUCTION "${BF2_CLIENT_OUTPUT_DIR}"
    PDB_OUTPUT_DIRECTORY "${BF2_CLIENT_OUTPUT_DIR}"
    PDB_OUTPUT_DIRECTORY_DEBUG "${BF2_CLIENT_OUTPUT_DIR}"
    PDB_OUTPUT_DIRECTORY_RELEASE "${BF2_CLIENT_OUTPUT_DIR}"
    PDB_OUTPUT_DIRECTORY_RECONSTRUCTION "${BF2_CLIENT_OUTPUT_DIR}"
    FOLDER "BF2"
)
bf2_configure_cpp_target(bf2_client)
bf2_set_image(bf2_client 0)
if(BF2_BUILD_X64)
    add_dependencies(bf2_client bf2_aidll)
endif()
target_compile_definitions(bf2_client PRIVATE _ITERATOR_DEBUG_LEVEL=0)
set_target_properties(bf2_client PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${BF2_GAME_DIR}"
    VS_DEBUGGER_COMMAND_ARGUMENTS "+modPath mods/bf2 +fullscreen 0 +multi 1"
)

# Normal startup includes the movies; +restart remains an optional explicit skip.
set(BF2_CLIENT_RUN_ARGS "+modPath;mods/bf2;+fullscreen;0;+multi;1"
    CACHE STRING "Arguments for the bf2_client_run target (a CMake ;-list).")
add_custom_target(bf2_client_run
    COMMAND "$<TARGET_FILE:bf2_client>" ${BF2_CLIENT_RUN_ARGS}
    DEPENDS bf2_client
    WORKING_DIRECTORY "${BF2_GAME_DIR}"
    USES_TERMINAL
    VERBATIM
)
set_property(TARGET bf2_client_run PROPERTY FOLDER "BF2")

if(EXISTS "${BF2_DICE_PY_LIB}")
    target_link_libraries(bf2_client PRIVATE "${BF2_DICE_PY_LIB}")
else()
    message(WARNING
        "bf2_client: ${BF2_DICE_PY_LIB} is missing - CPython calls remain in the link work list. "
        "Run Code/Build/python/build_python.cmd.")
endif()

if(EXISTS "${BF2_DICE_PY_DLL}")
    add_custom_command(TARGET bf2_client POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${BF2_DICE_PY_DLL}" "${BF2_CLIENT_OUTPUT_DIR}/dice_py.dll"
        COMMENT "Staging dice_py.dll beside BF2.exe"
        VERBATIM
    )
endif()

# [CHOSEN, not recovered] The Novint Falcon is archived (Code/Archive/NovintFalcon, issue #17): no image
# imports or stages NovintHFX.dll.

# [CHOSEN, not recovered] Build and stage one native client/plugin set:
# https://github.com/B2BF/OpenBF2/issues/7. These DLLs target the rebuilt host only;
# gameplay compatibility with retail clients is a separate protocol/content contract.
# LoadLibrary resolves the application directory before the game directory, which is never
# written. Include TextureAtlasBuilder.dll so RendDX9's static import resolves to our build.
# The atlas keeps its private /MT runtime for file-format rounding; the other three DLLs
# share the client's /MD CRT and native STL ABI.
# Code/Build/stage_plugin.cmake runs on every bf2_client build, even without an executable relink.
#
# All four plugins are rebuilt from source first: running
# the client must never stage a DLL that is older than the sources it was built from. Each script
# is its own authoritative build gate, carrying an explicit TU manifest, image defines, and
# native ABI assertions. Each wipes its object directory first, so every run is a clean build. That
# is why this rebuilds unconditionally instead of declaring a CMake dependency list: a partial
# dependency scan would miss a header edit and hand back a stale DLL while claiming freshness.
# None takes USES_TERMINAL, which would serialise them against every other
# console-pool job rather than just against each other.
#
# The plugin builds are ordered, not run in parallel.
# Run concurrently, one build in seven (1 of 7 observed, 2026-09-11) ended with cl reporting
# "51 compiled, 0 failed" while one object - 46_OverlayTexture.obj - never landed, and the link
# then failed on its two symbols. Both scripts compile /Zi /FS against a per-script PDB, so
# concurrent cl processes share one mspdbsrv; that is the same mechanism already recorded against
# build.cmd under concurrent builds, where /Z7 was the real fix. THIS ORDERING IS A MITIGATION,
# NOT A PROVEN CURE - it only restores the one-at-a-time mode these scripts were validated in, and
# does nothing about cl processes the client's own compiles run alongside them. If the flake
# reappears, move both scripts from /Zi /FS to /Z7 (link-time /DEBUG still emits the full PDB, so
# the FILE:LINE resolution renddx9.cmd documents is preserved) rather than re-running the build.
#
# A failing plugin build FAILS the client build on purpose, and the failure is loud and safe: the
# link has no /FORCE, so a lost object stops the build instead of reaching the DLL as an RVA-0
# body, and the previous DLL is NOT staged. Carrying on would be the silent-stale outcome this
# step exists to prevent. A re-run clears the flake.
#
# Rebuild and stage plugins on every client build. Their scripts do not emit full include
# dependencies, so a glob over plugin sources would miss changes in shared engine headers.
#
# Each script takes the architecture as its flavour argument: `native` builds the x86 DLL into
# Code/Build/bin_<name>_native, `x64` the native x64 DLL into Code/Build/bin_<name>_x64. The x64
# RendDX9.dll has no Bink import: it links the FFmpeg-based Bink stand-in (renddx9.cmd x64,
# Code/BF2/External/ffmpeg-bink/README.md); the x86 DLL imports binkw32.dll.
if(BF2_BUILD_X64)
    set(BF2_PLUGIN_FLAVOUR x64)
else()
    set(BF2_PLUGIN_FLAVOUR native)
endif()
if(TRUE)
    # ⚠ EACH SCRIPT IS NAMED BY ITS FULL NATIVE PATH, not bare beside WORKING_DIRECTORY. cmd.exe
    # finds a bare script name only by searching the current directory, and that search is OFF
    # wherever NoDefaultCurrentDirectoryInExePath is set. That is inherited per-process, not a
    # machine setting, so the bare form works or fails depending on who launched the build: it was
    # observed set (2026-09-11) in an agent-driven build, where `cmd.exe /d /c renddx9.cmd` reported
    # the script as "not recognized" from the very directory holding it. The explicit path works in
    # both. file(TO_NATIVE_PATH) supplies the backslashes cmd needs; CMake's own separator is the
    # forward slash, which cmd will not take for a program name. The scripts resolve their inputs
    # and outputs from %~dp0, so the working directory is set for consistency with the other batch
    # targets, not depended on. The sibling bf2_gap_report/bf2_syntaxcheck targets still use the
    # bare form and carry the same environment dependence.
    file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Code/Build/renddx9.cmd" BF2_RENDDX9_SCRIPT)
    file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Code/Build/swiffplayer.cmd" BF2_SWIFFPLAYER_SCRIPT)
    file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Code/Build/textureatlasbuilder.cmd" BF2_TEXTUREATLAS_SCRIPT)
    add_custom_target(bf2_textureatlas_native
        COMMAND cmd.exe /d /c "${BF2_TEXTUREATLAS_SCRIPT}" ${BF2_PLUGIN_FLAVOUR}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Code/Build"
        COMMENT "Building the ${BF2_PLUGIN_FLAVOUR} TextureAtlasBuilder.dll (Code/Build/textureatlasbuilder.cmd ${BF2_PLUGIN_FLAVOUR})"
        VERBATIM
    )
    set_property(TARGET bf2_textureatlas_native PROPERTY FOLDER "BF2")
    # [CHOSEN, not recovered] G2w: BF2_GFX_SEAM=ON makes renddx9.cmd compile the renderer against the subset
    # headers plus the generated forwarding backend (Platform/Gfx/D3D9Forward).
    set(BF2_RENDDX9_SEAM_ENV)
    if(BF2_GFX_SEAM)
        set(BF2_RENDDX9_SEAM_ENV "${CMAKE_COMMAND}" -E env BF2_GFX_SEAM=ON)
    endif()
    add_custom_target(bf2_renddx9_native
        COMMAND ${BF2_FRONTIER_DEFAULT_ENV} ${BF2_RENDDX9_SEAM_ENV} cmd.exe /d /c "${BF2_RENDDX9_SCRIPT}" ${BF2_PLUGIN_FLAVOUR}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Code/Build"
        COMMENT "Building the ${BF2_PLUGIN_FLAVOUR} RendDX9.dll (Code/Build/renddx9.cmd ${BF2_PLUGIN_FLAVOUR})"
        VERBATIM
    )
    set_property(TARGET bf2_renddx9_native PROPERTY FOLDER "BF2")
    if(NOT BF2_GFX_SEAM)
        add_dependencies(bf2_renddx9_native bf2_textureatlas_native)
    endif()
    add_custom_target(bf2_swiffplayer_native
        COMMAND ${BF2_FRONTIER_DEFAULT_ENV} cmd.exe /d /c "${BF2_SWIFFPLAYER_SCRIPT}" ${BF2_PLUGIN_FLAVOUR}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Code/Build"
        COMMENT "Building the ${BF2_PLUGIN_FLAVOUR} SwiffPlayer.dll (Code/Build/swiffplayer.cmd ${BF2_PLUGIN_FLAVOUR})"
        VERBATIM
    )
    set_property(TARGET bf2_swiffplayer_native PROPERTY FOLDER "BF2")
    # The ordering edge described above. RendDX9 is the longer build, so it goes first.
    add_dependencies(bf2_swiffplayer_native bf2_renddx9_native)
    # BF2Audio.dll - the OpenAL sound engine (2026-09-29). The native flavour (Code\Build\
    # bf2audio.cmd native -> Code/Build/bin_bf2audio_native) is the one the rebuilt BF2.exe
    # loads: BfSoundModule::init does LoadLibraryA("BF2Audio.dll") + getSoundEngine exactly as
    # retail (0x7E16D0). The DLL itself resolves BF2OpenAL.dll / OpenAL32.dll at run time from
    # the game directory (the client's working directory), so nothing else is staged. Same
    # ordering rule as the two above: after SwiffPlayer, never concurrently.
    file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Code/Build/bf2audio.cmd" BF2_BF2AUDIO_SCRIPT)
    add_custom_target(bf2_bf2audio_native
        COMMAND ${BF2_FRONTIER_DEFAULT_ENV} cmd.exe /d /c "${BF2_BF2AUDIO_SCRIPT}" ${BF2_PLUGIN_FLAVOUR}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Code/Build"
        COMMENT "Building the ${BF2_PLUGIN_FLAVOUR} BF2Audio.dll (Code/Build/bf2audio.cmd ${BF2_PLUGIN_FLAVOUR})"
        VERBATIM
    )
    set_property(TARGET bf2_bf2audio_native PROPERTY FOLDER "BF2")
    add_dependencies(bf2_bf2audio_native bf2_swiffplayer_native)
endif()

set(BF2_PLUGIN_STAGE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/Code/Build/stage_plugin.cmake")
add_custom_target(bf2_client_plugins
    COMMAND "${CMAKE_COMMAND}"
            "-DBF2_PLUGIN_SRC=${CMAKE_CURRENT_SOURCE_DIR}/Code/Build/bin_renddx9_${BF2_PLUGIN_FLAVOUR}/RendDX9.dll"
            "-DBF2_PLUGIN_DST_DIR=${BF2_CLIENT_OUTPUT_DIR}"
            "-DBF2_PLUGIN_HINT=build it with Code/Build/renddx9.cmd ${BF2_PLUGIN_FLAVOUR}"
            -P "${BF2_PLUGIN_STAGE_SCRIPT}"
    COMMAND "${CMAKE_COMMAND}"
            "-DBF2_PLUGIN_SRC=${CMAKE_CURRENT_SOURCE_DIR}/Code/Build/bin_swiffplayer_${BF2_PLUGIN_FLAVOUR}/SwiffPlayer.dll"
            "-DBF2_PLUGIN_DST_DIR=${BF2_CLIENT_OUTPUT_DIR}"
            "-DBF2_PLUGIN_HINT=build it with Code/Build/swiffplayer.cmd ${BF2_PLUGIN_FLAVOUR}"
            -P "${BF2_PLUGIN_STAGE_SCRIPT}"
    COMMAND "${CMAKE_COMMAND}"
            "-DBF2_PLUGIN_SRC=${CMAKE_CURRENT_SOURCE_DIR}/Code/Build/bin_bf2audio_${BF2_PLUGIN_FLAVOUR}/BF2Audio.dll"
            "-DBF2_PLUGIN_DST_DIR=${BF2_CLIENT_OUTPUT_DIR}"
            "-DBF2_PLUGIN_HINT=build it with Code/Build/bf2audio.cmd ${BF2_PLUGIN_FLAVOUR}"
            -P "${BF2_PLUGIN_STAGE_SCRIPT}"
    COMMAND "${CMAKE_COMMAND}"
            "-DBF2_PLUGIN_SRC=${CMAKE_CURRENT_SOURCE_DIR}/Code/Build/bin_textureatlasbuilder_${BF2_PLUGIN_FLAVOUR}/TextureAtlasBuilder.dll"
            "-DBF2_PLUGIN_DST_DIR=${BF2_CLIENT_OUTPUT_DIR}"
            "-DBF2_PLUGIN_HINT=build it with Code/Build/textureatlasbuilder.cmd ${BF2_PLUGIN_FLAVOUR}"
            -P "${BF2_PLUGIN_STAGE_SCRIPT}"
    COMMENT "Staging the four ${BF2_PLUGIN_FLAVOUR} plugins beside BF2.exe"
    VERBATIM
)
# Ordering AND obligation: all four plugin builds complete before anything is copied. This
# edge is unconditional on purpose - see the note above the plugin targets. Do not put it back
# behind a switch; a staged DLL older than its sources is indistinguishable from a real bug at the
# point where it hurts, which is a live debugging session.
add_dependencies(bf2_client_plugins bf2_textureatlas_native bf2_renddx9_native bf2_swiffplayer_native bf2_bf2audio_native)
add_dependencies(bf2_client bf2_client_plugins)
# The subsystem version that keeps the classic (pre-6.0) window frame; see the link options below.
# x64: 5.02, the lowest version link.exe accepts for an x64 image, on the same side of 6.0.
if(BF2_BUILD_X64)
    set(BF2_CLIENT_SUBSYSTEM "/SUBSYSTEM:WINDOWS,5.02")
else()
    set(BF2_CLIENT_SUBSYSTEM "/SUBSYSTEM:WINDOWS,5.01")
endif()
# System imports present in the retail BF2.exe.
target_link_libraries(bf2_client PRIVATE
    advapi32 crypt32 dbghelp dinput8 dsound dxguid gdi32 ole32 psapi shell32 user32 winmm ws2_32
)
# BF2.exe hosts a listen server, so it imports the CBF2VoipServer members from BF2VoipServer.dll
# (the client voice image built by bf2_voipserver_client above).
if(TARGET bf2_voipserver_client)
    target_link_libraries(bf2_client PRIVATE bf2_voipserver_client)
endif()
# [CHOSEN, not recovered] x64 (issue #7): BF2VoipServer.dll is x86-only (its .def pins x86 decorated
# names), so the native x64 client links the voice-server module statically, as the x64 dedicated
# server and the macOS client do. Client image (/DDEDICATED=0), same sources as bf2_voipserver_client.
if(BF2_BUILD_X64)
    add_library(bf2_voipserver_client_objects OBJECT EXCLUDE_FROM_ALL ${bf2_voipserver_only_sources})
    bf2_configure_cpp_target(bf2_voipserver_client_objects)
    bf2_set_image(bf2_voipserver_client_objects 0)
    bf2_set_platform_compat(bf2_voipserver_client_objects)
    target_compile_definitions(bf2_voipserver_client_objects PRIVATE _ITERATOR_DEBUG_LEVEL=0)
    target_compile_options(bf2_voipserver_client_objects PRIVATE ${BF2_CLIENT_X64_COMPILE_OPTIONS})
    set_property(TARGET bf2_voipserver_client_objects PROPERTY FOLDER "BF2")
    target_sources(bf2_client PRIVATE $<TARGET_OBJECTS:bf2_voipserver_client_objects>)
endif()
target_link_options(bf2_client PRIVATE
    /INCREMENTAL:NO
    /DEBUG
    # ⚠ THE ",4.0" IS LOAD-BEARING, AND IT IS A FIDELITY SETTING, NOT A PREFERENCE.
    # The shipped BF2.exe carries MajorSubsystemVersion 4.0; a modern link defaults to 6.0. Windows
    # reads that field to decide the NON-CLIENT FRAME: a pre-6.0 image gets the classic frame, a
    # 6.0+ image additionally gets SM_CXPADDEDBORDER (4 px) on every side. Window::create computes
    # its outer size the XP way, `width + 2*SM_CXFRAME` and `height + SM_CYCAPTION + 2*SM_CYFRAME`,
    # which is EXACTLY what the shipped body does (BF2.exe 0x58E434, and the matching resize
    # sub_45D7F0) - so at 6.0 that arithmetic lands 8 px short in each axis and the client area
    # comes out 792x592 for a requested 800x600.
    #
    # That 8 px was not cosmetic. The D3D device is created at the requested size, each font's ortho
    # projection is built from the viewport current at load (792x592, faithfully, as sub_100A4420
    # does), and SwiffRender then draws under 800x600 - so every glyph was displaced by 600/592
    # about the top-left origin: invisible at the top of the screen, ~5.5 px low at the login panel.
    # Measured 2026-09-11; patching only this field gives client 800x600, identical to retail.
    #
    # [PLATFORM] 5.01 RATHER THAN RETAIL'S 4.0, and the difference is forced by the toolchain, not
    # chosen: this link.exe answers ",4.0" with "LNK4010: invalid subsystem version number 4.0;
    # default subsystem version assumed" and silently falls back to 6.0, which is the bug. 5.01 is
    # the lowest value it accepts for x86. What governs the frame is the 6.0 boundary, so 5.01 and
    # retail's 4.0 behave identically here; the field itself still differs from the shipped image.
    "${BF2_CLIENT_SUBSYSTEM}"
    /NODEFAULTLIB:python23.lib
    "/MAP:${BF2_CLIENT_OUTPUT_DIR}/BF2.map"
    /IGNORE:4006,4088,4098
)

set_property(TARGET bf2_client_objects PROPERTY FOLDER "BF2")

# Compatibility build alias: bf2_client itself now performs the strict link.
add_custom_target(bf2_client_linkcheck DEPENDS bf2_client)
set_property(TARGET bf2_client_linkcheck PROPERTY FOLDER "BF2")

# The BF2 Editor links the client object set above, so it lives in this Windows client block
# (x86 and native x64). Every editor target is EXCLUDE_FROM_ALL.
add_subdirectory(Code/Tools/BF2Editor)
endif() # NOT BF2_BUILD_POSIX (client and editor)

# =================================================================================================
# bf2_client on macOS - THE NATIVE macOS CLIENT IMAGE. [CHOSEN, not recovered]
#
# An owner-requested port change (macOSplan.md, package S2.0; docs/macos-port.md, "Client triage").
# The client objects above are compiled with DEDICATED=0 and the macOS platform switches. The client
# plugin images are not shared libraries here: retail-DLL swapping is retired
# (https://github.com/B2BF/OpenBF2/issues/7), so RendDX9, SwiffPlayer and BF2Audio are static
# libraries, and Code/StaticPlugins/StaticPlugins.cpp answers the engine's LoadLibrary/GetProcAddress
# lookups for the ones linked in (BF2_STATIC_PLUGINS). RendDX9 is not linked by default; see
# BF2_MACOS_LINK_RENDDX9 below. Each library compiles its image's manifest
# from Code/Build/<plugin>.cmd with that image's define; keep the lists in step with the scripts.
# Left out of every library: the Windows loader entry (RendDX9Abi/DllMain.cpp) and the engine TUs the
# host image already compiles (Math/*, Core/ConsoleObjectBase.cpp), because one image holds one copy.
# The RendDX9Abi kit (heap adapters, vc71 string, frontier log) is compiled once, in
# bf2_plugin_kit_static, for the same reason. The three plugin entry pairs are renamed per image so
# they can coexist in one link. TextureAtlasBuilder is embedded in the native renderer.
#
# The link is strict (ld64 rejects undefined symbols). It does not complete yet: the compile triage
# in docs/macos-port.md lists the blocked translation units by blocker class.
# =================================================================================================
if(BF2_BUILD_MACOS)
    set(BF2_RENDDX9_STATIC_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/Code/RendDX9Abi/CrashReport.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Code/RendDX9Abi/EngineFile.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Code/RendDX9Abi/PluginEntry.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Code/RendDX9Abi/NativeConsoleObjects.cpp"
        # [CHOSEN, not recovered] The binkw32 entry points on the vendored FFmpeg Bink decoders
        # (bf2_ffmpeg_bink below; Code/BF2/External/ffmpeg-bink/README.md) and their AudioQueue sound output.
        "${CMAKE_CURRENT_SOURCE_DIR}/Code/RendDX9Abi/BinkFFmpeg.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Code/RendDX9Abi/BinkAudioCoreAudio.cpp"
    )
    # Code/Build/renddx9.cmd, STEP 2: SOURCES minus the kit, DllMain.cpp and the shared Math TUs,
    # then CONSOLE_SOURCES minus Core/ConsoleObjectBase.cpp.
    foreach(rel
            RendDX9/BF2Render.cpp RendDX9/PostProductionFrameWork.cpp RendDX9/RendDX9Init.cpp
            RendDX9/BufferManager.cpp RendDX9/QueryManager.cpp RendDX9/RendDX9Types.cpp
            RendDX9/Texture.cpp RendDX9/TextureManager.cpp RendDX9/TextureAtlas/TextureAtlas.cpp
            RendDX9/ResourceManager.cpp RendDX9/ResourceTracer.cpp RendDX9/VideoOptions.cpp
            RendDX9/Effect.cpp RendDX9/DiceEffect.cpp
            RendDX9/FXParameter.cpp RendDX9/ShaderManager.cpp RendDX9/StateManager.cpp
            RendDX9/RendDX9Draw.cpp RendDX9/RenderQueues.cpp RendDX9/Ra/EffectManager.cpp
            RendDX9/MovieRenderer.cpp
            RendDX9/Ra/RaShaderPermutator.cpp RendDX9/Ra/RaShader.cpp RendDX9/Ra/RaVertexStreams.cpp
            RendDX9/Ra/RaStreamDataEditable.cpp RendMisc/RendModule.cpp RendMisc/RenderView.cpp
            RendMisc/EnvMapSystem/EnvMapManager.cpp RendMisc/EnvMapSystem/EnvMapObject.cpp
            IO/Tokenizer.cpp Geom/ModuleGeom.cpp Geom/GeometryTemplateManager.cpp
            Geom/MeshTemplate.cpp Geom/StaticMeshTemplate.cpp Geom/SkinnedMeshTemplate.cpp
            Geom/SkinnedMesh.cpp Geom/BundledMeshTemplate.cpp Geom/BundledMeshMaterial.cpp
            Geom/BundledMeshStateManager.cpp Geom/BundledMesh.cpp Geom/BundledMeshDraw.cpp
            Geom/BundledMeshHeight.cpp Geom/StaticMesh.cpp Geom/StaticMeshLightmapGeneration.cpp
            Geom/StaticMeshMaterial.cpp Geom/StaticMeshStateManager.cpp Geom/SkyDome.cpp
            Geom/UndergrowthSystem.cpp Geom/UndergrowthSystemLoad.cpp
            Geom/LightningManager.cpp Geom/WeatherManager.cpp Geom/Rain/RainManager.cpp
            Geom/UndergrowthSystemRender.cpp Geom/Terrain.cpp Geom/QuadPatch.cpp
            Geom/RaQuadPatchAdapter.cpp
            Geom/SurroundingPatch.cpp Geom/TerrainDraw.cpp Geom/TerrainEditable.cpp
            Geom/TerrainEditableGetSets.cpp Geom/QuadPatchEditable.cpp
            Geom/SurroundingPatchEditable.cpp Geom/TerrainLightSampling.cpp
            Geom/LightmapGeneration/LightmapGeneration.cpp
            Geom/LightmapGeneration/MeshLightSampling.cpp Geom/ObjectDrawer.cpp
            Geom/XpLightManager.cpp Geom/LightMerger.cpp Geom/LightGeometry.cpp Geom/MeshOptimizer.cpp
            Geom/BundledMeshOptimizer.cpp Geom/StaticMeshOptimizer.cpp Geom/NametagManager.cpp Geom/NameTagRenderer.cpp
            Geom/Overgrowth/OverGrowth.cpp Geom/Overgrowth/OverGrowthLoad.cpp
            Geom/Overgrowth/OverGrowthCells.cpp Geom/Overgrowth/OverGrowthCellRender.cpp
            Geom/Overgrowth/OverGrowthRender.cpp Geom/RoadCompiledRenderer.cpp
            Geom/RoadCompiledTemplate.cpp Geom/RoadCompiled.cpp Geom/RoadTemplate.cpp
            Geom/RoadEditableRenderer.cpp Geom/RoadEditable.cpp Geom/DecalSystem/DecalManager.cpp
            Geom/DecalSystem/DecalShadowManager.cpp
            Geom/DecalSystem/DecalObject.cpp Geom/DecalSystem/DecalTemplate.cpp
            Geom/DecalSystem/DecalRenderer.cpp Geom/ParticleSystem/ParticleSystemManager.cpp
            Geom/HemiMapManager/HemiMapManager.cpp Geom/ParticleSystem/ParticleSystemTemplate.cpp
            Geom/ParticleSystem/ParticleSystemTemplateType1.cpp
            Geom/ParticleSystem/MeshParticleMesh/MeshParticleMeshTemplate.cpp
            Geom/ParticleSystem/MeshParticleMesh/MeshParticleMesh.cpp
            Geom/ParticleSystem/TrailSystemTemplate.cpp
            Geom/ParticleSystem/ParticleSystemTemplateType3.cpp
            Geom/ParticleSystem/NSAParticleSystemTemplate.cpp Geom/StaticMeshRenderer.cpp
            Geom/SkinnedMeshRenderer.cpp Geom/SwiffRender.cpp GeomMisc/ModuleGeomMisc.cpp
            GeomMisc/FSQuadDrawer.cpp GeomMisc/DebugTextWriter.cpp GeomMisc/Debug.cpp
            GeomMisc/DebugBoxMesh.cpp
            GeomMisc/DebugCylinderMesh.cpp
            GeomMisc/DebugSphereMesh.cpp
            GeomMisc/DebugPlaneMesh.cpp
            GeomMisc/DebugPolygonMesh.cpp
            GeomMisc/DebugTorusMesh.cpp
            GeomMisc/DebugXMesh.cpp
            GeomMisc/DebugRenderer.cpp GeomMisc/DebugTextDrawer.cpp
            GeomMisc/DebugCircleDrawer.cpp
            GeomMisc/DebugLineDrawer.cpp
            GeomMisc/FontSystem/FontSystem.cpp GeomMisc/FontSystem/SwiffFont.cpp
            GeomMisc/OverlayTexture.cpp GeomMisc/DebugLineGraphD3d.cpp GeomMisc/PerlinNoise.cpp
            GeomMisc/DebugBlockGraphD3D.cpp
            GeomMisc/LoadingScreen/LoadingScreen.cpp
            GeomMisc/SplineControlPoint.cpp GeomMisc/EditorSpline.cpp
            GeomMisc/MenuRend/MemeTexture.cpp GeomMisc/MenuRend/FSMoviePlayer.cpp
            GeomMisc/MenuRend/Video.cpp GeomMisc/MenuRend/QuadDrawer.cpp
            GeomMisc/MenuRend/QuadOutputCache.cpp
            Geom/LightManagerConsole.cpp Geom/HemiMapManager/HemiMapManagerConsole.cpp
            Geom/SkyDomeConsole.cpp Geom/TerrainConsole.cpp RendDX9/RendererConsole.cpp
            Geom/TerrainCullerConsole.cpp
            Geom/NametagManagerConsole.cpp Geom/Overgrowth/OverGrowthConsole.cpp
            Geom/UndergrowthSystemConsole.cpp Geom/DecalSystem/DecalManagerConsole.cpp
            Geom/ObjectGeometryConsole.cpp Geom/LightmapGeneration/LightmapGenerationConsole.cpp
            Geom/StaticMeshRendererConsole.cpp Geom/WeatherManagerConsole.cpp
            Geom/HeightmapEventNode.cpp)
        list(APPEND BF2_RENDDX9_STATIC_SOURCES "${BF2_ROOT}/${rel}")
    endforeach()

    # Code/Build/swiffplayer.cmd, STEP 2, minus the kit and DllMain.cpp.
    set(BF2_SWIFFPLAYER_STATIC_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/Code/SwiffPlayerAbi/PluginEntry.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Code/SwiffPlayerAbi/SwiffMemory.cpp"
        "${BF2_EXTERNAL_ROOT}/gameswf/SwiffPlayer/SwiffPlayer.cpp"
        "${BF2_EXTERNAL_ROOT}/gameswf/SwiffPlayer/SwiffCrashLog.cpp"
        "${BF2_EXTERNAL_ROOT}/gameswf/SwiffPlayer/SwiffPlayerModule.cpp"
    )
    foreach(rel
            base/config.cpp base/container.cpp base/file_util.cpp base/image.cpp
            base/image_filters.cpp base/jpeg.cpp base/membuf.cpp base/tu_file.cpp base/tu_random.cpp
            base/tu_timer.cpp base/tu_types.cpp base/utf8.cpp base/utility.cpp
            gameswf/gameswf_action.cpp gameswf/gameswf_button.cpp gameswf/gameswf_dlist.cpp
            gameswf/gameswf_font.cpp gameswf/gameswf_fontlib.cpp gameswf/gameswf_impl.cpp
            gameswf/gameswf_log.cpp gameswf/gameswf_morph2.cpp
            gameswf/gameswf_render.cpp gameswf/gameswf_shape.cpp gameswf/gameswf_sound.cpp
            gameswf/gameswf_stream.cpp gameswf/gameswf_string.cpp gameswf/gameswf_styles.cpp
            gameswf/gameswf_tesselate.cpp gameswf/gameswf_text.cpp
            gameswf/gameswf_timers.cpp gameswf/gameswf_types.cpp gameswf/gameswf_xml.cpp)
        list(APPEND BF2_SWIFFPLAYER_STATIC_SOURCES "${BF2_EXTERNAL_ROOT}/${rel}")
    endforeach()

    # Code/Build/bf2audio.cmd, STEP 2, minus the kit. Win32System.cpp is that image's DllMain and
    # its LoadLibrary wrappers; it stays in the list so the triage reports it (package S2.5).
    set(BF2_BF2AUDIO_STATIC_SOURCES "${BF2_ROOT}/Sound/NullSound.cpp")
    foreach(rel
            Ambient.cpp BfOpenAL.cpp BfSoundEngine.cpp Music.cpp OpenALProvider.cpp
            OpenALSample.cpp Reverb.cpp Sample.cpp Sound.cpp SoundDefaults.cpp SoundEvaluator.cpp
            SoundFile.cpp SoundTrigger.cpp WavFileFormat.cpp Win32System.cpp)
        list(APPEND BF2_BF2AUDIO_STATIC_SOURCES "${BF2_ROOT}/Sound/SoundEngine/${rel}")
    endforeach()
    set(BF2_VORBIS_STATIC_SOURCES
        "${BF2_EXTERNAL_ROOT}/libogg-1.3.6/src/bitwise.c"
        "${BF2_EXTERNAL_ROOT}/libogg-1.3.6/src/framing.c"
    )
    foreach(name
            analysis bitrate block codebook envelope floor0 floor1 info lookup lpc lsp mapping0
            mdct psy registry res0 sharedbook smallft synthesis vorbisfile window)
        list(APPEND BF2_VORBIS_STATIC_SOURCES "${BF2_EXTERNAL_ROOT}/libvorbis-1.3.7/lib/${name}.c")
    endforeach()

    # The settings every plugin library shares with the client objects.
    function(bf2_configure_static_plugin target image_define)
        bf2_configure_cpp_target("${target}")
        bf2_set_image("${target}" 0)
        bf2_set_platform_compat("${target}")
        target_compile_definitions("${target}" PRIVATE
            _ITERATOR_DEBUG_LEVEL=0 BF2_PLATFORM_POSIX _MACOSX
            BF2_STATIC_PLUGINS=1 ${image_define})
        set_property(TARGET "${target}" PROPERTY FOLDER "BF2")
    endfunction()

    # [CHOSEN, not recovered] Each statically linked plugin image keeps its own host-service globals
    # (dice::hfe::classManager, settings, g_system, game and the rest that its PluginEntry.cpp
    # defines and its initDll fills), as the DLL did. The host defines globals of the same names,
    # often with other types. The image's static library is therefore prelinked into one
    # relocatable object (ld -r) in which the symbols named in Code/StaticPlugins/
    # image-private-symbols.txt are bound to the image's own definitions and then made local;
    # everything else keeps its linkage. See docs/macos-port.md, "Plugin globals".
    # BF2Audio defines no host-service globals; its own null sound/template pair
    # (Sound/SoundEngine/BfSoundEngine.cpp, created by getSoundEngine) is image-private in the
    # same way and is listed in Code/StaticPlugins/bf2audio-private-symbols.txt.
    set(BF2_STATIC_PLUGIN_PRIVATE_SYMBOLS
        "${CMAKE_CURRENT_SOURCE_DIR}/Code/StaticPlugins/image-private-symbols.txt")
    function(bf2_prelink_static_plugin consumer library symbols)
        set(prelinked "${CMAKE_CURRENT_BINARY_DIR}/StaticPlugins/${library}_prelinked.o")
        set(arch_flags "")
        foreach(arch IN LISTS CMAKE_OSX_ARCHITECTURES)
            list(APPEND arch_flags -arch "${arch}")
        endforeach()
        add_custom_command(OUTPUT "${prelinked}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/StaticPlugins"
            COMMAND "${CMAKE_CXX_COMPILER}" ${arch_flags} -r -nostdlib -o "${prelinked}"
                    "-Wl,-force_load,$<TARGET_FILE:${library}>"
                    "-Wl,-unexported_symbols_list,${symbols}"
            DEPENDS "${library}" "$<TARGET_FILE:${library}>" "${symbols}"
            COMMENT "Prelinking ${library} with image-private host-service globals"
            VERBATIM)
        target_sources("${consumer}" PRIVATE "${prelinked}")
        set_source_files_properties("${prelinked}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
    endfunction()

    add_library(bf2_plugin_kit_static STATIC EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_SOURCE_DIR}/Code/RendDX9Abi/BfMemory.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Code/RendDX9Abi/Frontier.cpp"
    )
    bf2_configure_static_plugin(bf2_plugin_kit_static "")
    if(BF2_FRONTIER_DEFAULT_CONTINUE)
        target_compile_definitions(bf2_plugin_kit_static PRIVATE BF2_FRONTIER_DEFAULT_CONTINUE=1)
    endif()

    add_library(bf2_renddx9_static STATIC EXCLUDE_FROM_ALL ${BF2_RENDDX9_STATIC_SOURCES})
    bf2_configure_static_plugin(bf2_renddx9_static BF2_IMAGE_RENDDX9)
    # [CHOSEN, not recovered] Native factories/build/save use the renderer's Metal D3D9 seam.
    set(BF2_TEXTURE_ATLAS_NATIVE_SOURCES
        "${BF2_ROOT}/RendDX9/TextureAtlas/TextureAtlasBuilder/Packer.cpp"
        "${BF2_ROOT}/RendDX9/TextureAtlas/TextureAtlasBuilder/TextureAtlas.cpp"
        "${BF2_ROOT}/RendDX9/TextureAtlas/TextureAtlasBuilder/TextureAtlasConfiguration.cpp"
        "${BF2_ROOT}/RendDX9/TextureAtlas/TextureAtlasBuilder/TextureAtlasD3D9.cpp"
        "${BF2_ROOT}/RendDX9/TextureAtlas/TextureAtlasBuilder/TextureFormat.cpp"
    )
    target_sources(bf2_renddx9_static PRIVATE ${BF2_TEXTURE_ATLAS_NATIVE_SOURCES})
    # The host manifests also list these paths; keep their builder image guard inactive.
    set_source_files_properties(${BF2_TEXTURE_ATLAS_NATIVE_SOURCES}
        PROPERTIES COMPILE_DEFINITIONS
        "$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,bf2_renddx9_static>:BF2_IMAGE_TEXTUREATLASBUILDER=1>")
    # [CHOSEN, not recovered] One renderer-only prefix selects the subset without TU switches.
    target_include_directories(bf2_renddx9_static BEFORE PRIVATE
        "${BF2_ROOT}/Platform/Gfx/MacInclude" "${BF2_ROOT}/Platform/Gfx/Include")
    target_sources(bf2_renddx9_static PRIVATE
        "${BF2_ROOT}/Platform/Gfx/Metal/Frontiers.cpp"
        "${BF2_ROOT}/Platform/Gfx/Metal/Effect.cpp"
        "${BF2_ROOT}/Platform/Gfx/Metal/D3DXMath.cpp"
        "${BF2_ROOT}/Platform/Gfx/Metal/D3DXGeometry.cpp"
        "${BF2_ROOT}/Platform/Gfx/Metal/D3DXLine.cpp"
        "${BF2_ROOT}/Platform/Gfx/Metal/D3DXPixels.cpp"
        "${BF2_ROOT}/Platform/Gfx/Metal/D3DXImages.cpp"
        "${BF2_ROOT}/Platform/Gfx/Metal/D3DXTextures.cpp"
        "${BF2_ROOT}/Platform/Gfx/Metal/D3DXMesh.cpp"
        "${BF2_ROOT}/Platform/Gfx/Metal/Device.cpp"
        "${BF2_ROOT}/Platform/Gfx/Metal/FFVertex.cpp"
        "${BF2_ROOT}/Platform/Gfx/Metal/MetalBridge.mm"
        # zeux/meshoptimizer v1.3 (MIT, External/meshoptimizer/LICENSE.md): the stripifier behind
        # the native D3DXConvertMeshSubsetToSingleStrip in D3DXMesh.cpp.
        "${BF2_EXTERNAL_ROOT}/meshoptimizer/stripifier.cpp")
    set_source_files_properties("${BF2_ROOT}/Platform/Gfx/Metal/MetalBridge.mm"
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_compile_options(bf2_renddx9_static PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:-include${BF2_ROOT}/Platform/Gfx/MacSupport.h>")
    target_compile_definitions(bf2_renddx9_static PRIVATE
        initDll=bf2_renddx9_initDll deinitDll=bf2_renddx9_deinitDll)

    add_library(bf2_swiffplayer_static STATIC EXCLUDE_FROM_ALL ${BF2_SWIFFPLAYER_STATIC_SOURCES})
    bf2_configure_static_plugin(bf2_swiffplayer_static BF2_IMAGE_SWIFFPLAYER)
    target_compile_definitions(bf2_swiffplayer_static PRIVATE
        initDll=bf2_swiffplayer_initDll deinitDll=bf2_swiffplayer_deinitDll)
    target_include_directories(bf2_swiffplayer_static PRIVATE
        "${BF2_EXTERNAL_ROOT}" "${BF2_EXTERNAL_ROOT}/gameswf")

    # [CHOSEN, not recovered] Owner-approved Expat 1.95.0 substitute. The other
    # xmltok .c files are included fragments, not separate translation units.
    add_library(bf2_expat_static STATIC EXCLUDE_FROM_ALL
        "${BF2_EXTERNAL_ROOT}/expat/xmlparse.c"
        "${BF2_EXTERNAL_ROOT}/expat/xmltok.c"
        "${BF2_EXTERNAL_ROOT}/expat/xmlrole.c")
    bf2_configure_c_target(bf2_expat_static)
    bf2_set_image(bf2_expat_static 0)
    target_include_directories(bf2_expat_static PRIVATE "${BF2_EXTERNAL_ROOT}/expat")
    set_property(TARGET bf2_expat_static PROPERTY FOLDER "BF2")
    target_link_libraries(bf2_swiffplayer_static PUBLIC bf2_expat_static)

    add_library(bf2_vorbis_static STATIC EXCLUDE_FROM_ALL ${BF2_VORBIS_STATIC_SOURCES})
    bf2_configure_c_target(bf2_vorbis_static)
    target_include_directories(bf2_vorbis_static PRIVATE
        "${BF2_EXTERNAL_ROOT}/libogg-1.3.6/include"
        "${BF2_EXTERNAL_ROOT}/libvorbis-1.3.7/include"
        "${BF2_EXTERNAL_ROOT}/libvorbis-1.3.7/lib")
    set_property(TARGET bf2_vorbis_static PROPERTY FOLDER "BF2")

    # [CHOSEN, not recovered] FFmpeg 7.1.1's Bink video/audio decoders, unmodified, over the
    # OpenBF2 shim (Code/BF2/External/ffmpeg-bink/README.md; LGPL-2.1-or-later). This is the macOS
    # build; Windows x64 compiles the same files in Code/Build/renddx9.cmd, Windows x86 keeps RAD's
    # binkw32.dll. Built optimized in every configuration, as FFmpeg is.
    set(BF2_FFMPEG_BINK_DIR "${BF2_EXTERNAL_ROOT}/ffmpeg-bink")
    add_library(bf2_ffmpeg_bink STATIC EXCLUDE_FROM_ALL
        "${BF2_FFMPEG_BINK_DIR}/libavcodec/bink.c"
        "${BF2_FFMPEG_BINK_DIR}/libavcodec/binkdsp.c"
        "${BF2_FFMPEG_BINK_DIR}/libavcodec/binkaudio.c"
        "${BF2_FFMPEG_BINK_DIR}/libavcodec/wma_freqs.c"
        "${BF2_FFMPEG_BINK_DIR}/shim/ffbink_shim.c")
    target_include_directories(bf2_ffmpeg_bink PRIVATE "${BF2_FFMPEG_BINK_DIR}/shim")
    set_target_properties(bf2_ffmpeg_bink PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON
        C_EXTENSIONS ON)
    target_compile_options(bf2_ffmpeg_bink PRIVATE -O2 -fno-strict-aliasing -fno-math-errno
        -Wno-pointer-sign)
    set_property(TARGET bf2_ffmpeg_bink PROPERTY FOLDER "BF2")
    # [CHOSEN, not recovered] CPU check of the Bink port against reference frames/samples that
    # the local ffmpeg CLI produces from the read-only game content (Code/BF2/External/ffmpeg-bink/README.md).
    add_executable(bf2_bink_decode_test EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_SOURCE_DIR}/Code/Tests/macos_bink_decode.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Code/RendDX9Abi/BinkFFmpeg.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Code/RendDX9Abi/BinkAudioCoreAudio.cpp")
    target_compile_features(bf2_bink_decode_test PRIVATE cxx_std_17)
    target_link_libraries(bf2_bink_decode_test PRIVATE bf2_ffmpeg_bink "-framework AudioToolbox")
    # [TEMP] [CHOSEN, not recovered] The black-box Bink harness, port side: the same source the
    # retail build runs against binkw32.dll (Code/Tests/movies, Code/BF2/External/ffmpeg-bink/README.md).
    add_executable(bf2_bink_harness EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_SOURCE_DIR}/Code/Tests/movies/bink_harness.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Code/RendDX9Abi/BinkFFmpeg.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Code/RendDX9Abi/BinkAudioCoreAudio.cpp")
    target_compile_features(bf2_bink_harness PRIVATE cxx_std_17)
    target_link_libraries(bf2_bink_harness PRIVATE bf2_ffmpeg_bink "-framework AudioToolbox")

    add_library(bf2_bf2audio_static STATIC EXCLUDE_FROM_ALL ${BF2_BF2AUDIO_STATIC_SOURCES})
    bf2_configure_static_plugin(bf2_bf2audio_static "BF2_IMAGE_BF2AUDIO")
    target_include_directories(bf2_bf2audio_static PRIVATE
        "${BF2_EXTERNAL_ROOT}/libogg-1.3.6/include"
        "${BF2_EXTERNAL_ROOT}/libvorbis-1.3.7/include")

    # The client-image half of the macOS voice server, statically linked as the Linux server does.
    add_library(bf2_voipserver_client_objects OBJECT EXCLUDE_FROM_ALL ${bf2_voipserver_only_sources})
    bf2_configure_cpp_target(bf2_voipserver_client_objects)
    bf2_set_image(bf2_voipserver_client_objects 0)
    bf2_set_platform_compat(bf2_voipserver_client_objects)
    target_compile_definitions(bf2_voipserver_client_objects PRIVATE BF2_PLATFORM_POSIX _MACOSX)
    set_property(TARGET bf2_voipserver_client_objects PROPERTY FOLDER "BF2")

    add_executable(bf2_client EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_SOURCE_DIR}/Code/StaticPlugins/StaticPlugins.cpp"
        $<TARGET_OBJECTS:bf2_client_objects>
        $<TARGET_OBJECTS:bf2_voipserver_client_objects>
        $<TARGET_OBJECTS:bf2_zlib_objects>
        $<TARGET_OBJECTS:bf2_tinyxml_objects>
        $<TARGET_OBJECTS:bf2_gamespy_objects>
        $<TARGET_OBJECTS:bf2_gamespy_sb_objects>
        $<TARGET_OBJECTS:bf2_gamespy_gp_objects>
        $<TARGET_OBJECTS:bf2_gamespy_cdkeyc_objects>
        $<TARGET_OBJECTS:bf2_gamespy_voice_objects>
    )
    bf2_configure_static_plugin(bf2_client "")
    # bin_macos already holds the server `bf2`; APFS is case-insensitive, so the client cannot be
    # `BF2` beside it. It shares the directory, and the @loader_path rpath, with libdice_py.dylib.
    set_target_properties(bf2_client PROPERTIES
        LINKER_LANGUAGE CXX
        OUTPUT_NAME bf2_client
        RUNTIME_OUTPUT_DIRECTORY "${BF2_DEDICATED_OUTPUT_DIR}"
        BUILD_RPATH "@loader_path"
        INSTALL_RPATH "@loader_path"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
    if(TARGET dice_py)
        target_link_libraries(bf2_client PRIVATE dice_py)
    endif()
    # [CHOSEN, not recovered] The renderer uses the G2 subset and G3 Metal device;
    # unimplemented operations stop. OFF keeps the
    # renderer-free client: StaticPlugins.cpp has no RendDX9 entry and BF2Render::init takes
    # its shipped failure return. ON links the image for coordinator device-creation checks.
    option(BF2_MACOS_LINK_RENDDX9 "Link the RendDX9 static library into the macOS client" OFF)
    if(BF2_MACOS_LINK_RENDDX9)
        bf2_prelink_static_plugin(bf2_client bf2_renddx9_static "${BF2_STATIC_PLUGIN_PRIVATE_SYMBOLS}")
        target_compile_definitions(bf2_client PRIVATE BF2_STATIC_RENDDX9=1)
        # [CHOSEN, not recovered] The renderer's Bink port (BinkFFmpeg.cpp) and its audio output.
        target_link_libraries(bf2_client PRIVATE bf2_ffmpeg_bink "-framework AudioToolbox")
        # [CHOSEN, not recovered] The G4 effect facade (Metal/Effect.cpp) runs the s2 CPU runtime.
        target_link_libraries(bf2_client PRIVATE bf2_fx_runtime)
        # [CHOSEN, not recovered] System JPEG decoding in the prelinked renderer image.
        target_link_libraries(bf2_client PRIVATE "-framework ImageIO" "-framework CoreGraphics")
    endif()
    target_compile_definitions(bf2_client PRIVATE BF2_STATIC_SWIFFPLAYER=1 BF2_STATIC_BF2AUDIO=1)
    bf2_prelink_static_plugin(bf2_client bf2_swiffplayer_static "${BF2_STATIC_PLUGIN_PRIVATE_SYMBOLS}")
    bf2_prelink_static_plugin(bf2_client bf2_bf2audio_static
        "${CMAKE_CURRENT_SOURCE_DIR}/Code/StaticPlugins/bf2audio-private-symbols.txt")
    target_link_libraries(bf2_client PRIVATE
        bf2_vorbis_static bf2_expat_static bf2_plugin_kit_static
        Threads::Threads ${CMAKE_DL_LIBS} ${CURSES_LIBRARIES})
    target_link_options(bf2_client PRIVATE
        "-Wl,-map,${BF2_DEDICATED_OUTPUT_DIR}/bf2_client.map")

    # [CHOSEN, not recovered] The Cocoa host and the GCController poller (package S2.4,
    # docs/macos-s2-4.md): ARC Objective-C++ with the plain-C++ headers the engine includes.
    set(BF2_MACOS_HOST_OBJCXX_SOURCES
        "${BF2_ROOT}/Platform/macOS/CocoaHost.mm"
        "${BF2_ROOT}/Platform/macOS/MacGamepad.mm"
        "${BF2_ROOT}/Platform/macOS/MacRawMouse.mm"
        "${BF2_ROOT}/Platform/macOS/MacInputScript.mm")      # [TEMP] BF2_INPUT_SCRIPT
    target_sources(bf2_client PRIVATE ${BF2_MACOS_HOST_OBJCXX_SOURCES})
    set_source_files_properties(${BF2_MACOS_HOST_OBJCXX_SOURCES}
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(bf2_client PRIVATE
        "-framework AppKit" "-framework QuartzCore" "-framework Metal"
        "-framework GameController" "-framework Carbon" "-framework IOKit")
    # [CHOSEN, not recovered] BF2Audio's POSIX arm (Sound/SoundEngine/Win32System.cpp) queries the
    # default output device through CoreAudio; OpenAL.framework itself is loaded at run time with
    # dlopen, as the Win32 image loads OpenAL32.dll. MacSystem.cpp's alert uses CoreFoundation.
    target_link_libraries(bf2_client PRIVATE "-framework CoreAudio" "-framework CoreFoundation")
    # The SDK's CoreAudio voice layer (gvOSXAudio.c) converts sample rates with AudioConverter.
    target_link_libraries(bf2_client PRIVATE "-framework AudioToolbox")

    # [CHOSEN, not recovered] Keep all static-image views in native ABI agreement.
    # This also runs for renderer-OFF builds. The script caches successful probes
    # against real target flags and transitive headers; a warm gate takes seconds.
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    add_executable(bf2_native_vtable_check EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_SOURCE_DIR}/Code/Tests/macos_vtable_audit.cpp")
    bf2_configure_cpp_target(bf2_native_vtable_check)
    bf2_set_image(bf2_native_vtable_check 0)
    target_compile_definitions(bf2_native_vtable_check PRIVATE BF2_STATIC_PLUGINS=1)
    # [CHOSEN, not recovered] Pin captured BFMeme dispatch to native member encodings.
    add_executable(bf2_meme_dispatch_check EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_SOURCE_DIR}/Code/Tests/macos_meme_dispatch.cpp")
    bf2_configure_cpp_target(bf2_meme_dispatch_check)
    bf2_set_image(bf2_meme_dispatch_check 0)
    # A cross build (an arm64-only tree on an Intel Mac) cannot execute the native checks:
    # the static audit still gates the build with the target's own flags, and the linked
    # checks ship in the M2 bundle (docs/macos-c-arm64-client.md).
    set(BF2_VTABLE_CHECK_RUNNABLE ON)
    if(CMAKE_OSX_ARCHITECTURES AND NOT CMAKE_HOST_SYSTEM_PROCESSOR IN_LIST CMAKE_OSX_ARCHITECTURES)
        set(BF2_VTABLE_CHECK_RUNNABLE OFF)
    endif()
    if(BF2_VTABLE_CHECK_RUNNABLE)
        set(BF2_VTABLE_CHECK_COMMAND COMMAND "$<TARGET_FILE:bf2_native_vtable_check>"
            COMMAND "$<TARGET_FILE:bf2_meme_dispatch_check>")
    else()
        set(BF2_VTABLE_CHECK_COMMAND COMMAND "${CMAKE_COMMAND}" -E echo
            "bf2_native_vtable_check and bf2_meme_dispatch_check are built for ${CMAKE_OSX_ARCHITECTURES} and run in the M2 bundle")
    endif()
    add_custom_target(bf2_vtable_audit ALL
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/macos/vtable_audit.py"
                --build-dir "${CMAKE_BINARY_DIR}"
        # AI behaviour system (S0): native vtable order against libaidll.so's
        # (tools/macos/ai_behaviour_vtables.json); clang computes the slots, cached.
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/macos/ai_behaviour_vtables.py"
                --build-dir "${CMAKE_BINARY_DIR}"
        ${BF2_VTABLE_CHECK_COMMAND}
        DEPENDS bf2_native_vtable_check bf2_meme_dispatch_check
        COMMENT "Auditing native static-plugin vtables"
        VERBATIM)
    add_dependencies(bf2_client bf2_vtable_audit)
    # [CHOSEN, not recovered] A game hosted from the client menu (the welcome page's instant
    # battle, CREATE LOCAL) runs the server inside bf2_client, and BF2Engine names its AI image
    # "aidll.dll" (BF2.exe 0x409CBD) - retail ships AIDLL.dll beside BF2.exe. The POSIX loader maps
    # that to libaidll.dylib, found through the client's @loader_path rpath in bin_macos, where
    # bf2_aidll is written. Without it AIConnection::init aborts (the shipped load-failure defect).
    if(TARGET bf2_aidll)
        add_dependencies(bf2_client bf2_aidll)
    endif()
    add_dependencies(bf2 bf2_vtable_audit)
    add_dependencies(bf2_renddx9_static bf2_vtable_audit)
    add_dependencies(bf2_swiffplayer_static bf2_vtable_audit)
    add_dependencies(bf2_bf2audio_static bf2_vtable_audit)
endif()

# =================================================================================================
# RUNNING IT. ⚠⚠ THE WORKING DIRECTORY IS NOT A CONVENIENCE - IT IS THE ENGINE'S ROOT.
#
# Every path the server resolves is relative to the process's current directory: `python/bf2/...`
# for the embedded interpreter's modules, `mods/bf2/...` for the mod, its `.con` scripts and its
# `*.desc` files, and `Levels/...` for the map. Start it anywhere else and the first thing that
# happens is CPython failing to `import bf2`, long before any of the recovered engine runs.
# ★ So the exe stays in its architecture-specific build tree - it must NEVER be written into the
# game folder, which holds the original x86 reference image and x86 DLLs - and only the CWD points
# at the game folder. The architecture-matched dice_py.dll is staged beside the exe precisely so
# Windows finds it before anything in that content directory.
#
# ★ THE ARGUMENTS ARE THE PRODUCTION COMMAND LINE, the one AGENTS.md's first milestone names:
# the real BF2::init -> BF2Engine::startGame -> GameServer path with a stock map and stock gpm_cq.
# Blank BF2_RUN_ARGS to get the bare no-drive boot instead.
# =================================================================================================
set(BF2_RUN_ARGS "+modPath;mods/bf2;+loadLevel;dalian_plant;+gameMode;gpm_cq;+maxPlayers;16"
    CACHE STRING
    "Command line for the selected dedicated run target (a CMake ;-list). Empty = no-drive boot.")

set(BF2_DEDICATED_RUN_TARGET "${BF2_DEDICATED_TARGET}_run")
add_custom_target("${BF2_DEDICATED_RUN_TARGET}"
    COMMAND "$<TARGET_FILE:${BF2_DEDICATED_TARGET}>" ${BF2_RUN_ARGS}
    DEPENDS "${BF2_DEDICATED_TARGET}"
    WORKING_DIRECTORY "${BF2_GAME_DIR}"
    USES_TERMINAL
    VERBATIM
)
set_property(TARGET "${BF2_DEDICATED_RUN_TARGET}" PROPERTY FOLDER "BF2")

# ⚠ CLion's own Run button on an executable target does not use any of the above - it
# builds its own configuration and defaults the working directory to the build tree, which is
# exactly the "it starts and immediately cannot find python/" failure. Architecture-specific
# `.run/* (game dir).run.xml` files are checked in so the IDE gets the game folder and arguments;
# `.idea/` is gitignored, `.run/` is not, which is why the configurations live there.
# For the MSVC generator, do the same for the debugger:
set_target_properties("${BF2_DEDICATED_TARGET}" PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${BF2_GAME_DIR}"
    VS_DEBUGGER_COMMAND_ARGUMENTS "+modPath mods/bf2 +loadLevel dalian_plant +gameMode gpm_cq +maxPlayers 16"
)

if(WIN32)
# The batch linker remains authoritative for unresolved-symbol progress. This
# target is expected to be red until the reconstruction reaches zero gaps.
add_custom_target(bf2_gap_report
    COMMAND cmd.exe /d /c build.cmd
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Code/Build"
    USES_TERMINAL
    VERBATIM
)
set_property(TARGET bf2_gap_report PROPERTY FOLDER "Diagnostics")

# Parity check for the original compile gate. The native bf2_recovered target
# is the normal CMake compile path; this target verifies the batch gate too.
add_custom_target(bf2_syntaxcheck
    COMMAND cmd.exe /d /c syntaxcheck.cmd
    WORKING_DIRECTORY "${BF2_STUB_ROOT}"
    USES_TERMINAL
    VERBATIM
)
set_property(TARGET bf2_syntaxcheck PROPERTY FOLDER "Diagnostics")
endif()

include(CTest)

# [TEMP] Issue #33: content-free policy checks, with no renderer or gameplay claim.
add_executable(bf2_hud_widescreen_check EXCLUDE_FROM_ALL
    Code/Tests/hud_widescreen_policy.cpp
    "${BF2_ROOT}/Menu/HudWidescreen.cpp")
bf2_configure_cpp_target(bf2_hud_widescreen_check)
bf2_set_platform_compat(bf2_hud_widescreen_check)
bf2_set_image(bf2_hud_widescreen_check 0)
if(BUILD_TESTING)
    add_test(NAME bf2_hud_widescreen_check COMMAND bf2_hud_widescreen_check)
endif()

if(BF2_BUILD_MACOS AND TARGET bf2_swiffplayer_static)
    # [TEMP] [CHOSEN, not recovered] Content-free offline popup VM regression.
    add_executable(bf2_swiff_offline_connect_popup_check EXCLUDE_FROM_ALL
        Code/Tests/swiff_offline_connect_popup.cpp
        Code/Tests/gameswf_xml/cocoa_host_unavailable.cpp)
    bf2_configure_static_plugin(bf2_swiff_offline_connect_popup_check BF2_IMAGE_SWIFFPLAYER)
    target_include_directories(bf2_swiff_offline_connect_popup_check PRIVATE
        "${BF2_EXTERNAL_ROOT}" "${BF2_EXTERNAL_ROOT}/gameswf")
    target_link_libraries(bf2_swiff_offline_connect_popup_check PRIVATE
        bf2_swiffplayer_static bf2_plugin_kit_static)

    # [TEMP] [CHOSEN, not recovered] The __proto__ probe SWFs (tools/swf/proto_probe.py) in the
    # native gameswf must show retail 1.50's squares (docs/tracks/game/swf-proto-retail-probe.md).
    add_executable(bf2_swf_proto_probe_check EXCLUDE_FROM_ALL
        Code/Tests/swf_proto_probe_runtime.cpp
        Code/Tests/gameswf_xml/cocoa_host_unavailable.cpp)
    bf2_configure_static_plugin(bf2_swf_proto_probe_check BF2_IMAGE_SWIFFPLAYER)
    target_include_directories(bf2_swf_proto_probe_check PRIVATE
        "${BF2_EXTERNAL_ROOT}" "${BF2_EXTERNAL_ROOT}/gameswf")
    target_link_libraries(bf2_swf_proto_probe_check PRIVATE bf2_swiffplayer_static bf2_plugin_kit_static)
    if(BUILD_TESTING)
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(Python3_Interpreter_FOUND)
            add_test(NAME bf2_swf_proto_probe_generate
                COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/tools/swf/proto_probe.py"
                    swf --out "${CMAKE_CURRENT_BINARY_DIR}/swf-proto-probe")
            add_test(NAME bf2_swf_proto_probe_check
                COMMAND bf2_swf_proto_probe_check "${CMAKE_CURRENT_BINARY_DIR}/swf-proto-probe")
            set_tests_properties(bf2_swf_proto_probe_check PROPERTIES DEPENDS bf2_swf_proto_probe_generate)
        endif()
    endif()

    # [TEMP] [CHOSEN, not recovered] Production gameswf XML/string/file fixture.
    add_executable(bf2_gameswf_xml_check EXCLUDE_FROM_ALL
        Code/Tests/gameswf_xml/xml_check.cpp
        Code/Tests/gameswf_xml/cocoa_host_unavailable.cpp)
    bf2_configure_static_plugin(bf2_gameswf_xml_check BF2_IMAGE_SWIFFPLAYER)
    target_include_directories(bf2_gameswf_xml_check PRIVATE "${BF2_EXTERNAL_ROOT}")
    target_link_libraries(bf2_gameswf_xml_check PRIVATE bf2_swiffplayer_static bf2_plugin_kit_static)
    if(BUILD_TESTING)
        add_test(NAME bf2_gameswf_xml_check COMMAND bf2_gameswf_xml_check
            "${CMAKE_CURRENT_BINARY_DIR}/gameswf-xml-fixture.xml")
    endif()
endif()

# [TEMP] Behaviour fixture for the recovered slider bodies (Code/Tests/slider_node_behaviour.cpp, game
# frontier closure; Code/Tests/slider_node_behaviour.cmd is its Windows twin). The BFMeme families it
# never calls stay unresolved, as /FORCE:UNRESOLVED leaves them in the Windows fixtures.
if(APPLE)
    add_executable(bf2_slider_node_check EXCLUDE_FROM_ALL
        Code/Tests/slider_node_behaviour.cpp
        "${BF2_ROOT}/External/BFMeme/SliderNode.cpp"
        "${BF2_ROOT}/Menu/BfFixedSliderNode.cpp"
        "${BF2_ROOT}/External/BFMeme/Node.cpp"
        "${BF2_ROOT}/External/BFMeme/NodeState.cpp"
        "${BF2_ROOT}/External/BFMeme/Object.cpp"
        "${BF2_ROOT}/External/BFMeme/TypeDescriptor.cpp"
        "${BF2_ROOT}/External/BFMeme/System.cpp"
        "${BF2_ROOT}/External/BFMeme/Data.cpp"
        "${BF2_ROOT}/External/BFMeme/NamedObjectRegistry.cpp"
        "${BF2_ROOT}/External/BFMeme/Stream.cpp")
    bf2_configure_cpp_target(bf2_slider_node_check)
    bf2_set_platform_compat(bf2_slider_node_check)
    bf2_set_image(bf2_slider_node_check 0)
    target_link_options(bf2_slider_node_check PRIVATE "-Wl,-undefined,dynamic_lookup")
    if(BUILD_TESTING)
        add_test(NAME bf2_slider_node_check COMMAND bf2_slider_node_check)
    endif()
endif()

# [TEMP] [CHOSEN, not recovered] Receive-size limit of DataBlockManager (Code/Tests/datablock_cap_runtime.cpp):
# the actual DataBlockManager/DataBlockEvent/BitStream with a recording GameEventManager.
if(APPLE)
    add_executable(bf2_datablock_cap_check EXCLUDE_FROM_ALL
        Code/Tests/datablock_cap_runtime.cpp
        "${BF2_ROOT}/Game/Common/DataBlockManager.cpp"
        "${BF2_ROOT}/Game/GameEvents/DataBlockEvent.cpp"
        "${BF2_ROOT}/Game/Common/GameEvent.cpp"
        "${BF2_ROOT}/Core/EventManager.cpp"
        "${BF2_ROOT}/IO/BitStream.cpp")
    bf2_configure_cpp_target(bf2_datablock_cap_check)
    bf2_set_platform_compat(bf2_datablock_cap_check)
    bf2_set_image(bf2_datablock_cap_check 0)
    if(BUILD_TESTING)
        add_test(NAME bf2_datablock_cap_check COMMAND bf2_datablock_cap_check)
    endif()
endif()

# [TEMP] Cumulative fixture for game frontier-closure sites no normal session reaches
# (Code/Tests/game_frontier_check.cpp; docs/tracks/game/frontier-closure.md). Built on request; the
# families it never calls stay unresolved, as /FORCE:UNRESOLVED leaves them in the Windows fixtures.
if(APPLE)
    add_executable(bf2_game_frontier_check EXCLUDE_FROM_ALL
        Code/Tests/game_frontier_check.cpp
        Code/Tests/game_frontier_x87_narrow.cpp
        "${BF2_ROOT}/Game/LocalProfileManager.cpp"
        "${BF2_ROOT}/Game/LocalProfileManagerDiscovery.cpp"
        "${BF2_ROOT}/Game/LocalProfileManagerFields.cpp"
        "${BF2_ROOT}/Game/LocalProfileManagerInit.cpp"
        "${BF2_ROOT}/Game/LocalProfileManagerOperations.cpp"
        "${BF2_ROOT}/Game/LocalProfileManagerSelection.cpp"
        "${BF2_ROOT}/Game/LocalProfileManagerSuffix.cpp"
        "${BF2_ROOT}/Physics/MaterialManager.cpp"
        "${BF2_ROOT}/External/BFMeme/Node.cpp"
        "${BF2_ROOT}/External/BFMeme/NodeState.cpp"
        "${BF2_ROOT}/External/BFMeme/Object.cpp"
        "${BF2_ROOT}/External/BFMeme/TypeDescriptor.cpp"
        "${BF2_ROOT}/External/BFMeme/System.cpp"
        "${BF2_ROOT}/External/BFMeme/NamedObjectRegistry.cpp"
        "${BF2_ROOT}/External/BFMeme/Data.cpp"
        "${BF2_ROOT}/Menu/BfEditNode.cpp"
        "${BF2_ROOT}/Menu/Bf2EditNode.cpp"
        "${BF2_ROOT}/Menu/BfNewListBoxNode.cpp"
        "${BF2_ROOT}/Menu/BfListBoxData.cpp"
        "${BF2_ROOT}/External/BFMeme/Action.cpp"
        "${BF2_ROOT}/External/BFMeme/Tree.cpp"
        "${BF2_ROOT}/External/BFMeme/Stream.cpp"
        "${BF2_ROOT}/Menu/ProfileConsoleBindings.cpp"
        "${BF2_ROOT}/Core/ConsoleObjectBase.cpp"
        "${BF2_ROOT}/Game/DistributedLightRenderer/DistributedLightRenderer.cpp"
        "${BF2_ROOT}/Game/Common/GameEvent.cpp"
        "${BF2_ROOT}/Core/StringUtil.cpp"
        "${BF2_ROOT}/AI/AIInterface/BFKnowledge/BFGame.cpp")
    bf2_configure_cpp_target(bf2_game_frontier_check)
    bf2_set_platform_compat(bf2_game_frontier_check)
    bf2_set_image(bf2_game_frontier_check 0)
    set_source_files_properties(Code/Tests/game_frontier_x87_narrow.cpp PROPERTIES
        COMPILE_OPTIONS "-Wno-builtin-macro-redefined;-U__LDBL_MANT_DIG__;-D__LDBL_MANT_DIG__=53")
    target_link_options(bf2_game_frontier_check PRIVATE "-Wl,-undefined,dynamic_lookup")
    if(BUILD_TESTING)
        add_test(NAME bf2_game_frontier_check COMMAND bf2_game_frontier_check)
    endif()
endif()

# [CHOSEN, not recovered] The frontier trap's default (Code/Tests/frontier_default_check.cpp): the kit's
# Frontier.cpp built as a dev image (default abort) and as a release image
# (BF2_FRONTIER_DEFAULT_CONTINUE=1, default continue); explicit env values win in both.
if(APPLE)
    foreach(bf2_frontier_mode IN ITEMS abort continue)
        set(bf2_frontier_target bf2_frontier_default_${bf2_frontier_mode}_check)
        add_executable(${bf2_frontier_target} EXCLUDE_FROM_ALL
            "${CMAKE_CURRENT_SOURCE_DIR}/Code/Tests/frontier_default_check.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/Code/RendDX9Abi/Frontier.cpp")
        if(bf2_frontier_mode STREQUAL "continue")
            target_compile_definitions(${bf2_frontier_target} PRIVATE
                BF2_FRONTIER_DEFAULT_CONTINUE=1 BF2_FRONTIER_CHECK_EXPECT_CONTINUE=1)
        else()
            target_compile_definitions(${bf2_frontier_target} PRIVATE BF2_FRONTIER_CHECK_EXPECT_CONTINUE=0)
        endif()
        if(BUILD_TESTING)
            add_test(NAME ${bf2_frontier_target} COMMAND ${bf2_frontier_target})
        endif()
    endforeach()
endif()

# [TEMP] The voice client (Code/Tests/voip_client_runtime.cpp): VoipClient over loopback UDP and
# BF2VoipClient on fake game objects, with the synthetic capture device (no microphone, no permission
# prompt). The mixer (Code/Tests/voip_mixer_runtime.cpp) runs on its scripted WINMM table.
if(APPLE)
    add_executable(bf2_voip_client_check EXCLUDE_FROM_ALL
        Code/Tests/voip_client_runtime.cpp
        "${BF2_ROOT}/Game/VoipClient/VoipClient.cpp"
        "${BF2_ROOT}/Game/VoipClient/BF2VoipClient.cpp"
        "${BF2_ROOT}/Game/VoipClient/VoipMixer.cpp"
        "${BF2_ROOT}/Game/VoipClient/VoipFakeDevice.cpp"
        $<TARGET_OBJECTS:bf2_gamespy_voice_objects>
        $<TARGET_OBJECTS:bf2_gamespy_objects>)
    bf2_configure_cpp_target(bf2_voip_client_check)
    bf2_set_platform_compat(bf2_voip_client_check)
    bf2_set_image(bf2_voip_client_check 0)
    target_compile_definitions(bf2_voip_client_check PRIVATE BF2_PLATFORM_POSIX _MACOSX
        BF2_VOIPMIXER_NO_PLATFORM_BACKEND=1)
    target_link_options(bf2_voip_client_check PRIVATE "-Wl,-undefined,dynamic_lookup")
    target_link_libraries(bf2_voip_client_check PRIVATE "-framework CoreAudio" "-framework AudioToolbox"
        "-framework CoreFoundation" Threads::Threads)
    if(BUILD_TESTING)
        add_test(NAME bf2_voip_client_check COMMAND bf2_voip_client_check)
    endif()

    # The retail x86 layout asserts of the voice client, compiled for the MSVC x86 ABI from the Mac
    # (clang's i686-pc-windows-msvc target, stub STL headers); the real check is the Windows x86 build.
    if(BUILD_TESTING AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_test(NAME bf2_voip_msvc_x86_layout
            COMMAND "${CMAKE_CXX_COMPILER}" -target i686-pc-windows-msvc -fsyntax-only -std=c++14 -Wno-invalid-offsetof -Wno-unused-command-line-argument
                -nostdinc -nostdinc++ -I "${CMAKE_CURRENT_SOURCE_DIR}/Code/Tests/msvc_x86_layout/include"
                -D_M_IX86=600 "${CMAKE_CURRENT_SOURCE_DIR}/Code/Tests/msvc_x86_layout/msvc_x86_layout.cpp")
    endif()

    add_executable(bf2_voip_mixer_check EXCLUDE_FROM_ALL
        Code/Tests/voip_mixer_runtime.cpp
        "${BF2_ROOT}/Game/VoipClient/VoipMixer.cpp")
    bf2_configure_cpp_target(bf2_voip_mixer_check)
    bf2_set_platform_compat(bf2_voip_mixer_check)
    bf2_set_image(bf2_voip_mixer_check 0)
    target_compile_definitions(bf2_voip_mixer_check PRIVATE BF2_VOIPMIXER_NO_PLATFORM_BACKEND=1)
    if(BUILD_TESTING)
        add_test(NAME bf2_voip_mixer_check COMMAND bf2_voip_mixer_check)
    endif()
endif()

# [TEMP] The PunkBuster glue objects at the 32-bit x86 ABI (Code/Tests/msvc_x86_layout/punkbuster_x86_layout.cpp).
if(BUILD_TESTING AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    foreach(pb_abi IN ITEMS msvc elf)
        if(pb_abi STREQUAL "msvc")
            set(pb_triple i686-pc-windows-msvc)
            set(pb_define -D_M_IX86=600)
        else()
            set(pb_triple i386-pc-linux-gnu)
            set(pb_define -D__i386__=1)
        endif()
        add_test(NAME bf2_punkbuster_${pb_abi}_x86_layout
            COMMAND "${CMAKE_CXX_COMPILER}" -target ${pb_triple} -fsyntax-only -std=c++14 -Wno-invalid-offsetof -Wno-unused-command-line-argument -Wno-macro-redefined
                -nostdinc -nostdinc++ -I "${CMAKE_CURRENT_SOURCE_DIR}/Code/Tests/msvc_x86_layout/include"
                ${pb_define} "${CMAKE_CURRENT_SOURCE_DIR}/Code/Tests/msvc_x86_layout/punkbuster_x86_layout.cpp")
    endforeach()
endif()

# [TEMP] PunkBuster client glue and command hook (Code/Tests/punkbuster_client_runtime.cpp) against a
# fake pbcl.dll through the glue's library seam, fake PB_* functions and a fake server trap. Built on
# request; files only under a fresh temporary directory.
if(APPLE)
    add_executable(bf2_punkbuster_client_check EXCLUDE_FROM_ALL
        Code/Tests/punkbuster_client_runtime.cpp
        "${BF2_ROOT}/Game/PunkBuster/PbClient.cpp"
        "${BF2_ROOT}/Game/PunkBuster/PbCommandHook.cpp")
    bf2_configure_cpp_target(bf2_punkbuster_client_check)
    bf2_set_platform_compat(bf2_punkbuster_client_check)
    bf2_set_image(bf2_punkbuster_client_check 0)
    if(BUILD_TESTING)
        add_test(NAME bf2_punkbuster_client_check COMMAND bf2_punkbuster_client_check)
    endif()
endif()

# [TEMP] The BF2-side PunkBuster functions (Code/Tests/punkbuster_game_runtime.cpp) against fake games,
# console, players, glue entry points and helpers, in the client and the dedicated image. Built on request.
if(APPLE)
    add_executable(bf2_punkbuster_game_check EXCLUDE_FROM_ALL
        Code/Tests/punkbuster_game_runtime.cpp
        "${BF2_ROOT}/Game/PunkBuster/PbGame.cpp"
        "${BF2_ROOT}/Game/Objects/IPlayer.cpp")
    bf2_configure_cpp_target(bf2_punkbuster_game_check)
    bf2_set_platform_compat(bf2_punkbuster_game_check)
    bf2_set_image(bf2_punkbuster_game_check 0)
    target_include_directories(bf2_punkbuster_game_check PRIVATE ${BF2_PYTHON_INCLUDE_DIRS})
    if(BUILD_TESTING)
        add_test(NAME bf2_punkbuster_game_check COMMAND bf2_punkbuster_game_check)
    endif()
    add_executable(bf2_punkbuster_game_dedicated_check EXCLUDE_FROM_ALL
        Code/Tests/punkbuster_game_runtime.cpp
        "${BF2_ROOT}/Game/PunkBuster/PbGame.cpp"
        "${BF2_ROOT}/Game/Objects/IPlayer.cpp")
    bf2_configure_cpp_target(bf2_punkbuster_game_dedicated_check)
    bf2_set_platform_compat(bf2_punkbuster_game_dedicated_check)
    bf2_set_image(bf2_punkbuster_game_dedicated_check 1)
    target_include_directories(bf2_punkbuster_game_dedicated_check PRIVATE ${BF2_PYTHON_INCLUDE_DIRS})
    if(BUILD_TESTING)
        add_test(NAME bf2_punkbuster_game_dedicated_check COMMAND bf2_punkbuster_game_dedicated_check)
    endif()
endif()

# [TEMP] The PunkBuster server glue (Code/Tests/punkbuster_server_runtime.cpp) against a fake pbsv,
# fake PB_* functions and a scratch pb folder, in both images (the capture buffer's home differs).
# Built on request.
if(APPLE OR UNIX)
    foreach(pbsv_image IN ITEMS dedicated client)
        if(pbsv_image STREQUAL "dedicated")
            set(pbsv_target bf2_punkbuster_server_check)
            set(pbsv_define 1)
        else()
            set(pbsv_target bf2_punkbuster_server_client_check)
            set(pbsv_define 0)
        endif()
        add_executable(${pbsv_target} EXCLUDE_FROM_ALL
            Code/Tests/punkbuster_server_runtime.cpp
            "${BF2_ROOT}/Game/PunkBuster/PbServer.cpp")
        bf2_configure_cpp_target(${pbsv_target})
        bf2_set_platform_compat(${pbsv_target})
        bf2_set_image(${pbsv_target} ${pbsv_define})
        if(BUILD_TESTING)
            add_test(NAME ${pbsv_target} COMMAND ${pbsv_target})
        endif()
    endforeach()
endif()

# [TEMP] Camera::handleUpdate and its shake/recoil helpers (Code/Tests/camera_shake_runtime.cpp): the real
# Camera.cpp against generated fakes; every other engine symbol it references becomes an aborting stub
# generated from the objects by tools/macos/link_stubs.py. Built on request.
if(APPLE)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_library(bf2_camera_shake_objects OBJECT EXCLUDE_FROM_ALL
        Code/Tests/camera_shake_runtime.cpp
        "${BF2_ROOT}/Game/Objects/Base/Camera.cpp"
        "${BF2_ROOT}/Game/Objects/IPlayer.cpp"
        "${BF2_ROOT}/Scene/Object/ObjectPredicator.cpp")
    bf2_configure_cpp_target(bf2_camera_shake_objects)
    bf2_set_platform_compat(bf2_camera_shake_objects)
    bf2_set_image(bf2_camera_shake_objects 0)
    target_include_directories(bf2_camera_shake_objects PRIVATE ${BF2_PYTHON_INCLUDE_DIRS})
    set(bf2_camera_shake_stubs "${CMAKE_CURRENT_BINARY_DIR}/camera_shake_link_stubs.cpp")
    add_custom_command(OUTPUT "${bf2_camera_shake_stubs}"
        COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/tools/macos/link_stubs.py"
                "${bf2_camera_shake_stubs}" $<TARGET_OBJECTS:bf2_camera_shake_objects>
        DEPENDS bf2_camera_shake_objects $<TARGET_OBJECTS:bf2_camera_shake_objects>
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/macos/link_stubs.py"
        COMMAND_EXPAND_LISTS
        COMMENT "Generating the camera shake check's link stubs")
    add_executable(bf2_camera_shake_check EXCLUDE_FROM_ALL
        $<TARGET_OBJECTS:bf2_camera_shake_objects> "${bf2_camera_shake_stubs}")
    bf2_configure_cpp_target(bf2_camera_shake_check)
    bf2_set_platform_compat(bf2_camera_shake_check)
    bf2_set_image(bf2_camera_shake_check 0)
    if(BUILD_TESTING)
        add_test(NAME bf2_camera_shake_check COMMAND bf2_camera_shake_check)
    endif()
endif()

# [TEMP] The Soldier's control-box profile switch (Code/Tests/soldier_haptic_profile_runtime.cpp,
# ledger N2 17a) against a generated fake HapticSettings and fake player-control templates. Built on request.
if(APPLE OR UNIX)
    add_executable(bf2_soldier_haptic_profile_check EXCLUDE_FROM_ALL
        Code/Tests/soldier_haptic_profile_runtime.cpp
        "${BF2_ROOT}/Game/Objects/Game/SoldierHapticProfile.cpp"
        "${BF2_ROOT}/Math/Vec3.cpp")
    bf2_configure_cpp_target(bf2_soldier_haptic_profile_check)
    bf2_set_platform_compat(bf2_soldier_haptic_profile_check)
    bf2_set_image(bf2_soldier_haptic_profile_check 0)
    if(BUILD_TESTING)
        add_test(NAME bf2_soldier_haptic_profile_check COMMAND bf2_soldier_haptic_profile_check)
    endif()
endif()

# [TEMP] dice::hfe::Mutex recursion and exclusion (Code/Tests/mutex_runtime.cpp). Built on request.
if(APPLE OR UNIX)
    add_executable(bf2_mutex_check EXCLUDE_FROM_ALL
        Code/Tests/mutex_runtime.cpp
        "${BF2_ROOT}/Core/Mutex.cpp")
    bf2_configure_cpp_target(bf2_mutex_check)
    bf2_set_platform_compat(bf2_mutex_check)
    find_package(Threads REQUIRED)
    target_link_libraries(bf2_mutex_check PRIVATE Threads::Threads)
    if(BUILD_TESTING)
        add_test(NAME bf2_mutex_check COMMAND bf2_mutex_check)
    endif()
endif()

# [TEMP] CBF2VoipServer::Shutdown under one dice::hfe::Mutex (Code/Tests/voip_server_shutdown_runtime.cpp):
# the relock through ReleaseAllInstances must not hang. Built on request; UDP port 55197 only.
if(APPLE)
    add_executable(bf2_voip_server_shutdown_check EXCLUDE_FROM_ALL
        Code/Tests/voip_server_shutdown_runtime.cpp
        "${BF2_ROOT}/IO/Network/SocketManager.cpp"
        "${BF2_ROOT}/IO/Network/NetworkLayer/Socket/UDPSocket.cpp"
        "${BF2_ROOT}/VoipServer/VoipServer.cpp"
        "${BF2_ROOT}/VoipServer/VoipServerInstance.cpp"
        "${BF2_ROOT}/VoipServer/VoipDebug.cpp"
        "${BF2_ROOT}/VoipServer/VoipRouter.cpp"
        "${BF2_ROOT}/VoipServer/VoipClients.cpp"
        "${BF2_ROOT}/VoipServer/VoipOutgoingChannels.cpp"
        "${BF2_ROOT}/Core/Mutex.cpp"
        "${BF2_ROOT}/Core/Timer.cpp"
        "${BF2_ROOT}/Core/SystemInfo.cpp"
        "${BF2_ROOT}/Core/StringUtil.cpp")
    bf2_configure_cpp_target(bf2_voip_server_shutdown_check)
    bf2_set_platform_compat(bf2_voip_server_shutdown_check)
    bf2_set_image(bf2_voip_server_shutdown_check 1)
    target_compile_definitions(bf2_voip_server_shutdown_check PRIVATE BF2_PLATFORM_POSIX _MACOSX)
    # The challenge hash's MD5 (GameSpy md5c.c) is C; it builds without the C++ compatibility header.
    add_library(bf2_voip_server_shutdown_md5 OBJECT EXCLUDE_FROM_ALL "${BF2_ROOT}/External/GameSpy/md5c.c")
    target_sources(bf2_voip_server_shutdown_check PRIVATE $<TARGET_OBJECTS:bf2_voip_server_shutdown_md5>)
    if(BUILD_TESTING)
        add_test(NAME bf2_voip_server_shutdown_check COMMAND bf2_voip_server_shutdown_check)
    endif()
endif()

# [TEMP] Voice-server debug logs (Code/Tests/voip_debug_runtime.cpp, Windows twin
# voip_debug_runtime.cmd): HandlePacketDebugRoutingData, the routing dump and the voice trace, read
# back from the files they write. Built on request; the dedicated image (DEDICATED=1) as in the twin.
if(APPLE)
    add_executable(bf2_voip_debug_check EXCLUDE_FROM_ALL
        Code/Tests/voip_debug_runtime.cpp
        "${BF2_ROOT}/VoipServer/VoipServerInstance.cpp"
        "${BF2_ROOT}/VoipServer/VoipDebug.cpp"
        "${BF2_ROOT}/VoipServer/VoipRouter.cpp"
        "${BF2_ROOT}/VoipServer/VoipClients.cpp"
        "${BF2_ROOT}/VoipServer/VoipOutgoingChannels.cpp"
        "${BF2_ROOT}/Core/Timer.cpp"
        "${BF2_ROOT}/Core/SystemInfo.cpp"
        "${BF2_ROOT}/Core/StringUtil.cpp")
    bf2_configure_cpp_target(bf2_voip_debug_check)
    bf2_set_platform_compat(bf2_voip_debug_check)
    bf2_set_image(bf2_voip_debug_check 1)
    target_compile_definitions(bf2_voip_debug_check PRIVATE BF2_PLATFORM_POSIX _MACOSX)
    target_link_options(bf2_voip_debug_check PRIVATE "-Wl,-undefined,dynamic_lookup")
    if(BUILD_TESTING)
        add_test(NAME bf2_voip_debug_check COMMAND bf2_voip_debug_check)
    endif()
endif()

# [TEMP] The online services' domain (Code/Tests/online_provider_runtime.cpp): the host names formed
# for the retail domain and for a +gamespy value, looked up by the linked GameSpy SDK and by
# the persistence classes. The fixture replaces the resolver, so no name leaves the process, and
# listens on the services' own loopback ports. Built on request; the client image (DEDICATED=0),
# which links every service.
if(APPLE)
    add_executable(bf2_online_provider_check EXCLUDE_FROM_ALL
        Code/Tests/online_provider_runtime.cpp
        "${BF2_ROOT}/Game/MatchMaking/OnlineProvider.cpp"
        "${BF2_ROOT}/Game/MatchMaking/OnlineProviderGameSpy.cpp"
        "${BF2_ROOT}/Game/Persistence/HTTPRequest.cpp"
        "${BF2_ROOT}/Game/Persistence/StatsManager.cpp"
        "${BF2_ROOT}/Game/Persistence/PlayerStatsManager.cpp"
        "${BF2_ROOT}/Core/StringUtil.cpp"
        $<TARGET_OBJECTS:bf2_gamespy_objects>
        $<TARGET_OBJECTS:bf2_gamespy_sb_objects>
        $<TARGET_OBJECTS:bf2_gamespy_gp_objects>)
    bf2_configure_cpp_target(bf2_online_provider_check)
    bf2_set_platform_compat(bf2_online_provider_check)
    bf2_set_image(bf2_online_provider_check 0)
    target_compile_definitions(bf2_online_provider_check PRIVATE BF2_PLATFORM_POSIX _MACOSX)
    find_package(Threads REQUIRED)
    target_link_libraries(bf2_online_provider_check PRIVATE Threads::Threads)
    if(BUILD_TESTING)
        add_test(NAME bf2_online_provider_check COMMAND bf2_online_provider_check)
    endif()
endif()

# [TEMP] The stats web requests and their parsers (Code/Tests/persistence_stats_runtime.cpp): every
# endpoint the game requests, formed for +gamespy test.openbf2, against a loopback listener with
# handwritten bodies. The fixture replaces the resolver, so no name leaves the process. Built on
# request; the client image (DEDICATED=0), which holds the profile manager's createplayer post.
if(APPLE)
    add_executable(bf2_persistence_stats_check EXCLUDE_FROM_ALL
        Code/Tests/persistence_stats_runtime.cpp
        "${BF2_ROOT}/Game/MatchMaking/OnlineProvider.cpp"
        "${BF2_ROOT}/Game/MatchMaking/OnlineProviderGameSpy.cpp"
        "${BF2_ROOT}/Game/Persistence/HTTPRequest.cpp"
        "${BF2_ROOT}/Game/Persistence/KeyParser.cpp"
        "${BF2_ROOT}/Game/Persistence/PlayerStats.cpp"
        "${BF2_ROOT}/Game/Persistence/PlayerAwards.cpp"
        "${BF2_ROOT}/Game/Persistence/LeaderBoard.cpp"
        "${BF2_ROOT}/Game/Persistence/PlayerUnlocks.cpp"
        "${BF2_ROOT}/Game/Persistence/PlayerUnlockInfo.cpp"
        "${BF2_ROOT}/Game/Persistence/PlayerRankInfo.cpp"
        "${BF2_ROOT}/Game/Persistence/SearchForPlayer.cpp"
        "${BF2_ROOT}/Game/Persistence/GSServerSideNameValidation.cpp"
        "${BF2_ROOT}/Game/Persistence/StatsManager.cpp"
        "${BF2_ROOT}/Game/Persistence/PlayerStatsManager.cpp"
        "${BF2_ROOT}/Game/ProfileManager.cpp"
        "${BF2_ROOT}/Game/Persistence/AntiHacks.cpp"
        "${BF2_ROOT}/Core/System.cpp"
        "${BF2_ROOT}/Core/SystemInfo.cpp"
        "${BF2_ROOT}/Core/StringUtil.cpp"
        $<TARGET_OBJECTS:bf2_gamespy_objects>
        $<TARGET_OBJECTS:bf2_gamespy_sb_objects>
        $<TARGET_OBJECTS:bf2_gamespy_gp_objects>)
    bf2_configure_cpp_target(bf2_persistence_stats_check)
    bf2_set_platform_compat(bf2_persistence_stats_check)
    bf2_set_image(bf2_persistence_stats_check 0)
    target_compile_definitions(bf2_persistence_stats_check PRIVATE BF2_PLATFORM_POSIX _MACOSX)
    target_include_directories(bf2_persistence_stats_check PRIVATE ${BF2_PYTHON_INCLUDE_DIRS})
    find_package(Threads REQUIRED)
    target_link_libraries(bf2_persistence_stats_check PRIVATE Threads::Threads)
    if(BUILD_TESTING)
        add_test(NAME bf2_persistence_stats_check COMMAND bf2_persistence_stats_check)
    endif()
endif()

# [TEMP] SwiffHost's leaderboard and awards caches (Code/Tests/swiff_leaderboard_runtime.cpp) through
# the real persistence classes, against the local stats fixture (Code/Tests/stats_fixture) on
# 127.0.0.1:18124, with the host formed for +gamespy test.openbf2. The fixture replaces the
# resolver, so no name leaves the process. Built on request; needs Python 3 for the fixture.
if(APPLE)
    add_executable(bf2_swiff_leaderboard_check EXCLUDE_FROM_ALL
        Code/Tests/swiff_leaderboard_runtime.cpp
        "${BF2_ROOT}/Menu/SwiffHost.cpp"
        "${BF2_ROOT}/Game/MatchMaking/OnlineProvider.cpp"
        "${BF2_ROOT}/Game/Persistence/HTTPRequest.cpp"
        "${BF2_ROOT}/Game/Persistence/KeyParser.cpp"
        "${BF2_ROOT}/Game/Persistence/LeaderBoard.cpp"
        "${BF2_ROOT}/Game/Persistence/PlayerAwards.cpp"
        "${BF2_ROOT}/Game/Persistence/PlayerUnlocks.cpp"
        "${BF2_ROOT}/Game/Persistence/PlayerUnlockInfo.cpp"
        "${BF2_ROOT}/Game/Persistence/PlayerRankInfo.cpp"
        "${BF2_ROOT}/Game/Persistence/SearchForPlayer.cpp"
        "${BF2_ROOT}/Core/StringUtil.cpp"
        $<TARGET_OBJECTS:bf2_gamespy_objects>)
    bf2_configure_cpp_target(bf2_swiff_leaderboard_check)
    bf2_set_platform_compat(bf2_swiff_leaderboard_check)
    bf2_set_image(bf2_swiff_leaderboard_check 0)
    target_compile_definitions(bf2_swiff_leaderboard_check PRIVATE BF2_PLATFORM_POSIX _MACOSX)
    target_include_directories(bf2_swiff_leaderboard_check PRIVATE ${BF2_PYTHON_INCLUDE_DIRS})
    if(BUILD_TESTING)
        find_package(Python3 COMPONENTS Interpreter)
        if(Python3_Interpreter_FOUND)
            add_test(NAME bf2_swiff_leaderboard_check
                     COMMAND bf2_swiff_leaderboard_check "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/Code/Tests/stats_fixture")
        endif()
    endif()
endif()

# [TEMP] Test-only host-name redirect (Code/Tests/name_redirect): a DYLD_INSERT_LIBRARIES library that
# resolves the names of one test domain to 127.0.0.1 for locally built images. Built on request, into
# its own directory outside bin_macos; no game image links it and no package contains it.
if(APPLE)
    add_library(bf2_test_name_redirect SHARED EXCLUDE_FROM_ALL Code/Tests/name_redirect/name_redirect.c)
    set_target_properties(bf2_test_name_redirect PROPERTIES
        OUTPUT_NAME bf2_test_name_redirect
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test_tools"
        C_VISIBILITY_PRESET hidden)
endif()

# bf2_memory - the rebuilt Memory.dll (Code/BF2/Memory). Built on request only (EXCLUDE_FROM_ALL)
# and linked into no game image (issue #7). Its tests join CTest when BF2_MEMORY_TESTS is ON.
add_subdirectory(Code/BF2/Memory)

# [CHOSEN, not recovered] CPU-only effect pipeline for the macOS renderer facade.
if(BF2_BUILD_MACOS)
    add_subdirectory(Code/BF2/Platform/Gfx/Effect)
    add_subdirectory(Code/Tests/macos_gfx_forward)
    # [CHOSEN, not recovered] Native effect compilation is mandatory; the effect subdirectory
    # rejects missing dependencies at configure time. No developer oracle is invoked here.
    if(TARGET bf2_renddx9_static)
        target_compile_definitions(bf2_renddx9_static PRIVATE BF2_FX_NATIVE=1)
    endif()
    if(BF2_MACOS_LINK_RENDDX9 AND TARGET bf2_client AND TARGET bf2_renddx9_static)
        target_link_libraries(bf2_client PRIVATE bf2_fx_native_compiler)
        target_include_directories(bf2_renddx9_static PRIVATE "${BF2_ROOT}/Platform/Gfx/Effect")
    endif()
endif()
# The focused checks mirror Code/Tests/*.cmd, which are MSVC x86 gates; they are not part of the
# Linux and macOS ports (their checks are the dice_py runtime check and the production run).
if(BUILD_TESTING AND MSVC)
    function(bf2_add_check target)
        add_executable("${target}" EXCLUDE_FROM_ALL ${ARGN})
        target_compile_features("${target}" PRIVATE cxx_std_14)
        # /O2 matches the Code/Tests/*.cmd script each of these targets mirrors, so CLion and the
        # command line validate the same binary. It only coexists with a Debug build because
        # /RTC1 is stripped at the top of this file - see the block there, including the one
        # check that behaves differently without /O2.
        target_compile_options("${target}" PRIVATE /EHsc /O2 /permissive)
        bf2_use_static_msvc_runtime("${target}")
        bf2_set_image("${target}" 1)
        set_property(TARGET "${target}" PROPERTY FOLDER "Checks")
        add_test(NAME "${target}" COMMAND $<TARGET_FILE:${target}>)
        set_tests_properties("${target}" PROPERTIES
            RUN_SERIAL TRUE
            WORKING_DIRECTORY "${BF2_STUB_ROOT}"
        )
    endfunction()

    bf2_add_check(bf2_bitstream_roundtrip
        "${BF2_STUB_ROOT}/bitstream_roundtrip.cpp"
    )

    bf2_add_check(bf2_networkable_abi
        "${BF2_STUB_ROOT}/networkable_abi.cpp"
        "${BF2_ROOT}/IO/Network/INetworkable.cpp"
        "${BF2_ROOT}/IO/Network/NetworkableBase.cpp"
    )

    bf2_add_check(bf2_bundle_template_core
        "${BF2_STUB_ROOT}/bundle_template_core.cpp"
        "${BF2_ROOT}/Game/Objects/Base/BundleTemplate.cpp"
        "${BF2_ROOT}/Math/Mat4.cpp"
    )
    target_compile_options(bf2_bundle_template_core PRIVATE /Gy)
    target_link_options(bf2_bundle_template_core PRIVATE /OPT:REF /FORCE:UNRESOLVED)

    bf2_add_check(bf2_bundle_serialization
        "${BF2_STUB_ROOT}/bundle_serialization.cpp"
        "${BF2_ROOT}/Game/Objects/IObjectTemplate.cpp"
        "${BF2_ROOT}/Game/Objects/ObjectTemplate.cpp"
        "${BF2_ROOT}/Game/Objects/Base/SimpleObjectTemplate.cpp"
        "${BF2_ROOT}/Game/Objects/Base/BundleTemplate.cpp"
        "${BF2_ROOT}/Physics/PhysicsType.cpp"
        "${BF2_ROOT}/Math/Mat4.cpp"
        "${BF2_ROOT}/Core/StringUtil.cpp"
        "${BF2_ROOT}/Core/Debug.cpp"
        "${BF2_ROOT}/Core/Globals.cpp"
    )
    target_link_options(bf2_bundle_serialization PRIVATE /FORCE:UNRESOLVED)

    # Keep this explicit source list aligned with Code/Tests/runtime_pair.cmd.
    bf2_add_check(bf2_runtime_pair
        "${BF2_STUB_ROOT}/runtime_pair.cpp"
        "${BF2_ROOT}/Game/Objects/IObject.cpp"
        "${BF2_ROOT}/Game/Objects/IObjectComponent.cpp"
        "${BF2_ROOT}/Game/Objects/IObjectTemplate.cpp"
        "${BF2_ROOT}/Game/Objects/ObjectTemplate.cpp"
        "${BF2_ROOT}/Game/Objects/Base/SimpleObjectTemplate.cpp"
        "${BF2_ROOT}/Game/Objects/Base/BundleTemplate.cpp"
        "${BF2_ROOT}/Game/Objects/ItemContainerTemplate.cpp"
        "${BF2_ROOT}/Game/Objects/Base/SimpleObject.cpp"
        "${BF2_ROOT}/Game/Objects/Base/Bundle.cpp"
        "${BF2_ROOT}/Game/Objects/Visual/LightSource.cpp"
        "${BF2_ROOT}/Game/Objects/Visual/LightSourceTemplate.cpp"
        "${BF2_ROOT}/Scene/Object/WorldObjTemplPortal/PortalManager.cpp"
        "${BF2_ROOT}/Scene/Object/WorldObjTemplPortal/PortalPassing.cpp"
        "${BF2_ROOT}/Scene/Object/WorldObjTemplPortal/Sector.cpp"
        "${BF2_ROOT}/Scene/Object/WorldObjTemplPortal/PortalEvent.cpp"
        "${BF2_ROOT}/Scene/Object/Object.cpp"
        "${BF2_ROOT}/Scene/Object/ObjectPredicator.cpp"
        "${BF2_ROOT}/Scene/Object/ObjectUtil.cpp"
        "${BF2_ROOT}/Scene/Object/ObjectTypes/Game/IPlayerControlObject.cpp"
        "${BF2_ROOT}/Game/Utility/Vars.cpp"
        "${BF2_ROOT}/Physics/IPhysicsNode.cpp"
        "${BF2_ROOT}/Physics/IResponsePhysics.cpp"
        "${BF2_ROOT}/Physics/GeomUtil.cpp"
        "${BF2_ROOT}/Physics/PhysicsNode.cpp"
        "${BF2_ROOT}/Physics/PhysicsType.cpp"
        "${BF2_ROOT}/Physics/PhysicsUtil.cpp"
        "${BF2_ROOT}/Physics/PointPhysicsNode.cpp"
        "${BF2_ROOT}/Physics/PointResponsePhysics.cpp"
        "${BF2_ROOT}/Physics/ResponsePhysics.cpp"
        "${BF2_ROOT}/Physics/ResponsePhysicsManager.cpp"
        "${BF2_ROOT}/Physics/RotationalPointPhysicsNode.cpp"
        "${BF2_ROOT}/Physics/RotationalPointResponsePhysics.cpp"
        "${BF2_ROOT}/Physics/SoldierPhysicsNode.cpp"
        "${BF2_ROOT}/Physics/SoldierResponse.cpp"
        "${BF2_ROOT}/Physics/StaticPhysicsNode.cpp"
        "${BF2_ROOT}/Physics/StaticResponsePhysics.cpp"
        "${BF2_ROOT}/IO/Network/INetworkable.cpp"
        "${BF2_ROOT}/Math/LocalBoundingBox.cpp"
        "${BF2_ROOT}/Math/Intersect.cpp"
        "${BF2_ROOT}/IO/Stream/BoundingBox.cpp"
        "${BF2_ROOT}/Math/Frustum.cpp"
        "${BF2_ROOT}/Math/Mat4.cpp"
        "${BF2_ROOT}/Math/Plane3.cpp"
        "${BF2_ROOT}/Math/Quat.cpp"
        "${BF2_ROOT}/Math/Vec3.cpp"
        "${BF2_ROOT}/Core/StringUtil.cpp"
        "${BF2_ROOT}/Core/Debug.cpp"
        "${BF2_ROOT}/Core/Globals.cpp"
    )
    target_compile_options(bf2_runtime_pair PRIVATE /Gy)
    target_link_options(bf2_runtime_pair PRIVATE /OPT:REF)

    # [CHOSEN, not recovered] The client's Registry.dat (docs/registry-file.md): fake CD keys
    # round-trip through CryptProtectData and the production reader, and through the dev script.
    # A client image (DEDICATED=0), so it is not a bf2_add_check.
    add_executable(bf2_registry_dat_check EXCLUDE_FROM_ALL
        "${BF2_STUB_ROOT}/registry_dat_check.cpp"
        "${BF2_ROOT}/Core/Registry.cpp"
    )
    target_compile_features(bf2_registry_dat_check PRIVATE cxx_std_14)
    target_compile_options(bf2_registry_dat_check PRIVATE /EHsc /O2 /permissive)
    bf2_use_static_msvc_runtime(bf2_registry_dat_check)
    bf2_set_image(bf2_registry_dat_check 0)
    set_property(TARGET bf2_registry_dat_check PROPERTY FOLDER "Checks")
    add_test(NAME bf2_registry_dat_check COMMAND $<TARGET_FILE:bf2_registry_dat_check>)
    add_test(NAME bf2_registry_dat_script
        COMMAND powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command
            "& '${CMAKE_CURRENT_SOURCE_DIR}/tools/windows/new-registry-dat.ps1' -CdKey FAKE-SCRIPT-0000-1111 -Path '${CMAKE_BINARY_DIR}/registry_dat_script/Registry.dat'; if (-not $?) { exit 1 }; & '$<TARGET_FILE:bf2_registry_dat_check>' '${CMAKE_BINARY_DIR}/registry_dat_script/Registry.dat' FAKE-SCRIPT-0000-1111; exit $LASTEXITCODE"
    )
    set_tests_properties(bf2_registry_dat_check bf2_registry_dat_script PROPERTIES RUN_SERIAL TRUE)

    add_custom_target(bf2_run_checks
        COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure -C $<CONFIG>
        DEPENDS
            bf2_bitstream_roundtrip
            bf2_networkable_abi
            bf2_bundle_template_core
            bf2_bundle_serialization
            bf2_runtime_pair
            bf2_registry_dat_check
            bf2_hud_widescreen_check
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        USES_TERMINAL
        VERBATIM
    )
    set_property(TARGET bf2_run_checks PROPERTY FOLDER "Checks")
endif()
