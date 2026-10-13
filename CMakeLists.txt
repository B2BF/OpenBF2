cmake_minimum_required(VERSION 3.21)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()

project(BF2Rebuilt LANGUAGES C CXX)

# =================================================================================================
# TOOLCHAINS (2026-09-29). Three configurations, one source tree:
#   * MSVC x86            - the retail-fidelity reconstruction: bf2_w32ded.exe, /FORCE diagnostic link.
#   * MSVC x64            - the native Windows dedicated port: bf2_w64ded.exe, strict link.
#   * GCC/Clang, Linux    - the native Linux dedicated port: `bf2` + `libdice_py.so`. It is the x64
#     x86-64                port's pointer-bearing layouts compiled by the platform family the ELF
#                           oracle was built for.
# Everything Linux-specific in this file sits under BF2_BUILD_LINUX; the MSVC configurations are
# untouched by it. The Linux build is a native port (`[CHOSEN, not recovered]` layouts, like x64),
# not a reproduction of the shipped 32-bit ELF's object layouts.
# =================================================================================================
set(BF2_BUILD_LINUX OFF)
if(MSVC)
    # The two Windows configurations, selected by pointer width below.
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(BF2_BUILD_LINUX ON)
else()
    message(FATAL_ERROR
        "BF2Rebuilt builds with MSVC on Windows (msvc-x86 / msvc-x64 presets) or with GCC/Clang on "
        "Linux x86-64 (linux-x64 preset). This toolchain is ${CMAKE_CXX_COMPILER_ID} on "
        "${CMAKE_SYSTEM_NAME}.")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND NOT BF2_BUILD_LINUX)
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
    else()
        set(BF2_NATIVE_ARCH "x64")
        message(STATUS
            "Configuring the native x64 dedicated-server port with strict linking (no /FORCE). "
            "See docs/building.md for toolchain and runtime setup.")
    endif()
