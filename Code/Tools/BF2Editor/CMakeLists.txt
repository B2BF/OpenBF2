# =================================================================================================
# BF2Editor - the rebuilt BF2 Editor (AGENTS.md "Editor").
#
# Added by the root CMakeLists.txt inside its Windows client block (x86 and the native x64 client,
# https://github.com/B2BF/OpenBF2/issues/7). EVERY TARGET HERE IS
# EXCLUDE_FROM_ALL: no server, archive, check or client target depends on the editor.
#
# The editor links the SAME client object set bf2_client links (bf2_client_objects, DEDICATED=0);
# it is not a second compilation of the engine. Editor-only code lives in this directory and is
# compiled with BF2_IMAGE_EDITOR=1. A shared TU that needs an editor-only region is compiled a second
# time here with BF2_IMAGE_EDITOR=1 and filtered out of the client objects (bf2_editor_shared_sources
# below lists them). See docs/building.md.
#
#   bf2_editor              BF2Editor.exe, a /FORCE diagnostic link like bf2_client
#   bf2_editor_renddx9      the editor's OWN native RendDX9.dll, built into this build tree and staged
#                           beside BF2Editor.exe (every bf2_editor build runs it)
#   bf2_editor_linkcheck    the strict companion link: its unresolved list is the editor's work list
#   bf2_editor_checks       the engine-free framework checks (command line, source control, undo
#                           history, selection)
#   bf2_editor_run_checks   builds and runs bf2_editor_checks
#
# Unlike bf2_client's plugin steps, nothing here writes the SHARED plugin directories
# (Code/Build/{obj,bin}_renddx9_native, ...): the editor's RendDX9 build is redirected into this
# build tree. SwiffPlayer and BF2Audio are not built or loaded (no menu; sound off until the editor
# stages its own BF2Audio.dll).
# =================================================================================================

