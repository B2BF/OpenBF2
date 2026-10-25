@echo off
setlocal EnableDelayedExpansion
rem ===========================================================================
rem SCAFFOLDING - NOT RECOVERED SOURCE. Builds the REPLACEMENT RendDX9.dll.
rem
rem   Code\Build\build.cmd      the dedicated server, bf2_w32ded.exe
rem   Code\Build\renddx9.cmd    THIS. A different shipped image entirely.
rem
rem IT NEVER WRITES INTO THE GAME DIRECTORY. Output goes to
rem    Code\Build\bin_renddx9_native\. The shipped RendDX9.dll at
rem    "D:\B2BF Launcher\Battlefield2\RendDX9.dll" is the REFERENCE BINARY and
rem    overwriting it would destroy the only copy this rebuild reads from.
rem    Testing against the real client is a DELIBERATE, SEPARATE act - see
rem    docs\building.md for host ABI and runtime setup.
rem
rem   usage:  renddx9.cmd [native]   rebuilt host ABI (/MD, no Memory.dll)
rem [CHOSEN, not recovered] Native-only builds: https://github.com/B2BF/OpenBF2/issues/7
rem ===========================================================================
if not "%~2"=="" goto :usage
if not "%~1"=="" if /i not "%~1"=="native" goto :usage
call "%~dp0vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1

for %%I in ("%~dp0..") do set CODE=%%~fI
set ABI=%CODE%\RendDX9Abi
set BF2=%CODE%\BF2
set OUT=%~dp0obj_renddx9_native
set BIN=%~dp0bin_renddx9_native
set CRT=/MD
set PLUGIN_ABI=/DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0
rem Agent/CI isolation keeps builds out of another contributor's directories.
if defined BF2_RENDDX9_OUT set OUT=%BF2_RENDDX9_OUT%
if defined BF2_RENDDX9_BIN set BIN=%BF2_RENDDX9_BIN%

rem Prefer the stock import library from the exact SDK generation BF2 used.  Do not silently take
rem a later d3dx9.lib: the June 2010 SDK's library names d3dx9_43.dll, while retail RendDX9 names
rem d3dx9_25.dll.  Keep the narrow .def-generated library as the reproducible no-SDK fallback.
set "DXSDK_2005_ROOT="
set "DXSDK_CANDIDATE="
if defined DXSDK_DIR for %%I in ("%DXSDK_DIR%\.") do set "DXSDK_CANDIDATE=%%~fI"
if defined DXSDK_CANDIDATE if exist "%DXSDK_CANDIDATE%\Include\d3dx9core.h" (
    findstr /c:"#define D3DX_SDK_VERSION 25" "%DXSDK_CANDIDATE%\Include\d3dx9core.h" >nul 2>&1
    if not errorlevel 1 if exist "%DXSDK_CANDIDATE%\Lib\x86\d3dx9.lib" set "DXSDK_2005_ROOT=%DXSDK_CANDIDATE%"
)
if not defined DXSDK_2005_ROOT if exist "C:\Program Files (x86)\Microsoft DirectX 9.0 SDK (April 2005)\Include\d3dx9core.h" (
    findstr /c:"#define D3DX_SDK_VERSION 25" "C:\Program Files (x86)\Microsoft DirectX 9.0 SDK (April 2005)\Include\d3dx9core.h" >nul 2>&1
    if not errorlevel 1 if exist "C:\Program Files (x86)\Microsoft DirectX 9.0 SDK (April 2005)\Lib\x86\d3dx9.lib" set "DXSDK_2005_ROOT=C:\Program Files (x86)\Microsoft DirectX 9.0 SDK (April 2005)"
)
set "D3DX25_LIB=%OUT%\d3dx9_25.lib"
if defined DXSDK_2005_ROOT set "D3DX25_LIB=%DXSDK_2005_ROOT%\Lib\x86\d3dx9.lib"