else()
    message(FATAL_ERROR
        "BF2Rebuilt supports only MSVC x86 (retail-fidelity), MSVC x64 (native dedicated port) "
        "and Linux x86-64 (native dedicated port) configurations; "
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
    if(IS_DIRECTORY "${component_path}" AND NOT component_name STREQUAL "External")
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
list(SORT bf2_engine_headers)

# These translation units belong to AIDLL_w32ded.dll, not bf2_w32ded.exe.  The source tree is
# intentionally broader than one image, so a recursive glob is only the discovery input; this
# manifest is the image boundary.  CommonSingletonServer.cpp STAYS in the engine link: its class is
# present in both shipped images (bf2 0x841B3C0, libaidll 0x2EB410) and engine code consumes it.
# Its AIDLL-only half WAS SPLIT OUT on 2026-09-08 into AI/AIDLL/AI/Main/AISingletonServer.cpp, which
# this list excludes - `AISingletonServer` exists only in libaidll.so.  The side-DLL target that the
# old note was waiting for is Code/Build/aidll.cmd plus bf2_aidll below.
set(bf2_aidll_only_sources
    "${BF2_ROOT}/Core/DllModule.cpp"
    "${BF2_ROOT}/Core/DllGlobals.cpp"                        # the side-DLL image's globals TU
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/BotMain.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/BotNavigation.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/BotSquad.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/SensingAgent.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/CommunicationAgent.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/BotSenses.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIPlanInterpreter/BAPContext.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/InformationManager.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/Information.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/WeaponInfo.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/IPIArmament.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/IPIArmamentReal.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/IPICover.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/IPIMobile.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/IPIMobileReal.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/IPIMobileKnown.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/IPIUnit.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/InformationReal.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/WeaponInfoReal.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/IPIPhysicalReal.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIUtils/VehicleOccupancy.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/BFInformationEnvironment.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/AIInformationGrid.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIUtils/DebugRendering/AIRenderUtil.cpp"  # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/WeaponInfoManager.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/BFInformationCourier.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/InformationFactory.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIStrategicMap/BFStrategicEnvironment.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIStrategicMap/BotSpawner.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/DrawPrimitives/DPManager.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AICollisionHandler.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AIEventCourier.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AIMineManager.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AIPlayerCourier.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/Main/BFPathfindingCourier.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIPathFinding/Material/Material.cpp"          # libaidll materialId parser
    "${BF2_ROOT}/AI/AIDLL/AI/AIPathFinding/Material/MaterialManager.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIPathFinding/NavMesh/MapProperties.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIPathFinding/NavMesh/QuadTreeManager.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIPathFinding/NavMesh/QuadTree.cpp"   # libaidll only - chunk 04
    "${BF2_ROOT}/AI/AIDLL/AI/AIPathFinding/NavMesh/QuadTreeNode.cpp"   # libaidll only - live polygon query
    "${BF2_ROOT}/AI/AIDLL/AI/AIPathFinding/Cluster/ClusterGraph.cpp"   # libaidll only - chunk 04
    "${BF2_ROOT}/AI/AIDLL/AI/AIPathFinding/Cluster/ClusterNodeManager.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIPathFinding/NavMesh/QTSearchNodeManager.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIStrategicMap/AerialHeightMap.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIPlanInterpreter/BAPEntries.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/SensingAgentPatterns.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/SquadManager.cpp"           # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/SquadCoordinator.cpp"      # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/SquadFormation.cpp"        # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/SquadMember.cpp"           # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIBotBehaviour/CustomCurve.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/BotMemory.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/BotSubObjects.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIBotBehaviour/UrgencyMerger.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AIConsole.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIBotBehaviour/UrgeCurve.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIStrategicEntity/CoverPosition/CPNodeManager.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIStrategicEntity/CoverPosition/CoverPositionManager.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/AIStrategicEntity/AIStrategicObjectManager.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIStrategicEntity/AILandingZoneManager.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIStrategicEntity/AIStrategicObject.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIStrategicEntity/AIStrategicArea.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIStrategicEntity/SAWayPoint.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIStrategic/SAI.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/Main/BotBehaviourManager.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/Main/CommunicationHub.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/Main/StrategyManager.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AIPathfinding.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/Main/IAIPathfinding.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/Main/UserOrderManager.cpp"   # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AIMain.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AIModule.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AIRandom.cpp"              # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AILODManager.cpp"          # libaidll only - chunk 02
    "${BF2_ROOT}/AI/AIDLL/AI/Main/IBotManager.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AISettings.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AISingletonServer.cpp"     # libaidll only - see the split, below
    "${BF2_ROOT}/AI/AIDLL/AI/Main/BF2AI.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/BotManager.cpp"
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
    "${BF2_ROOT}/Game/Main/BF2ClientFrontiers.cpp"
    "${BF2_ROOT}/Game/LocalProfileManager.cpp"
    "${BF2_ROOT}/Game/LocalProfileManagerInit.cpp"
    "${BF2_ROOT}/Game/LocalProfileManagerOperations.cpp"
    "${BF2_ROOT}/Game/LocalProfileManagerSelection.cpp"
    "${BF2_ROOT}/Game/LocalProfileManagerDiscovery.cpp"
    "${BF2_ROOT}/Game/LocalProfileManagerFields.cpp"
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
    "${BF2_ROOT}/IO/Input/FalconDevice.cpp"
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
# the manifest on Linux instead. Same discipline as the AIDLL and client lists above.
set(bf2_win32_only_sources
    "${BF2_ROOT}/IO/File/Win32FileSystem.cpp"
    "${BF2_ROOT}/Game/Main/StatusMonitorImpl_win32.cpp"
)
if(BF2_BUILD_LINUX)
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
        target_compile_options("${target}" PRIVATE /W3 /wd4996)
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
# External/python-embedded/linux/ (a chosen path - see that directory's README).
if(BF2_BUILD_LINUX)
    set(BF2_PYTHON_INCLUDE_DIRS
        "${BF2_EXTERNAL_ROOT}/python-embedded/linux"
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
if(BF2_BUILD_LINUX)
    target_include_directories(bf2_engine_objects PRIVATE ${CURSES_INCLUDE_DIRS})
    # The engine's own GameSpy-facing TUs (persistence, match-making, HTTP) include the SDK headers,
    # whose platform switch is _LINUX (gsPlatform.h) - the same define bf2_configure_c_target gives
    # the SDK's C objects.
    target_compile_definitions(bf2_engine_objects PRIVATE _LINUX)
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
    BF2_PLUGIN_ABI_NATIVE=1
    _ITERATOR_DEBUG_LEVEL=0
)
target_include_directories(bf2_client_objects PRIVATE
    ${BF2_PYTHON_INCLUDE_DIRS}
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3"
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3/contrib/minizip"
)
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
# ⚠ The x86 diagnostic target retains /FORCE: every unwritten function links at RVA 0 and faults
# when reached. The x64 target DOES NOT use /FORCE. It must not emit a runnable image until every
# compile, dependency, and strict-link requirement is satisfied.
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
if(BF2_BUILD_LINUX)
    # libdice_py.so - the embedded CPython 2.3.4 as a shared library, built beside `bf2`.
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
if(BF2_BUILD_LINUX)
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
if(BF2_BUILD_LINUX)
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
if(NOT BF2_BUILD_X64)
    target_link_options("${BF2_DEDICATED_TARGET}" PRIVATE /FORCE)
else()
    target_link_options("${BF2_DEDICATED_TARGET}" PRIVATE /OPT:REF /OPT:ICF /INCREMENTAL:NO)
endif()
endif() # BF2_BUILD_LINUX / MSVC link contracts

# =================================================================================================
# bf2_aidll - THE BOT-AI SIDE-DLL IMAGE.
# The native-x64 profile is intentionally dedicated-only. Do not let it emit x64 binaries under
# the shipped x86 AIDLL/BF2 names; those ports require separate targets and ABI validation.
if(NOT BF2_BUILD_X64)
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
# EXCLUDE_FROM_ALL, and /FORCE for the same reason bf2_w32ded is: every unwritten function links at
# RVA 0 and faults BY NAME on the path that reaches it. The only unresolved symbol today is
# ai::informationManager, the InformationManager frontier BF2AI.cpp declares and chunk 02 fills.
#
# OUTPUT: <BF2_AIDLL_OUTPUT_DIR>/AIDLL_w32ded.dll - the build tree's bin/, NEVER the game folder.
# "D:/B2BF Launcher/Battlefield2/AIDLL_w32ded.dll" is the REFERENCE binary. The server reaches the
# rebuilt DLL through a RELATIVE `AIDLL` setting (LoadLibraryBf builds ".\\" + name against the
# current directory); Code/Tests/aidll/run.py does exactly that and copies nothing.
# =================================================================================================
set(BF2_AIDLL_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin" CACHE PATH
    "Where AIDLL_w32ded.dll is written (NOT the game folder - the original lives there)")

add_library(bf2_aidll SHARED EXCLUDE_FROM_ALL
    # group 1 - the module entry point; engine code compiled into every side-DLL, never into the exe
    "${BF2_ROOT}/Core/DllModule.cpp"
    "${BF2_ROOT}/Core/DllGlobals.cpp"
    # group 2 - the AIDLL-only AI translation units
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/BotMain.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/BotNavigation.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/BotSquad.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/SensingAgent.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/CommunicationAgent.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/BotSenses.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIPlanInterpreter/BAPContext.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/InformationManager.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/Information.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/WeaponInfo.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/IPIArmament.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/IPIArmamentReal.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/IPICover.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/IPIMobile.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/IPIMobileReal.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/IPIMobileKnown.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/IPIUnit.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/InformationReal.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/WeaponInfoReal.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/IPIPhysicalReal.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIUtils/VehicleOccupancy.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/BFInformationEnvironment.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/AIInformationGrid.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIUtils/DebugRendering/AIRenderUtil.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/WeaponInfoManager.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/BFInformationCourier.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIKnowledge/InformationFactory.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIStrategicMap/BFStrategicEnvironment.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIStrategicMap/BotSpawner.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/DrawPrimitives/DPManager.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AICollisionHandler.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AIEventCourier.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AIMineManager.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AIPlayerCourier.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/BFPathfindingCourier.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIPathFinding/Material/Material.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIPathFinding/Material/MaterialManager.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIPathFinding/NavMesh/MapProperties.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIPathFinding/NavMesh/QuadTreeManager.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIPathFinding/NavMesh/QuadTree.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIPathFinding/NavMesh/QuadTreeNode.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIPathFinding/Cluster/ClusterGraph.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIPathFinding/Cluster/ClusterNodeManager.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIPathFinding/NavMesh/QTSearchNodeManager.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIStrategicMap/AerialHeightMap.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIPlanInterpreter/BAPEntries.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/SensingAgentPatterns.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/SquadManager.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/SquadCoordinator.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/SquadFormation.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/SquadMember.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIBotBehaviour/CustomCurve.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/BotMemory.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIBot/BotSubObjects.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIBotBehaviour/UrgencyMerger.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AIConsole.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIBotBehaviour/UrgeCurve.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIStrategicEntity/CoverPosition/CPNodeManager.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIStrategicEntity/CoverPosition/CoverPositionManager.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIStrategicEntity/AIStrategicObjectManager.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIStrategicEntity/AILandingZoneManager.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIStrategicEntity/AIStrategicObject.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIStrategicEntity/AIStrategicArea.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIStrategicEntity/SAWayPoint.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/AIStrategic/SAI.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/BotBehaviourManager.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/CommunicationHub.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/StrategyManager.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AIPathfinding.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/IAIPathfinding.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/UserOrderManager.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AIMain.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AIModule.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AIRandom.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AILODManager.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/IBotManager.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AISettings.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/AISingletonServer.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/BF2AI.cpp"
    "${BF2_ROOT}/AI/AIDLL/AI/Main/BotManager.cpp"
    # group 3 - present in BOTH shipped images
    "${BF2_ROOT}/AI/AIDLL/AI/Main/CommonSingletonServer.cpp"
    # group 4 - engine translation units libaidll.so statically links, added as the strict link
    # demands them (census/00_statically_linked_engine_code.tsv confirms each one is really there)
    "${BF2_ROOT}/Core/ConsoleObjects.cpp"     # 0x4865C0.. - registerConsoleObjects, AIModule::init
    "${BF2_ROOT}/Core/ConsoleObjectBase.cpp"  # the ConsoleClassN base - the ai.init binding
    "${BF2_ROOT}/Math/Frustum.cpp"           # a bot sees by frustum - BotSenses
    "${BF2_ROOT}/Math/Mat4Inverse.cpp"        # AIDLL PE body matches the recovered client PE body
    "${BF2_ROOT}/AI/AIInterface/BFKnowledge/PlanePolygon.cpp" # physical-cover fallback dependency
    "${BF2_ROOT}/Math/Plane3.cpp"            # Frustum pulls Plane3::transform + intersect
    "${BF2_ROOT}/Math/Vec3.cpp"              # Vec3::getZero - BotMain's constructor
    "${BF2_ROOT}/Core/Debug.cpp"              # 0x487C30.. - Debug ctor/dtor, reached by checkPointer
    "${BF2_ROOT}/Core/StringUtil.cpp"         # 0x487AF0   - calcStringHashValueNoCase, for StringHashFunc
    # Kept in step with Code/Build/aidll.cmd (2026-09-28): File.cpp joined that manifest on
    # 2026-09-11 without this list; PlayerInput.cpp joins both for Input<PlayerInputMap,64>::setInput.
    "${BF2_ROOT}/IO/File/File.cpp"            # io::File - AerialHeightMap's file paths
    "${BF2_ROOT}/IO/Input/PlayerInput.cpp"    # 0x18F020   - setInput(float), the plan executor
)
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

# =================================================================================================
# bf2_client - THE CLIENT LINK-REACH IMAGE (2026-09-03).
#
# This is a real /DDEDICATED=0 image. Modern link.exe does
# not enumerate unresolved symbols when /FORCE is active, so bf2_client_linkcheck below is the
# deliberately failing strict link whose diagnostics are the client work list. Both targets are
# EXCLUDE_FROM_ALL. Process startup constructs HUD, then stops before singleton registration (2026-09-05);
# this is not a runnable-client claim. RendDX9 now has a separately proven
# native registry/module ABI flavour, but its HWND/display-mode boundary and the exe-side
# BF2Render bridge are still open; SwiffPlayer's native AS host callbacks remain frontiers. Both plugin
# builds are staged beside this diagnostic host since 2026-09-08 (the bf2_client_plugins dependency below).
# Output stays in this build tree and can never overwrite the retail BF2.exe used as
# reverse-engineering evidence.
# =================================================================================================
# Fixed inside this build tree on purpose: unlike the server's opt-in deployment directory, a
# diagnostic client target must never be redirected onto the retail BF2.exe (CMake clean would then
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
target_compile_definitions(bf2_client PRIVATE BF2_PLUGIN_ABI_NATIVE=1 _ITERATOR_DEBUG_LEVEL=0)
set_target_properties(bf2_client PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${BF2_GAME_DIR}"
    VS_DEBUGGER_COMMAND_ARGUMENTS "+modPath mods/bf2 +fullscreen 0 +multi 1"
)

# Normal startup includes the movies; +restart remains an optional explicit skip.
set(BF2_CLIENT_RUN_ARGS "+modPath;mods/bf2;+fullscreen;0;+multi;1"
    CACHE STRING "Arguments for the diagnostic bf2_client_run target (a CMake ;-list).")
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
        COMMENT "Staging dice_py.dll beside the diagnostic BF2.exe"
        VERBATIM
    )
endif()

# The bounded Novint SDK surface contains no cross-compiler STL objects. Import
# names and the 32-byte ApplicationData carrier are read from the original client;
# the matching DLL supplies third-party bodies.
set(BF2_NOVINT_IMPORT_LIB "${CMAKE_BINARY_DIR}/novint_hfx/NovintHFX.lib")
add_custom_command(OUTPUT "${BF2_NOVINT_IMPORT_LIB}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/novint_hfx"
    COMMAND "${CMAKE_AR}" /nologo /machine:x86
            "/def:${BF2_EXTERNAL_ROOT}/NovintHFX/NovintHFX.def"
            "/out:${BF2_NOVINT_IMPORT_LIB}"
    DEPENDS "${BF2_EXTERNAL_ROOT}/NovintHFX/NovintHFX.def"
    VERBATIM
)
add_custom_target(bf2_novint_imports DEPENDS "${BF2_NOVINT_IMPORT_LIB}")
add_dependencies(bf2_client bf2_novint_imports)
target_link_libraries(bf2_client PRIVATE "${BF2_NOVINT_IMPORT_LIB}")
if(EXISTS "${BF2_GAME_DIR}/NovintHFX.dll")
    add_custom_command(TARGET bf2_client POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${BF2_GAME_DIR}/NovintHFX.dll" "${BF2_CLIENT_OUTPUT_DIR}/NovintHFX.dll"
        COMMENT "Staging the original Novint SDK companion beside the diagnostic BF2.exe"
        VERBATIM
    )
endif()

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
        COMMAND cmd.exe /d /c "${BF2_TEXTUREATLAS_SCRIPT}" native
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Code/Build"
        COMMENT "Building the native TextureAtlasBuilder.dll (Code/Build/textureatlasbuilder.cmd native)"
        VERBATIM
    )
    set_property(TARGET bf2_textureatlas_native PROPERTY FOLDER "BF2")
    add_custom_target(bf2_renddx9_native
        COMMAND cmd.exe /d /c "${BF2_RENDDX9_SCRIPT}" native
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Code/Build"
        COMMENT "Building the native RendDX9.dll (Code/Build/renddx9.cmd native)"
        VERBATIM
    )
    set_property(TARGET bf2_renddx9_native PROPERTY FOLDER "BF2")
    add_dependencies(bf2_renddx9_native bf2_textureatlas_native)
    add_custom_target(bf2_swiffplayer_native
        COMMAND cmd.exe /d /c "${BF2_SWIFFPLAYER_SCRIPT}" native
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Code/Build"
        COMMENT "Building the native SwiffPlayer.dll (Code/Build/swiffplayer.cmd native)"
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
        COMMAND cmd.exe /d /c "${BF2_BF2AUDIO_SCRIPT}" native
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Code/Build"
        COMMENT "Building the native BF2Audio.dll (Code/Build/bf2audio.cmd native)"
        VERBATIM
    )
    set_property(TARGET bf2_bf2audio_native PROPERTY FOLDER "BF2")
    add_dependencies(bf2_bf2audio_native bf2_swiffplayer_native)