set(BF2_EDITOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
# Inside this build tree, like bin_client: nothing here may ever be written over a retail image.
set(BF2_EDITOR_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin_editor")

# Engine-free: these also build the checks program.
set(bf2_editor_framework_sources
    "${BF2_EDITOR_ROOT}/App/CommandLine.cpp"
    "${BF2_EDITOR_ROOT}/App/CommandLine.h"
    "${BF2_EDITOR_ROOT}/Framework/CommandHistory.cpp"
    "${BF2_EDITOR_ROOT}/Framework/CommandHistory.h"
    "${BF2_EDITOR_ROOT}/Framework/EditorConfig.h"
    "${BF2_EDITOR_ROOT}/Framework/EditorLog.cpp"
    "${BF2_EDITOR_ROOT}/Framework/EditorLog.h"
    "${BF2_EDITOR_ROOT}/Framework/EditorSettings.cpp"
    "${BF2_EDITOR_ROOT}/Framework/EditorSettings.h"
    "${BF2_EDITOR_ROOT}/Framework/LayerSummary.cpp"
    "${BF2_EDITOR_ROOT}/Framework/LayerSummary.h"
    "${BF2_EDITOR_ROOT}/Framework/LevelObjectFile.cpp"
    "${BF2_EDITOR_ROOT}/Framework/Shortcuts.cpp"
    "${BF2_EDITOR_ROOT}/Framework/Shortcuts.h"
    "${BF2_EDITOR_ROOT}/Framework/LevelCopier.cpp"
    "${BF2_EDITOR_ROOT}/Framework/LevelCopier.h"
    "${BF2_EDITOR_ROOT}/Framework/LevelPacker.cpp"
    "${BF2_EDITOR_ROOT}/Framework/LevelPacker.h"
    "${BF2_EDITOR_ROOT}/Framework/UndergrowthConfig.cpp"
    "${BF2_EDITOR_ROOT}/Framework/UndergrowthConfig.h"
    "${BF2_EDITOR_ROOT}/Framework/UndergrowthMap.cpp"
    "${BF2_EDITOR_ROOT}/Framework/UndergrowthMap.h"
    "${BF2_EDITOR_ROOT}/Framework/ZipWriter.cpp"
    "${BF2_EDITOR_ROOT}/Framework/ZipWriter.h"
    "${BF2_EDITOR_ROOT}/Framework/LevelObjectFile.h"
    "${BF2_EDITOR_ROOT}/Framework/LevelSettingsFile.cpp"
    "${BF2_EDITOR_ROOT}/Framework/LevelSettingsFile.h"
    "${BF2_EDITOR_ROOT}/Framework/MapDescription.cpp"
    "${BF2_EDITOR_ROOT}/Framework/MapDescription.h"
    "${BF2_EDITOR_ROOT}/Framework/ModScaffold.cpp"
    "${BF2_EDITOR_ROOT}/Framework/ModScaffold.h"
    "${BF2_EDITOR_ROOT}/Framework/ObjectArchives.cpp"
    "${BF2_EDITOR_ROOT}/Framework/ObjectArchives.h"
    "${BF2_EDITOR_ROOT}/Framework/ObjectLibrary.cpp"
    "${BF2_EDITOR_ROOT}/Framework/ObjectLibrary.h"
    "${BF2_EDITOR_ROOT}/Framework/TemplateFile.cpp"
    "${BF2_EDITOR_ROOT}/Framework/TemplateClone.cpp"
    "${BF2_EDITOR_ROOT}/Framework/WizardRules.cpp"
    "${BF2_EDITOR_ROOT}/Framework/MaterialFiles.cpp"
    "${BF2_EDITOR_ROOT}/Framework/MaterialFiles.h"
    "${BF2_EDITOR_ROOT}/Framework/HelpDocuments.cpp"
    "${BF2_EDITOR_ROOT}/Framework/HelpDocuments.h"
    "${BF2_EDITOR_ROOT}/Framework/ConsoleCompletion.cpp"
    "${BF2_EDITOR_ROOT}/Framework/ConsoleCompletion.h"
    "${BF2_EDITOR_ROOT}/Framework/LevelEditorDefs.cpp"
    "${BF2_EDITOR_ROOT}/Framework/LevelEditorDefs.h"
    "${BF2_EDITOR_ROOT}/Framework/DescFile.cpp"
    "${BF2_EDITOR_ROOT}/Framework/DescFile.h"
    "${BF2_EDITOR_ROOT}/Framework/AnimationSystemFile.cpp"
    "${BF2_EDITOR_ROOT}/Framework/AnimationSystemFile.h"
    "${BF2_EDITOR_ROOT}/Framework/BafHeader.cpp"
    "${BF2_EDITOR_ROOT}/Framework/BafHeader.h"
    "${BF2_EDITOR_ROOT}/Framework/GameplayLayers.cpp"
    "${BF2_EDITOR_ROOT}/Framework/GameplayLayers.h"
    "${BF2_EDITOR_ROOT}/Framework/LightSetupFile.cpp"
    "${BF2_EDITOR_ROOT}/Framework/LightSetupFile.h"
    "${BF2_EDITOR_ROOT}/Framework/GroundPropertiesFile.cpp"
    "${BF2_EDITOR_ROOT}/Framework/GroundPropertiesFile.h"
    "${BF2_EDITOR_ROOT}/Framework/TerrainRawLayers.cpp"
    "${BF2_EDITOR_ROOT}/Framework/TerrainRawLayers.h"
    "${BF2_EDITOR_ROOT}/Framework/TerrainRawFile.cpp"
    "${BF2_EDITOR_ROOT}/Framework/TerrainRawFile.h"
    "${BF2_EDITOR_ROOT}/Framework/TerrainRawCompile.cpp"
    "${BF2_EDITOR_ROOT}/Framework/TerrainRawCompile.h"
    "${BF2_EDITOR_ROOT}/Framework/TerrainLowDetail.cpp"
    "${BF2_EDITOR_ROOT}/Framework/TerrainLowDetail.h"
    "${BF2_EDITOR_ROOT}/Framework/NewLevel.cpp"
    "${BF2_EDITOR_ROOT}/Framework/NewLevel.h"
    "${BF2_EDITOR_ROOT}/Framework/RoadFiles.cpp"
    "${BF2_EDITOR_ROOT}/Framework/RoadFiles.h"
    "${BF2_EDITOR_ROOT}/Framework/LightmapAtlas.cpp"
    "${BF2_EDITOR_ROOT}/Framework/LightmapAtlas.h"
    "${BF2_EDITOR_ROOT}/Framework/TextureEncode.cpp"
    "${BF2_EDITOR_ROOT}/Framework/TextureEncode.h"
    "${BF2_EDITOR_ROOT}/Framework/DdsImage.cpp"
    "${BF2_EDITOR_ROOT}/Framework/DdsImage.h"
    "${BF2_EDITOR_ROOT}/Framework/TemplateFile.h"
    "${BF2_EDITOR_ROOT}/Framework/Matrix4.h"
    "${BF2_EDITOR_ROOT}/Framework/Selection.cpp"
    "${BF2_EDITOR_ROOT}/Framework/Selection.h"
    "${BF2_EDITOR_ROOT}/Framework/SourceControl.cpp"
    "${BF2_EDITOR_ROOT}/Framework/SourceControl.h"
    "${BF2_EDITOR_ROOT}/Framework/SavePlan.cpp"
    "${BF2_EDITOR_ROOT}/Framework/SavePlan.h"
    "${BF2_EDITOR_ROOT}/Framework/TextDiff.cpp"
    "${BF2_EDITOR_ROOT}/Framework/TextDiff.h"
    "${BF2_EDITOR_ROOT}/Framework/HudSetup.cpp"
    "${BF2_EDITOR_ROOT}/Framework/HudSetup.h"
    "${BF2_EDITOR_ROOT}/Framework/ArchiveRewrite.cpp"
    "${BF2_EDITOR_ROOT}/Framework/ArchiveRewrite.h"
)
# Framework services that talk to the engine (editor only).
set(bf2_editor_engine_framework_sources
    "${BF2_EDITOR_ROOT}/Framework/EditorConsole.cpp"
    "${BF2_EDITOR_ROOT}/Framework/EditorConsole.h"
)
set(bf2_editor_app_sources
    "${BF2_EDITOR_ROOT}/App/EditorApp.cpp"
    "${BF2_EDITOR_ROOT}/App/EditorApp.h"
    "${BF2_EDITOR_ROOT}/App/EditorMain.cpp"
    "${BF2_EDITOR_ROOT}/App/EngineHost.cpp"
    "${BF2_EDITOR_ROOT}/App/EngineHost.h"
    "${BF2_EDITOR_ROOT}/App/HudEditor.cpp"
    "${BF2_EDITOR_ROOT}/App/HudEditor.h"
    "${BF2_EDITOR_ROOT}/App/ImGuiHost.cpp"
    "${BF2_EDITOR_ROOT}/App/ImGuiHost.h"
    "${BF2_EDITOR_ROOT}/App/EditorShell.cpp"
    "${BF2_EDITOR_ROOT}/App/EditorShell.h"
    "${BF2_EDITOR_ROOT}/App/EditorWindow.cpp"
    "${BF2_EDITOR_ROOT}/App/EditorWindow.h"
    "${BF2_EDITOR_ROOT}/App/FrameCapture.cpp"
    "${BF2_EDITOR_ROOT}/App/FrameCapture.h"
    "${BF2_EDITOR_ROOT}/App/FrameProfile.cpp"
    "${BF2_EDITOR_ROOT}/App/FrameProfile.h"
    "${BF2_EDITOR_ROOT}/App/FreeCamera.cpp"
    "${BF2_EDITOR_ROOT}/App/FreeCamera.h"
    "${BF2_EDITOR_ROOT}/App/SceneOverlay.cpp"
    "${BF2_EDITOR_ROOT}/App/SceneOverlay.h"
    "${BF2_EDITOR_ROOT}/App/ResourceBrowser.cpp"
    "${BF2_EDITOR_ROOT}/App/ResourceBrowser.h"
    "${BF2_EDITOR_ROOT}/App/EditorGame.cpp"
    "${BF2_EDITOR_ROOT}/App/EditorGame.h"
    "${BF2_EDITOR_ROOT}/App/EditorLevel.cpp"
    "${BF2_EDITOR_ROOT}/App/EditorLevel.h"
    "${BF2_EDITOR_ROOT}/App/PlayTest.cpp"
    "${BF2_EDITOR_ROOT}/App/PlayTest.h"
    "${BF2_EDITOR_ROOT}/App/TerrainLayerRecords.cpp"
    "${BF2_EDITOR_ROOT}/App/TerrainLayerRecords.h"
    "${BF2_EDITOR_ROOT}/App/EditSession.cpp"
    "${BF2_EDITOR_ROOT}/App/EditSession.h"
    "${BF2_EDITOR_ROOT}/App/EditSessionAnchors.cpp"
    "${BF2_EDITOR_ROOT}/App/EditSessionFiles.cpp"
    "${BF2_EDITOR_ROOT}/App/EditSessionTemplates.cpp"
    "${BF2_EDITOR_ROOT}/App/EditSessionEffects.cpp"
    "${BF2_EDITOR_ROOT}/App/EditSessionLighting.cpp"
    "${BF2_EDITOR_ROOT}/App/EditSessionHost.cpp"
    "${BF2_EDITOR_ROOT}/App/EditSessionHostTools.cpp"
    "${BF2_EDITOR_ROOT}/App/EditSessionOverlays.cpp"
    "${BF2_EDITOR_ROOT}/App/ViewRecorder.cpp"
    "${BF2_EDITOR_ROOT}/App/EditSessionMod.cpp"
    "${BF2_EDITOR_ROOT}/App/EditSessionLevelTools.cpp"
    "${BF2_EDITOR_ROOT}/App/EditSessionPreview.cpp"
    "${BF2_EDITOR_ROOT}/App/DescriptionDocuments.cpp"
    "${BF2_EDITOR_ROOT}/App/DescriptionDocuments.h"
    "${BF2_EDITOR_ROOT}/App/ViewRecorder.h"
    "${BF2_EDITOR_ROOT}/App/EditSessionMaterials.cpp"
    "${BF2_EDITOR_ROOT}/App/EditSessionAnimation.cpp"
    "${BF2_EDITOR_ROOT}/App/EditSessionSupply.cpp"
    "${BF2_EDITOR_ROOT}/App/EditSessionLayers.cpp"
    "${BF2_EDITOR_ROOT}/App/EditSessionLightSetup.cpp"
    "${BF2_EDITOR_ROOT}/App/GameplayLayerModel.cpp"
    "${BF2_EDITOR_ROOT}/App/GameplayLayerModel.h"
    "${BF2_EDITOR_ROOT}/App/AnimationDocuments.cpp"
    "${BF2_EDITOR_ROOT}/App/AnimationDocuments.h"
    "${BF2_EDITOR_ROOT}/App/MaterialDocuments.cpp"
    "${BF2_EDITOR_ROOT}/App/MaterialDocuments.h"
    "${BF2_EDITOR_ROOT}/App/FrozenFrame.cpp"
    "${BF2_EDITOR_ROOT}/App/ObjectWizard.cpp"
    "${BF2_EDITOR_ROOT}/App/WizardPanel.cpp"
    "${BF2_EDITOR_ROOT}/App/MaterialPanel.cpp"
    "${BF2_EDITOR_ROOT}/App/MaterialPanel.h"
    "${BF2_EDITOR_ROOT}/App/AnimationPanel.cpp"
    "${BF2_EDITOR_ROOT}/App/AnimationPanel.h"
    "${BF2_EDITOR_ROOT}/App/EditSessionLibrary.cpp"
    "${BF2_EDITOR_ROOT}/App/EditSessionRename.cpp"
    "${BF2_EDITOR_ROOT}/App/EditSessionTerrain.cpp"
    "${BF2_EDITOR_ROOT}/App/EditSessionUndergrowth.cpp"
    "${BF2_EDITOR_ROOT}/App/TerrainEditing.cpp"
    "${BF2_EDITOR_ROOT}/App/TerrainEditing.h"
    "${BF2_EDITOR_ROOT}/App/ShortcutKeys.cpp"
    "${BF2_EDITOR_ROOT}/App/ShortcutKeys.h"
    "${BF2_EDITOR_ROOT}/App/TemplateDocuments.cpp"
    "${BF2_EDITOR_ROOT}/App/TemplateDocuments.h"
    "${BF2_EDITOR_ROOT}/App/TerrainPanel.cpp"
    "${BF2_EDITOR_ROOT}/App/TerrainPanel.h"
    "${BF2_EDITOR_ROOT}/App/LibraryPanel.cpp"
    "${BF2_EDITOR_ROOT}/App/ObjectTemplatePanel.cpp"
    "${BF2_EDITOR_ROOT}/App/ObjectTemplatePanel.h"
    "${BF2_EDITOR_ROOT}/App/LibraryPanel.h"
    "${BF2_EDITOR_ROOT}/App/LevelDocument.cpp"
    "${BF2_EDITOR_ROOT}/App/LevelDocument.h"
    "${BF2_EDITOR_ROOT}/App/LevelSettings.cpp"
    "${BF2_EDITOR_ROOT}/App/LevelSettings.h"
    "${BF2_EDITOR_ROOT}/App/SessionPanels.cpp"
    "${BF2_EDITOR_ROOT}/App/SessionPanels.h"
    "${BF2_EDITOR_ROOT}/App/MinimapPanel.cpp"
    "${BF2_EDITOR_ROOT}/App/MinimapPanel.h"
    "${BF2_EDITOR_ROOT}/App/TestHarness.cpp"
    "${BF2_EDITOR_ROOT}/App/TestHarness.h"
    "${BF2_EDITOR_ROOT}/App/EditableTerrain.cpp"
    "${BF2_EDITOR_ROOT}/App/EditableTerrain.h"
    "${BF2_EDITOR_ROOT}/App/RoadNetwork.cpp"
    "${BF2_EDITOR_ROOT}/App/RoadNetwork.h"
    "${BF2_EDITOR_ROOT}/App/EditSessionRoads.cpp"
    "${BF2_EDITOR_ROOT}/App/EditSessionSave.cpp"
    "${BF2_EDITOR_ROOT}/App/SavePreviewPanel.cpp"
    "${BF2_EDITOR_ROOT}/App/SavePreviewPanel.h"
    "${BF2_EDITOR_ROOT}/App/RoadPanel.cpp"
    "${BF2_EDITOR_ROOT}/App/RoadPanel.h"
    "${BF2_EDITOR_ROOT}/App/StagingFolder.cpp"
    "${BF2_EDITOR_ROOT}/App/StagingFolder.h"
    "${BF2_EDITOR_ROOT}/App/PropertyPickers.cpp"
    "${BF2_EDITOR_ROOT}/App/PropertyPickers.h"
    "${BF2_EDITOR_ROOT}/App/TexturePalette.cpp"
    "${BF2_EDITOR_ROOT}/App/TexturePalette.h"
    "${BF2_EDITOR_ROOT}/App/ViewportTools.cpp"
    "${BF2_EDITOR_ROOT}/App/ViewportTools.h"
)

# SHARED TUs THE EDITOR COMPILES A SECOND TIME, with the client objects' exact contract plus
# BF2_IMAGE_EDITOR=1. Each one holds an editor-only region and is filtered out of
# the editor's copy of bf2_client_objects below, so the link sees exactly one definition.
#   Game/Main/BF2Engine.cpp       - SingleInstanceGuard takes no Battlefield2_Instance_Mutex (WP3).
#   Core/ModuleRegistry.cpp       - ModuleGame::handleEvent drives no menu or loading screen (WP4).
#   Game/Main/BF2EngineSetup.cpp  - no profile-console module, the HUD's only with +hud 1 (WP4); the
#                                   "HeightmapCluster" singleton is the editable class (WP8).
#   Physics/HeightmapCluster.cpp  - addHeightmap makes HeightmapEditable tiles (WP8).
#   Physics/HeightmapConsole.cpp  - heightmapCluster.create resolves the editable class (WP8).
#   Menu/Bf2TransformNode.cpp     - tells the HUD editor where each script-made HUD node stands (App/HudEditor).
set(bf2_editor_shared_sources
    "${BF2_ROOT}/Game/Main/BF2Engine.cpp"
    "${BF2_ROOT}/Core/ModuleRegistry.cpp"
    "${BF2_ROOT}/Game/Main/BF2EngineSetup.cpp"
    "${BF2_ROOT}/Physics/HeightmapCluster.cpp"
    "${BF2_ROOT}/Physics/HeightmapConsole.cpp"
    "${BF2_ROOT}/Menu/Bf2TransformNode.cpp"
)
# Matches the object path of each shared TU inside $<TARGET_OBJECTS:bf2_client_objects>.
set(bf2_editor_shared_object_regex
    "(Game[/\\\\]Main[/\\\\]BF2Engine|Core[/\\\\]ModuleRegistry|Game[/\\\\]Main[/\\\\]BF2EngineSetup|Physics[/\\\\]HeightmapCluster|Physics[/\\\\]HeightmapConsole|Menu[/\\\\]Bf2TransformNode)\\.cpp\\.obj")

# Dear ImGui (docking) and ImGuizmo are vendored under Code/BF2/External.
# Only the Win32 and D3D9 backends are built.
set(BF2_IMGUI_ROOT "${BF2_EXTERNAL_ROOT}/imgui")
set(BF2_IMGUIZMO_ROOT "${BF2_EXTERNAL_ROOT}/ImGuizmo/src")
set(bf2_editor_imgui_sources
    "${BF2_IMGUI_ROOT}/imgui.cpp"
    "${BF2_IMGUI_ROOT}/imgui_demo.cpp"
    "${BF2_IMGUI_ROOT}/imgui_draw.cpp"
    "${BF2_IMGUI_ROOT}/imgui_tables.cpp"
    "${BF2_IMGUI_ROOT}/imgui_widgets.cpp"
    "${BF2_IMGUI_ROOT}/backends/imgui_impl_dx9.cpp"
    "${BF2_IMGUI_ROOT}/backends/imgui_impl_win32.cpp"
    "${BF2_IMGUI_ROOT}/misc/cpp/imgui_stdlib.cpp"
    "${BF2_IMGUIZMO_ROOT}/ImGuizmo.cpp"
)
# ImGui configuration macros change its API, so every TU that includes imgui.h - ours and ImGui's
# own - must see the same set.
# IMGUI_ENABLE_TEST_ENGINE: ImGui reports each item's label and box to hooks App/TestHarness.cpp defines, for
# EditorTest.mouseToItem (no test engine is linked).
set(BF2_EDITOR_IMGUI_DEFINES IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_ENABLE_TEST_ENGINE)
set(BF2_EDITOR_IMGUI_INCLUDES
    "${BF2_IMGUI_ROOT}"
    "${BF2_IMGUI_ROOT}/backends"
    "${BF2_IMGUI_ROOT}/misc/cpp"
    "${BF2_IMGUIZMO_ROOT}"
)

# The compile contract every editor TU shares. _ITERATOR_DEBUG_LEVEL=0 is not optional: the editor
# passes STL objects to and from the client objects and its RendDX9.dll, so it must use their exact
# STL layout (bf2_client_objects sets the same define).
function(bf2_editor_configure target)
    bf2_configure_cpp_target("${target}")
    bf2_set_image("${target}" 0)
    target_compile_definitions("${target}" PRIVATE
        BF2_IMAGE_EDITOR=1
        _ITERATOR_DEBUG_LEVEL=0
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        ${BF2_EDITOR_IMGUI_DEFINES}
    )
    target_include_directories("${target}" PRIVATE "${BF2_EDITOR_ROOT}" "${BF2_ROOT}" ${BF2_EDITOR_IMGUI_INCLUDES}
        ${BF2_PYTHON_INCLUDE_DIRS} "${BF2_EXTERNAL_ROOT}/zlib-1.2.3" "${BF2_EXTERNAL_ROOT}/zlib-1.2.3/contrib/minizip")
    target_compile_options("${target}" PRIVATE ${BF2_CLIENT_X64_COMPILE_OPTIONS})
    set_target_properties("${target}" PROPERTIES FOLDER "BF2Editor")
endfunction()

# The shared TUs' second compilation: bf2_client_objects' settings exactly (CMakeLists.txt at the
# root), plus the editor image define. Nothing else may differ, or the editor's copy of a shared TU
# would not be the code the client runs.
add_library(bf2_editor_shared_objects OBJECT EXCLUDE_FROM_ALL ${bf2_editor_shared_sources})
bf2_configure_cpp_target(bf2_editor_shared_objects)
bf2_set_image(bf2_editor_shared_objects 0)
bf2_set_platform_compat(bf2_editor_shared_objects)
target_compile_definitions(bf2_editor_shared_objects PRIVATE
    _ITERATOR_DEBUG_LEVEL=0
    BF2_IMAGE_EDITOR=1
)
target_compile_options(bf2_editor_shared_objects PRIVATE ${BF2_CLIENT_X64_COMPILE_OPTIONS})
target_include_directories(bf2_editor_shared_objects PRIVATE
    ${BF2_PYTHON_INCLUDE_DIRS}
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3"
    "${BF2_EXTERNAL_ROOT}/zlib-1.2.3/contrib/minizip"
)
set_target_properties(bf2_editor_shared_objects PROPERTIES FOLDER "BF2Editor")

# Third-party C++ is compiled with the runtime, STL layout and debug information of the editor, but
# without the tree's image define or warning set: it is upstream code, not ours to adjust.
add_library(bf2_editor_imgui_objects OBJECT EXCLUDE_FROM_ALL ${bf2_editor_imgui_sources})
target_compile_features(bf2_editor_imgui_objects PRIVATE cxx_std_14)
target_compile_options(bf2_editor_imgui_objects PRIVATE /EHsc /W3)
bf2_add_compile_debug_info(bf2_editor_imgui_objects)
bf2_use_static_msvc_runtime(bf2_editor_imgui_objects)
target_compile_definitions(bf2_editor_imgui_objects PRIVATE _ITERATOR_DEBUG_LEVEL=0 ${BF2_EDITOR_IMGUI_DEFINES})
target_include_directories(bf2_editor_imgui_objects PRIVATE ${BF2_EDITOR_IMGUI_INCLUDES})
set_target_properties(bf2_editor_imgui_objects PROPERTIES FOLDER "BF2Editor")

function(bf2_editor_output target)
    set_target_properties("${target}" PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${BF2_EDITOR_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${BF2_EDITOR_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${BF2_EDITOR_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RECONSTRUCTION "${BF2_EDITOR_OUTPUT_DIR}"
        PDB_OUTPUT_DIRECTORY "${BF2_EDITOR_OUTPUT_DIR}"
        PDB_OUTPUT_DIRECTORY_DEBUG "${BF2_EDITOR_OUTPUT_DIR}"
        PDB_OUTPUT_DIRECTORY_RELEASE "${BF2_EDITOR_OUTPUT_DIR}"
        PDB_OUTPUT_DIRECTORY_RECONSTRUCTION "${BF2_EDITOR_OUTPUT_DIR}"
    )
endfunction()

add_library(bf2_editor_objects OBJECT EXCLUDE_FROM_ALL
    ${bf2_editor_framework_sources}
    ${bf2_editor_engine_framework_sources}
    ${bf2_editor_app_sources}
)
bf2_editor_configure(bf2_editor_objects)
source_group(TREE "${BF2_EDITOR_ROOT}" PREFIX "BF2Editor" FILES
    ${bf2_editor_framework_sources}
    ${bf2_editor_engine_framework_sources}
    ${bf2_editor_app_sources}
)

# The objects both editor links take: the editor's own, the client object set, and the vendored
# third-party C objects bf2_client links.
set(bf2_editor_link_objects
    $<TARGET_OBJECTS:bf2_editor_objects>
    $<TARGET_OBJECTS:bf2_editor_imgui_objects>
    $<TARGET_OBJECTS:bf2_editor_shared_objects>
    "$<FILTER:$<TARGET_OBJECTS:bf2_client_objects>,EXCLUDE,${bf2_editor_shared_object_regex}>"
    $<TARGET_OBJECTS:bf2_zlib_objects>
    $<TARGET_OBJECTS:bf2_tinyxml_objects>
    $<TARGET_OBJECTS:bf2_gamespy_objects>
    $<TARGET_OBJECTS:bf2_gamespy_sb_objects>
    $<TARGET_OBJECTS:bf2_gamespy_gp_objects>
    $<TARGET_OBJECTS:bf2_gamespy_cdkeyc_objects>
)
# The system imports bf2_client links (the client objects need them).
set(bf2_editor_system_libs
    advapi32 comdlg32 crypt32 dbghelp dinput8 dsound dxguid gdi32 mfplat mfreadwrite mfuuid ole32 psapi shell32 user32 winmm ws2_32
)

function(bf2_editor_link_common target)
    set_target_properties("${target}" PROPERTIES LINKER_LANGUAGE CXX WIN32_EXECUTABLE TRUE)
    bf2_editor_configure("${target}")
    bf2_editor_output("${target}")
    if(EXISTS "${BF2_DICE_PY_LIB}")
        target_link_libraries("${target}" PRIVATE "${BF2_DICE_PY_LIB}")
    endif()
    target_link_libraries("${target}" PRIVATE ${bf2_editor_system_libs})
    # x86 only: the x64 client objects import nothing from the 32-bit NovintHFX.dll.
    if(TARGET bf2_novint_imports)
        add_dependencies("${target}" bf2_novint_imports)
        target_link_libraries("${target}" PRIVATE "${BF2_NOVINT_IMPORT_LIB}")
    endif()
endfunction()

# ---- BF2Editor.exe ------------------------------------------------------------------------------
# /FORCE, like bf2_client: the client object set still has unresolved externals (the strict list
# below). A /FORCE image proves only the paths it runs.
# /ENTRY: see App/EditorMain.cpp - the game's WinMain is linked but never the entry.
add_executable(bf2_editor EXCLUDE_FROM_ALL ${bf2_editor_link_objects})
bf2_editor_link_common(bf2_editor)
set_target_properties(bf2_editor PROPERTIES OUTPUT_NAME BF2Editor)
target_link_options(bf2_editor PRIVATE
    /FORCE
    /INCREMENTAL:NO
    /DEBUG
    /SUBSYSTEM:WINDOWS
    /ENTRY:wWinMainCRTStartup
    /NODEFAULTLIB:python23.lib
    "/MAP:${BF2_EDITOR_OUTPUT_DIR}/BF2Editor.map"
    /IGNORE:4006,4088,4098
)
if(EXISTS "${BF2_DICE_PY_DLL}")
    add_custom_command(TARGET bf2_editor POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${BF2_DICE_PY_DLL}" "${BF2_EDITOR_OUTPUT_DIR}/dice_py.dll"
        COMMENT "Staging dice_py.dll beside BF2Editor.exe"
        VERBATIM
    )
endif()
# The x86 client objects import NovintHFX.dll; bf2_client stages the original the same way. Reading
# the game directory is all this does.
if(NOT BF2_BUILD_X64 AND EXISTS "${BF2_GAME_DIR}/NovintHFX.dll")
    add_custom_command(TARGET bf2_editor POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${BF2_GAME_DIR}/NovintHFX.dll" "${BF2_EDITOR_OUTPUT_DIR}/NovintHFX.dll"
        COMMENT "Staging the original Novint SDK companion beside BF2Editor.exe"
        VERBATIM
    )
endif()

# ---- The editor's own native RendDX9.dll (WP4) ------------------------------------------------------
# Built by the tree's own script (Code/Build/renddx9.cmd native, or x64 in the x64 configuration -
# BF2_PLUGIN_FLAVOUR from the root CMakeLists.txt), REDIRECTED into this build tree
# through the script's agent-isolation variables BF2_RENDDX9_OUT / BF2_RENDDX9_BIN, so the shared plugin
# directories (Code/Build/obj_renddx9_native, bin_renddx9_native) are never rewritten, then staged
# beside BF2Editor.exe. LoadLibrary searches the exe's directory first; a working directory holding the
# retail RendDX9.dll must never supply the editor's renderer. Rebuilt with every bf2_editor build, for
# the reason bf2_client's plugins are (CMakeLists.txt at the root): a DLL older than its sources is
# indistinguishable from a real bug. About 20 s.
set(BF2_EDITOR_RENDDX9_DIR "${CMAKE_BINARY_DIR}/editor_renddx9")
file(TO_NATIVE_PATH "${BF2_EDITOR_RENDDX9_DIR}/obj" BF2_EDITOR_RENDDX9_OUT)
file(TO_NATIVE_PATH "${BF2_EDITOR_RENDDX9_DIR}/bin" BF2_EDITOR_RENDDX9_BIN)
# The TextureAtlasBuilder.dll that RendDX9.dll imports (RW-278), native flavour, built the same way and staged
# beside it: RendDX9.dll does not load without it.
file(TO_NATIVE_PATH "${BF2_EDITOR_RENDDX9_DIR}/atlas_obj" BF2_EDITOR_ATLAS_OUT)
file(TO_NATIVE_PATH "${BF2_EDITOR_RENDDX9_DIR}/atlas_bin" BF2_EDITOR_ATLAS_BIN)
# renddx9_for_editor.cmd runs it - or keeps the DLL built before when BF2_EDITOR_KEEP_RENDDX9=1
# (build_editor.ps1 -KeepRendDX9), which the log then says.
file(TO_NATIVE_PATH "${BF2_EDITOR_ROOT}/renddx9_for_editor.cmd" BF2_EDITOR_RENDDX9_SCRIPT)
add_custom_target(bf2_editor_renddx9
    COMMAND "${CMAKE_COMMAND}" -E env
            "BF2_RENDDX9_OUT=${BF2_EDITOR_RENDDX9_OUT}" "BF2_RENDDX9_BIN=${BF2_EDITOR_RENDDX9_BIN}"
            "BF2_TEXTUREATLAS_OUT=${BF2_EDITOR_ATLAS_OUT}" "BF2_TEXTUREATLAS_BIN=${BF2_EDITOR_ATLAS_BIN}"
            cmd.exe /d /c "${BF2_EDITOR_RENDDX9_SCRIPT}" ${BF2_PLUGIN_FLAVOUR}
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${BF2_EDITOR_OUTPUT_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${BF2_EDITOR_RENDDX9_DIR}/bin/RendDX9.dll" "${BF2_EDITOR_OUTPUT_DIR}/RendDX9.dll"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${BF2_EDITOR_RENDDX9_DIR}/atlas_bin/TextureAtlasBuilder.dll" "${BF2_EDITOR_OUTPUT_DIR}/TextureAtlasBuilder.dll"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/Code/Build"
    COMMENT "Building the editor's ${BF2_PLUGIN_FLAVOUR} RendDX9.dll and TextureAtlasBuilder.dll (into ${BF2_EDITOR_RENDDX9_DIR})"
    VERBATIM
)
set_property(TARGET bf2_editor_renddx9 PROPERTY FOLDER "BF2Editor")
add_dependencies(bf2_editor bf2_editor_renddx9)

# ---- The strict companion link --------------------------------------------------------------------
# EXPECTED TO FAIL until the client object set links clean; the LNK2019/LNK2001 names are the
# editor's whole-image work list (shared engine debt included). /FORCE:MULTIPLE tolerates only
# duplicates, as for bf2_client_linkcheck. Compare its symbol set with bf2_client_linkcheck's before
# attributing a gap to editor code.
add_executable(bf2_editor_linkcheck EXCLUDE_FROM_ALL ${bf2_editor_link_objects})
bf2_editor_link_common(bf2_editor_linkcheck)
set_target_properties(bf2_editor_linkcheck PROPERTIES OUTPUT_NAME BF2Editor_linkcheck)
target_link_options(bf2_editor_linkcheck PRIVATE
    /FORCE:MULTIPLE
    /INCREMENTAL:NO
    /DEBUG
    /SUBSYSTEM:WINDOWS
    /ENTRY:wWinMainCRTStartup
    /NODEFAULTLIB:python23.lib
)

# ---- Engine-free checks -------------------------------------------------------------------------
add_executable(bf2_editor_checks EXCLUDE_FROM_ALL
    ${bf2_editor_framework_sources}
    "${BF2_EDITOR_ROOT}/Tests/EditorChecks.cpp"
    $<TARGET_OBJECTS:bf2_zlib_objects>
)
bf2_editor_configure(bf2_editor_checks)
bf2_editor_output(bf2_editor_checks)
add_custom_target(bf2_editor_run_checks
    COMMAND "$<TARGET_FILE:bf2_editor_checks>"
    DEPENDS bf2_editor_checks
    COMMENT "Running the BF2Editor framework checks"
    VERBATIM
)
set_property(TARGET bf2_editor_run_checks PROPERTY FOLDER "BF2Editor")