rem /MD - shares the rebuilt client's native CRT/STL ABI; BfMemory's native branch does
rem   keep the global replacements, but routes them to that shared CRT and never references
rem   Memory.dll. Recovered objects mix new/delete with engineAlloc/engineFree, so both routes must
rem   terminate in one heap.
rem /DBF2_IMAGE_RENDDX9 - the guard that admits the four module translation units under Code\BF2.
rem   Without it they compile to nothing, which is exactly what build.cmd's recursive sweep sees.
rem /Zi + /DEBUG below - a fault in this DLL has to resolve to FILE:LINE, or the whole
rem   instrument-and-compare workflow the handoff describes cannot be run.
rem /DDEDICATED=0 + /FI Core\BuildConfig.h, 2026-09-02: the IMAGE define. This DLL is part of the CLIENT
rem configuration of the shared source, so client-only regions under #if !DEDICATED are compiled here and
rem the header errors if a compile line forgets to say which image it builds.
set FLAGS=/nologo /c %CRT% /std:c++14 /permissive /EHsc /W4 /wd4100 /wd4127 /Zi /FS ^
 /DBF2_IMAGE_RENDDX9 %PLUGIN_ABI% /DDEDICATED=0 /FI"%BF2%\Core\BuildConfig.h" /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS /Fd"%OUT%\RendDX9.pdb"
rem BF2_RENDDX9_EDITOR=1 (set only by Code\Tools\BF2Editor\renddx9_for_editor.cmd): the editor's own
rem flavour. /DBF2_IMAGE_EDITOR=1 admits the editor-only regions of shared RendDX9 TUs (AGENTS.md, Editor
rem front: editor-only needs in shared code go behind the editor image define, never into game behaviour).
if "%BF2_RENDDX9_EDITOR%"=="1" set FLAGS=%FLAGS% /DBF2_IMAGE_EDITOR=1

if not exist "%OUT%" mkdir "%OUT%"
if not exist "%BIN%" mkdir "%BIN%"
del /q "%OUT%\*.obj" >nul 2>&1
del /q "%OUT%\*.log" >nul 2>&1

rem ---------------------------------------------------------------------------
rem STEP 1 - import libraries.
rem
rem Build the Bink import library. D3DX uses the exact April 2005 SDK library when present,
rem or manufactures the same
rem d3dx9_25.dll binding from the retained .def when that SDK is absent.
rem ---------------------------------------------------------------------------
set LIB_BAD=0
if defined DXSDK_2005_ROOT (
    >"%OUT%\lib.log" echo [D3DX] official April 2005 SDK import library: "%D3DX25_LIB%"
) else (
    >"%OUT%\lib.log" echo [D3DX] exact SDK absent; generating d3dx9_25.lib from D3dx9_25.def
    lib /nologo /def:"%ABI%\D3dx9_25.def" /machine:x86 /out:"%D3DX25_LIB%" >>"%OUT%\lib.log" 2>&1
    if errorlevel 1 set LIB_BAD=1
)
echo [1/4] import libraries - native ABI, no Memory.dll
lib /nologo /def:"%ABI%\binkw32.def" /machine:x86 /out:"%OUT%\binkw32.lib" >>"%OUT%\lib.log" 2>&1
if errorlevel 1 set LIB_BAD=1
rem TextureAtlasBuilder.dll: RendDX9.dll 1.50 carries a STATIC import
rem descriptor for it with the two exported creates (RendDX9Abi\TextureAtlasBuilder.def), which
rem TextureAtlas\TextureAtlas.cpp's atlas generator calls. Static, as in retail: without the DLL on
rem the search path RendDX9.dll does not load. Stage the rebuilt atlas DLL beside the executable;
rem Code\Build\textureatlasbuilder.cmd builds it and CMake stages it with the client.
lib /nologo /def:"%ABI%\TextureAtlasBuilder.def" /machine:x86 /out:"%OUT%\TextureAtlasBuilder.lib" >>"%OUT%\lib.log" 2>&1
if errorlevel 1 set LIB_BAD=1
if not "%LIB_BAD%"=="0" (
    echo   FAILED - see "%OUT%\lib.log"
    type "%OUT%\lib.log"
    exit /b 1
)