endif()

set(BF2_PLUGIN_STAGE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/Code/Build/stage_plugin.cmake")
add_custom_target(bf2_client_plugins
    COMMAND "${CMAKE_COMMAND}"
            "-DBF2_PLUGIN_SRC=${CMAKE_CURRENT_SOURCE_DIR}/Code/Build/bin_renddx9_native/RendDX9.dll"
            "-DBF2_PLUGIN_DST_DIR=${BF2_CLIENT_OUTPUT_DIR}"
            "-DBF2_PLUGIN_HINT=build it with Code/Build/renddx9.cmd native"
            -P "${BF2_PLUGIN_STAGE_SCRIPT}"
    COMMAND "${CMAKE_COMMAND}"
            "-DBF2_PLUGIN_SRC=${CMAKE_CURRENT_SOURCE_DIR}/Code/Build/bin_swiffplayer_native/SwiffPlayer.dll"
            "-DBF2_PLUGIN_DST_DIR=${BF2_CLIENT_OUTPUT_DIR}"
            "-DBF2_PLUGIN_HINT=build it with Code/Build/swiffplayer.cmd native"
            -P "${BF2_PLUGIN_STAGE_SCRIPT}"
    COMMAND "${CMAKE_COMMAND}"
            "-DBF2_PLUGIN_SRC=${CMAKE_CURRENT_SOURCE_DIR}/Code/Build/bin_bf2audio_native/BF2Audio.dll"
            "-DBF2_PLUGIN_DST_DIR=${BF2_CLIENT_OUTPUT_DIR}"
            "-DBF2_PLUGIN_HINT=build it with Code/Build/bf2audio.cmd native"
            -P "${BF2_PLUGIN_STAGE_SCRIPT}"
    COMMAND "${CMAKE_COMMAND}"
            "-DBF2_PLUGIN_SRC=${CMAKE_CURRENT_SOURCE_DIR}/Code/Build/bin_textureatlasbuilder_native/TextureAtlasBuilder.dll"
            "-DBF2_PLUGIN_DST_DIR=${BF2_CLIENT_OUTPUT_DIR}"
            "-DBF2_PLUGIN_HINT=build it with Code/Build/textureatlasbuilder.cmd native"
            -P "${BF2_PLUGIN_STAGE_SCRIPT}"
    COMMENT "Staging the four native plugins beside the diagnostic BF2.exe"
    VERBATIM
)
# Ordering AND obligation: all four plugin builds complete before anything is copied. This
# edge is unconditional on purpose - see the note above the plugin targets. Do not put it back
# behind a switch; a staged DLL older than its sources is indistinguishable from a real bug at the
# point where it hurts, which is a live debugging session.
add_dependencies(bf2_client_plugins bf2_textureatlas_native bf2_renddx9_native bf2_swiffplayer_native bf2_bf2audio_native)
add_dependencies(bf2_client bf2_client_plugins)
# System imports present in the retail BF2.exe.
target_link_libraries(bf2_client PRIVATE
    advapi32 crypt32 dbghelp dinput8 dsound dxguid gdi32 ole32 psapi shell32 user32 winmm ws2_32
)
# BF2.exe hosts a listen server, so it imports the CBF2VoipServer members from BF2VoipServer.dll
# (the client voice image built by bf2_voipserver_client above).
if(TARGET bf2_voipserver_client)
    target_link_libraries(bf2_client PRIVATE bf2_voipserver_client)
endif()
target_link_options(bf2_client PRIVATE
    /FORCE
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
    /SUBSYSTEM:WINDOWS,5.01
    /NODEFAULTLIB:python23.lib
    "/MAP:${BF2_CLIENT_OUTPUT_DIR}/BF2.map"
    /IGNORE:4006,4088,4098
)

set_property(TARGET bf2_client_objects PROPERTY FOLDER "BF2")

# Strict companion to bf2_client. This target is EXPECTED TO FAIL until every client external is
# recovered; the final LNK1120 count and the preceding symbol names are the measured whole-image
# work list (shared engine debt included, not a client-only delta).
# /FORCE:MULTIPLE tolerates the five known duplicate FileChangeManager constants without tolerating
# unresolveds. Keeping it separate lets bf2_client emit the RVA-0 PE used for map/import checks.
add_executable(bf2_client_linkcheck EXCLUDE_FROM_ALL
    $<TARGET_OBJECTS:bf2_client_objects>
    $<TARGET_OBJECTS:bf2_zlib_objects>
    $<TARGET_OBJECTS:bf2_tinyxml_objects>
    $<TARGET_OBJECTS:bf2_gamespy_objects>
    $<TARGET_OBJECTS:bf2_gamespy_sb_objects>
    $<TARGET_OBJECTS:bf2_gamespy_gp_objects>
    $<TARGET_OBJECTS:bf2_gamespy_cdkeyc_objects>
)
set_target_properties(bf2_client_linkcheck PROPERTIES
    LINKER_LANGUAGE CXX
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME BF2_linkcheck
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
bf2_configure_cpp_target(bf2_client_linkcheck)
bf2_set_image(bf2_client_linkcheck 0)

if(EXISTS "${BF2_DICE_PY_LIB}")
    target_link_libraries(bf2_client_linkcheck PRIVATE "${BF2_DICE_PY_LIB}")
endif()
target_link_libraries(bf2_client_linkcheck PRIVATE
    advapi32 crypt32 dbghelp dinput8 dsound dxguid gdi32 ole32 psapi shell32 user32 winmm ws2_32
)
add_dependencies(bf2_client_linkcheck bf2_novint_imports)
target_link_libraries(bf2_client_linkcheck PRIVATE "${BF2_NOVINT_IMPORT_LIB}")
target_link_options(bf2_client_linkcheck PRIVATE
    /FORCE:MULTIPLE
    /INCREMENTAL:NO
    /DEBUG
    /SUBSYSTEM:WINDOWS
    /NODEFAULTLIB:python23.lib
)

# The BF2 Editor links the client object
# set above, so it lives in this Windows-x86-only block. Every editor target is EXCLUDE_FROM_ALL.
add_subdirectory(Code/Tools/BF2Editor)
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
# The focused checks mirror Code/Tests/*.cmd, which are MSVC x86 gates; they are not part of the
# Linux port (its checks are the dice_py runtime check and the production run).
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

    add_custom_target(bf2_run_checks
        COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure -C $<CONFIG>
        DEPENDS
            bf2_bitstream_roundtrip
            bf2_networkable_abi
            bf2_bundle_template_core
            bf2_bundle_serialization
            bf2_runtime_pair
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        USES_TERMINAL
        VERBATIM
    )
    set_property(TARGET bf2_run_checks PROPERTY FOLDER "Checks")
endif()