rem ---------------------------------------------------------------------------
rem STEP 2 - compile. The list is EXPLICIT, not a glob.
rem
rem A recursive glob over Code\BF2 would sweep in the entire dedicated server. The four DICE
rem translation units this image needs are named one at a time, and adding a fifth is a deliberate
rem edit here - which is the same discipline build.cmd's engine manifest enforces from the other
rem direction.
rem ---------------------------------------------------------------------------
echo [2/4] compiling
set SOURCES=^
 "%ABI%\BfMemory.cpp" ^
 "%ABI%\Msvcr71Heap.cpp" ^
 "%ABI%\Vc71String.cpp" ^
 "%ABI%\Frontier.cpp" ^
 "%ABI%\CrashReport.cpp" ^
 "%ABI%\EngineFile.cpp" ^
 "%ABI%\DllMain.cpp" ^
 "%ABI%\PluginEntry.cpp" ^
 "%BF2%\RendDX9\BF2Render.cpp" ^
 "%BF2%\RendDX9\PostProductionFrameWork.cpp" ^
 "%BF2%\RendDX9\RendDX9Init.cpp" ^
 "%BF2%\RendDX9\BufferManager.cpp" ^
 "%BF2%\RendDX9\QueryManager.cpp" ^
 "%BF2%\RendDX9\RendDX9Types.cpp" ^
 "%BF2%\RendDX9\Texture.cpp" ^
 "%BF2%\RendDX9\TextureManager.cpp" ^
 "%BF2%\RendDX9\TextureAtlas\TextureAtlas.cpp" ^
 "%BF2%\RendDX9\ResourceManager.cpp" ^
 "%BF2%\RendDX9\VideoOptions.cpp" ^
 "%BF2%\RendDX9\Effect.cpp" ^
 "%BF2%\RendDX9\FXParameter.cpp" ^
 "%BF2%\RendDX9\ShaderManager.cpp" ^
 "%BF2%\RendDX9\StateManager.cpp" ^
 "%BF2%\RendDX9\RendDX9Draw.cpp" ^
 "%BF2%\RendDX9\RenderQueues.cpp" ^
 "%BF2%\RendDX9\Ra\EffectManager.cpp" ^
 "%BF2%\RendDX9\Ra\RaShaderPermutator.cpp" ^
 "%BF2%\RendDX9\Ra\RaShader.cpp" ^
 "%BF2%\RendDX9\Ra\RaVertexStreams.cpp" ^
 "%BF2%\RendDX9\Ra\RaStreamDataEditable.cpp" ^
 "%BF2%\RendMisc\RendModule.cpp" ^
 "%BF2%\RendMisc\RenderView.cpp" ^
 "%BF2%\RendMisc\EnvMapSystem\EnvMapManager.cpp" ^
 "%BF2%\RendMisc\EnvMapSystem\EnvMapObject.cpp" ^
 "%BF2%\Math\Mat4.cpp" ^
 "%BF2%\Math\Mat4Inverse.cpp" ^
 "%BF2%\Math\Vec3.cpp" ^
 "%BF2%\Math\Frustum.cpp" ^
 "%BF2%\Math\Plane3.cpp" ^
 "%BF2%\IO\Tokenizer.cpp" ^
 "%BF2%\Geom\ModuleGeom.cpp" ^
 "%BF2%\Geom\GeometryTemplateManager.cpp" ^
 "%BF2%\Geom\MeshTemplate.cpp" ^
 "%BF2%\Geom\StaticMeshTemplate.cpp" ^
 "%BF2%\Geom\SkinnedMeshTemplate.cpp" ^
 "%BF2%\Geom\SkinnedMesh.cpp" ^
 "%BF2%\Geom\BundledMeshTemplate.cpp" ^
 "%BF2%\Geom\BundledMeshMaterial.cpp" ^
 "%BF2%\Geom\BundledMeshStateManager.cpp" ^
 "%BF2%\Geom\BundledMesh.cpp" ^
 "%BF2%\Geom\BundledMeshDraw.cpp" ^
 "%BF2%\Geom\BundledMeshHeight.cpp" ^
 "%BF2%\Geom\StaticMesh.cpp" ^
 "%BF2%\Geom\StaticMeshLightmapGeneration.cpp" ^
 "%BF2%\Geom\StaticMeshMaterial.cpp" ^
 "%BF2%\Geom\StaticMeshStateManager.cpp" ^
 "%BF2%\Geom\SkyDome.cpp" ^
 "%BF2%\Geom\UndergrowthSystem.cpp" ^
 "%BF2%\Geom\UndergrowthSystemLoad.cpp" ^
 "%BF2%\Geom\UndergrowthSystemRender.cpp" ^
 "%BF2%\Geom\Terrain.cpp" ^
 "%BF2%\Geom\QuadPatch.cpp" ^
 "%BF2%\Geom\SurroundingPatch.cpp" ^
 "%BF2%\Geom\TerrainDraw.cpp" ^
 "%BF2%\Geom\TerrainEditable.cpp" ^
 "%BF2%\Geom\TerrainEditableGetSets.cpp" ^
 "%BF2%\Geom\QuadPatchEditable.cpp" ^
 "%BF2%\Geom\SurroundingPatchEditable.cpp" ^
 "%BF2%\Geom\TerrainLightSampling.cpp" ^
 "%BF2%\Geom\LightmapGeneration\LightmapGeneration.cpp" ^
 "%BF2%\Geom\LightmapGeneration\MeshLightSampling.cpp" ^
 "%BF2%\Geom\ObjectDrawer.cpp" ^
 "%BF2%\Geom\XpLightManager.cpp" ^
 "%BF2%\Geom\NametagManager.cpp" ^
 "%BF2%\Geom\NameTagRenderer.cpp" ^
 "%BF2%\Geom\Overgrowth\OverGrowth.cpp" ^
 "%BF2%\Geom\Overgrowth\OverGrowthLoad.cpp" ^
 "%BF2%\Geom\Overgrowth\OverGrowthCells.cpp" ^
 "%BF2%\Geom\Overgrowth\OverGrowthCellRender.cpp" ^
 "%BF2%\Geom\Overgrowth\OverGrowthRender.cpp" ^
 "%BF2%\Geom\RoadCompiledRenderer.cpp" ^
 "%BF2%\Geom\RoadCompiledTemplate.cpp" ^
 "%BF2%\Geom\RoadCompiled.cpp" ^
 "%BF2%\Geom\RoadTemplate.cpp" ^
 "%BF2%\Geom\RoadEditableRenderer.cpp" ^
 "%BF2%\Geom\RoadEditable.cpp" ^
 "%BF2%\Geom\DecalSystem\DecalManager.cpp" ^
 "%BF2%\Geom\DecalSystem\DecalObject.cpp" ^
 "%BF2%\Geom\DecalSystem\DecalTemplate.cpp" ^
 "%BF2%\Geom\DecalSystem\DecalRenderer.cpp" ^
 "%BF2%\Geom\ParticleSystem\ParticleSystemManager.cpp" ^
 "%BF2%\Geom\HemiMapManager\HemiMapManager.cpp" ^
 "%BF2%\Geom\ParticleSystem\ParticleSystemTemplate.cpp" ^
 "%BF2%\Geom\ParticleSystem\ParticleSystemTemplateType1.cpp" ^
 "%BF2%\Geom\ParticleSystem\MeshParticleMesh\MeshParticleMeshTemplate.cpp" ^
 "%BF2%\Geom\ParticleSystem\MeshParticleMesh\MeshParticleMesh.cpp" ^
 "%BF2%\Geom\ParticleSystem\TrailSystemTemplate.cpp" ^
 "%BF2%\Geom\ParticleSystem\ParticleSystemTemplateType3.cpp" ^
 "%BF2%\Geom\ParticleSystem\NSAParticleSystemTemplate.cpp" ^
 "%BF2%\Geom\StaticMeshRenderer.cpp" ^
 "%BF2%\Geom\SkinnedMeshRenderer.cpp" ^
 "%BF2%\Geom\SwiffRender.cpp" ^
 "%BF2%\GeomMisc\ModuleGeomMisc.cpp" ^
 "%BF2%\GeomMisc\FSQuadDrawer.cpp" ^
 "%BF2%\GeomMisc\DebugTextWriter.cpp" ^
 "%BF2%\GeomMisc\FontSystem\FontSystem.cpp" ^
 "%BF2%\GeomMisc\FontSystem\SwiffFont.cpp" ^
 "%BF2%\GeomMisc\OverlayTexture.cpp" ^
 "%BF2%\GeomMisc\DebugLineGraphD3d.cpp" ^
 "%BF2%\GeomMisc\PerlinNoise.cpp" ^
 "%BF2%\GeomMisc\SplineControlPoint.cpp" ^
 "%BF2%\GeomMisc\EditorSpline.cpp" ^
 "%BF2%\GeomMisc\MenuRend\MemeTexture.cpp" ^
 "%BF2%\GeomMisc\MenuRend\FSMoviePlayer.cpp" ^
 "%BF2%\GeomMisc\MenuRend\Video.cpp" ^
 "%BF2%\GeomMisc\MenuRend\QuadDrawer.cpp"

rem These sources use the rebuilt host's native std::string/list/map ABI.
rem Keep them in a separate list and loop: appending them to SOURCES made that one `set` line
rem exceed cmd.exe's 8191-character limit at deeper checkout paths, which silently dropped the
rem whole assignment and left every ConsoleObject symbol unresolved at link time.
set CONSOLE_SOURCES= "%BF2%\Core\ConsoleObjectBase.cpp" "%ABI%\NativeConsoleObjects.cpp" "%BF2%\Geom\LightManagerConsole.cpp" "%BF2%\Geom\HemiMapManager\HemiMapManagerConsole.cpp" "%BF2%\Geom\SkyDomeConsole.cpp" "%BF2%\Geom\TerrainConsole.cpp" "%BF2%\RendDX9\RendererConsole.cpp" "%BF2%\Geom\NametagManagerConsole.cpp" "%BF2%\Geom\Overgrowth\OverGrowthConsole.cpp" "%BF2%\Geom\UndergrowthSystemConsole.cpp" "%BF2%\Geom\DecalSystem\DecalManagerConsole.cpp"
rem RD1 (2026-09-30): the `Object.geometry.loadMesh` row - native console ABI, like the list above.
set CONSOLE_SOURCES=!CONSOLE_SOURCES! "%BF2%\Geom\ObjectGeometryConsole.cpp"
rem The 2006 editor's `lightmapGeneration` rows - native console ABI; the TU is empty unless
rem BF2_IMAGE_EDITOR (the BF2_RENDDX9_EDITOR=1 flavour) is set.
set CONSOLE_SOURCES=!CONSOLE_SOURCES! "%BF2%\Geom\LightmapGeneration\LightmapGenerationConsole.cpp"
rem The StaticMeshRenderer atlas rows (loadAllLightMapFiles / loadAllLightMapFilesDLL) - native console ABI.
set CONSOLE_SOURCES=!CONSOLE_SOURCES! "%BF2%\Geom\StaticMeshRendererConsole.cpp"
rem Roads step 3 (2026-10-01): the native host's EventNode shape for the HeightmapEventNode
rem RoadEditable::projectToTerrain posts.
set CONSOLE_SOURCES=!CONSOLE_SOURCES! "%BF2%\Geom\HeightmapEventNode.cpp"

set OK=0
set BAD=0
set /a OBJN=0
rem Objects are NUMBERED for the same reason build.cmd numbers its own: the output directory is flat,
rem and Geom\ModuleGeom.cpp meeting a future GeomMisc\ModuleGeom.cpp would silently overwrite one
rem object with the other and make a written body look unwritten at link time.
for %%F in (%SOURCES%) do (
    set /a OBJN+=1
    cl %FLAGS% /Fo"%OUT%\!OBJN!_%%~nF.obj" "%%F" >"%OUT%\cl_%%~nF.log" 2>&1
    if errorlevel 1 (
        set /a BAD+=1
        echo   COMPILE FAIL %%~nxF
        type "%OUT%\cl_%%~nF.log"
    ) else (
        set /a OK+=1
    )
)
rem Second loop continues the same OBJN so object numbering stays unique across both lists.
for %%F in (%CONSOLE_SOURCES%) do (
    set /a OBJN+=1
    cl %FLAGS% /Fo"%OUT%\!OBJN!_%%~nF.obj" "%%F" >"%OUT%\cl_%%~nF.log" 2>&1
    if errorlevel 1 (
        set /a BAD+=1
        echo   COMPILE FAIL %%~nxF
        type "%OUT%\cl_%%~nF.log"
    ) else (
        set /a OK+=1
    )
)
echo   %OK% compiled, %BAD% failed
if not "%BAD%"=="0" exit /b 1

rem ---------------------------------------------------------------------------
rem STEP 3 - link the DLL.
rem
rem /DEF gives the two undecorated exports and pins their ordinals to the shipped image's.
rem /BASE:0x10000000 matches the shipped image base. Cosmetic under ASLR, and kept because a
rem   side-by-side dumpbin of the two DLLs is a check this rebuild will want to keep running.
rem ---------------------------------------------------------------------------
echo [3/4] linking
link /nologo /DLL /MACHINE:X86 /BASE:0x10000000 /DEBUG ^
     /DEF:"%ABI%\RendDX9.def" ^
     /OUT:"%BIN%\RendDX9.dll" ^
     /PDB:"%BIN%\RendDX9.pdb" ^
     /IMPLIB:"%OUT%\RendDX9.lib" ^
     /MAP:"%OUT%\RendDX9.map" ^
     "%OUT%\*.obj" "%D3DX25_LIB%" "%OUT%\binkw32.lib" "%OUT%\TextureAtlasBuilder.lib" kernel32.lib user32.lib d3d9.lib > "%OUT%\link.log" 2>&1
if errorlevel 1 (
    echo   LINK FAILED - see "%OUT%\link.log"
    type "%OUT%\link.log"
    exit /b 1
)

rem ---------------------------------------------------------------------------
rem STEP 4 - the ABI checks that matter, run every build.
rem
rem The native image must carry both exports and no retail heap or CRT imports.
rem If either shape drifts, the DLL loads and then behaves wrongly rather than failing.
rem ---------------------------------------------------------------------------
echo [4/4] ABI check
dumpbin /nologo /exports "%BIN%\RendDX9.dll" > "%OUT%\exports.txt"
dumpbin /nologo /dependents "%BIN%\RendDX9.dll" > "%OUT%\dependents.txt"
dumpbin /nologo /imports:Memory.dll "%BIN%\RendDX9.dll" > "%OUT%\imports.txt"

rem ⚠ findstr, NOT find. `find` is SHADOWED when this script is launched from a shell that puts a
rem POSIX toolchain on PATH - Git Bash's find.exe takes the arguments as paths and reports the DLL's
rem imports as MISSING on a build that has them. That misfired once and is exactly the kind of check
rem that is worse than no check: it accused a correct DLL. findstr has no POSIX twin.
set ABI_BAD=0
findstr /c:"initDll"    "%OUT%\exports.txt"    >nul && findstr /c:"deinitDll" "%OUT%\exports.txt" >nul
if not errorlevel 1 (
    echo   exports:      initDll + deinitDll  OK
) else (
    echo   exports:      MISSING - the host resolves both by name
    set /a ABI_BAD+=1
)
findstr /i /c:"Memory.dll" /c:"msvcr71.dll" /c:"msvcp71.dll" "%OUT%\dependents.txt" >nul
if not errorlevel 1 (
    echo   imports:      retail heap/CRT UNEXPECTED in native ABI
    set /a ABI_BAD+=1
)
findstr /l /c:"?bf_malloc@memory@hfe@dice@@YAPAXIH@Z" "%OUT%\imports.txt" >nul
if not errorlevel 1 (
    echo   imports:      engine heap function UNEXPECTED in native ABI
    set /a ABI_BAD+=1
)
findstr /i /c:"VCRUNTIME140.dll" "%OUT%\dependents.txt" >nul
if errorlevel 1 (
    echo   imports:      native shared VCRUNTIME140.dll MISSING
    set /a ABI_BAD+=1
)
findstr /i /c:"api-ms-win-crt-heap-" /c:"ucrtbase.dll" "%OUT%\dependents.txt" >nul
if errorlevel 1 (
    echo   imports:      native shared CRT heap MISSING
    set /a ABI_BAD+=1
)
if not "!ABI_BAD!"=="0" (
    echo   NATIVE ABI CHECK FAILED - !ABI_BAD! invariant^(s^) violated
    exit /b 1
)

rem TextureAtlasBuilder.dll (2026-10-01): the shipped image imports exactly its two creates, by name.
rem A miss stops the script (a generator that cannot bind is a broken image).
dumpbin /nologo /imports:TextureAtlasBuilder.dll "%BIN%\RendDX9.dll" > "%OUT%\imports_tab.txt"
set TAB_BAD=0
findstr /l /c:"?create@ITextureAtlas@textureatlas@@SIPAV12@PAUIDirect3DDevice9@@@Z" "%OUT%\imports_tab.txt" >nul
if errorlevel 1 set TAB_BAD=1
findstr /l /c:"?create@ITextureAtlasConfiguration@textureatlas@@SIPAV12@XZ" "%OUT%\imports_tab.txt" >nul
if errorlevel 1 set TAB_BAD=1
if "%TAB_BAD%"=="0" (echo   imports:      TextureAtlasBuilder.dll  OK - its two creates, by name, as the shipped image) else (echo   imports:      TextureAtlasBuilder.dll creates MISSING - the atlas generator cannot bind)
if "%TAB_BAD%"=="1" exit /b 1

echo.
echo   built: %BIN%\RendDX9.dll
echo   NOTE: this is NOT installed anywhere. See docs\building.md before using this DLL with a client.
exit /b 0

:usage
echo Usage: renddx9.cmd [native]
exit /b 2
