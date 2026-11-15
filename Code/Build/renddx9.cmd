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
rem   usage:  renddx9.cmd [native]   rebuilt host ABI (/MD, no Memory.dll), x86
rem           renddx9.cmd x64        the native x64 client's RendDX9.dll (Bink: the FFmpeg-based port)
rem [CHOSEN, not recovered] Native-only builds: https://github.com/B2BF/OpenBF2/issues/7
rem ===========================================================================
if not "%~2"=="" goto :usage
set "ARCH=x86"
if /i "%~1"=="x64" set "ARCH=x64"
if not "%~1"=="" if /i not "%~1"=="native" if /i not "%~1"=="x64" goto :usage
call "%~dp0vsdevcmd.cmd" %ARCH%
if errorlevel 1 exit /b 1
rem The D3DX headers and import library come from the installed April 2005 DirectX SDK, the
rem generation whose d3dx9.lib binds d3dx9_25.dll as retail RendDX9 does. dxsdk.cmd refuses any
rem other SDK and appends it to INCLUDE/LIB after the Windows SDK.
rem [CHOSEN, not recovered] BF2_GFX_SEAM=ON (G2w): the renderer TUs compile against the subset headers,
rem but the generated forwarding TUs still need the SDK's d3d9.h/d3dx9.h and dxguid.lib.
rem [CHOSEN, not recovered] Without the SDK, dxsdk.cmd's fallback (BF2_D3DX, docs\building.md "D3DX
rem without the DirectX SDK") sets BF2_D3DX_FALLBACK: d3dx9_25.lib is generated from
rem RendDX9Abi\D3dx9_25.def and <d3dx9.h> resolves to RendDX9Abi\D3dxFallbackInclude.
call "%~dp0dxsdk.cmd" %ARCH% fallback
if errorlevel 1 exit /b 1
if defined BF2_D3DX_FALLBACK if /i "%BF2_GFX_SEAM%"=="ON" (
    echo renddx9.cmd: BF2_GFX_SEAM=ON needs the April 2005 SDK's headers; the D3DX fallback cannot build it.
    exit /b 1
)

for %%I in ("%~dp0..") do set CODE=%%~fI
set ABI=%CODE%\RendDX9Abi
set BF2=%CODE%\BF2
set OUT=%~dp0obj_renddx9_native
set BIN=%~dp0bin_renddx9_native
if "%ARCH%"=="x64" set OUT=%~dp0obj_renddx9_x64
if "%ARCH%"=="x64" set BIN=%~dp0bin_renddx9_x64
set CRT=/MD
set PLUGIN_ABI=/DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0
rem Agent/CI isolation keeps builds out of another contributor's directories.
if defined BF2_RENDDX9_OUT set OUT=%BF2_RENDDX9_OUT%
if defined BF2_RENDDX9_BIN set BIN=%BF2_RENDDX9_BIN%
rem [CHOSEN, not recovered] A seam build never overwrites the default build's objects or DLL.
if /i "%BF2_GFX_SEAM%"=="ON" if not defined BF2_RENDDX9_OUT set OUT=%OUT%_seam
if /i "%BF2_GFX_SEAM%"=="ON" if not defined BF2_RENDDX9_BIN set BIN=%BIN%_seam

set "D3DX25_LIB=%BF2_DXSDK_LIB%\d3dx9.lib"
if defined BF2_D3DX_FALLBACK set "D3DX25_LIB=%OUT%\d3dx9_25.lib"
rem [CHOSEN, not recovered] Seam build: the forwarding objects define Direct3DCreate9 and the D3DX functions
rem themselves and bind d3d9.dll / d3dx9_25.dll at run time, so neither import library is linked;
rem only the April 2005 SDK's dxguid.lib (the IID_ constants QueryInterface forwarding compares against).
set GFX_LIBS="%D3DX25_LIB%"
set GFX_LIBS_TAIL=d3d9.lib
rem [CHOSEN, not recovered] d3dx9_25.dll is delay-loaded, so a missing (or, for x64, 32-bit only) D3DX
rem runtime reaches BF2Render::init's message box instead of failing RendDX9.dll's load. The host
rem loads the DLL before this one (Game\Main\BF2Render.cpp); retail imports it directly.
set "D3DX_DELAY=/DELAYLOAD:d3dx9_25.dll delayimp.lib"
if /i "%BF2_GFX_SEAM%"=="ON" set GFX_LIBS="%BF2_DXSDK_LIB%\dxguid.lib"
if /i "%BF2_GFX_SEAM%"=="ON" set GFX_LIBS_TAIL=
if /i "%BF2_GFX_SEAM%"=="ON" set "D3DX_DELAY="

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
rem [CHOSEN, not recovered] BF2_FRONTIER_DEFAULT_CONTINUE=ON (CMake option of the same name, passed by
rem the CMake plugin targets; CI release builds set it): Frontier.cpp's trap defaults to continue when
rem BF2_RENDDX9_FRONTIER is unset. Anything else keeps the dev default, abort.
if /i "%BF2_FRONTIER_DEFAULT_CONTINUE%"=="ON" set FLAGS=%FLAGS% /DBF2_FRONTIER_DEFAULT_CONTINUE=1
rem x64: a pointer truncated into a 32-bit integer is an error, as for the x64 dedicated server.
if "%ARCH%"=="x64" set FLAGS=%FLAGS% /we4311 /we4312
if defined BF2_D3DX_FALLBACK set FLAGS=%FLAGS% /I"%ABI%\D3dxFallbackInclude"
rem Effect.cpp asks for the SDK's dxerr9.lib by #pragma; the fallback compiles DxErr9StandIn.cpp instead.
set "D3DX_LINK="
if defined BF2_D3DX_FALLBACK set "D3DX_LINK=/NODEFAULTLIB:dxerr9.lib"
rem [CHOSEN, not recovered] G2w: renderer TUs see the subset headers (FLAGS); the forwarding TUs do
rem not (FWD_FLAGS keeps the unmodified flags, so <d3d9.h> is the SDK's there).
set FWD_FLAGS=%FLAGS% /DBF2_GFX_FORWARD_TU=1 /I"%BF2%\Platform\Gfx\D3D9Forward"
if /i "%BF2_GFX_SEAM%"=="ON" set FLAGS=%FLAGS% /DBF2_GFX_SEAM=1 /I"%BF2%\Platform\Gfx\Include"

if not exist "%OUT%" mkdir "%OUT%"
if not exist "%BIN%" mkdir "%BIN%"
del /q "%OUT%\*.obj" >nul 2>&1
del /q "%OUT%\*.log" >nul 2>&1

rem ---------------------------------------------------------------------------
rem STEP 1 - import libraries.
rem
rem Build the Bink and TextureAtlasBuilder import libraries. D3DX links the DirectX SDK's own
rem d3dx9.lib (D3DX25_LIB above), or without the SDK the library generated here from D3dx9_25.def.
rem ---------------------------------------------------------------------------
set LIB_BAD=0
>"%OUT%\lib.log" echo [D3DX] DirectX SDK import library: "%D3DX25_LIB%"
if defined BF2_D3DX_FALLBACK (
    >"%OUT%\lib.log" echo [D3DX] no SDK: generating "%D3DX25_LIB%" from D3dx9_25.def
    lib /nologo /def:"%ABI%\D3dx9_25.def" /machine:%ARCH% /out:"%D3DX25_LIB%" >>"%OUT%\lib.log" 2>&1
    if errorlevel 1 set LIB_BAD=1
)
echo [1/4] import libraries - native ABI, no Memory.dll
rem binkw32.dll exists only as a 32-bit DLL. The x64 image has no Bink import: it links the port
rem (STEP 2, BINK_PORT_SOURCES; Code\BF2\External\ffmpeg-bink\README.md) instead.
set "BINK_LIB="
if "%ARCH%"=="x86" (
    lib /nologo /def:"%ABI%\binkw32.def" /machine:x86 /out:"%OUT%\binkw32.lib" >>"%OUT%\lib.log" 2>&1
    if errorlevel 1 set LIB_BAD=1
    set "BINK_LIB=%OUT%\binkw32.lib"
)
rem TextureAtlasBuilder.dll: RendDX9.dll 1.50 carries a STATIC import
rem descriptor for it with the two exported creates (RendDX9Abi\TextureAtlasBuilder.def), which
rem TextureAtlas\TextureAtlas.cpp's atlas generator calls. Static, as in retail: without the DLL on
rem the search path RendDX9.dll does not load. Stage the rebuilt atlas DLL beside the executable;
rem Code\Build\textureatlasbuilder.cmd builds it and CMake stages it with the client.
set "TAB_DEF=%ABI%\TextureAtlasBuilder.def"
if "%ARCH%"=="x64" set "TAB_DEF=%ABI%\TextureAtlasBuilder_x64.def"
lib /nologo /def:"%TAB_DEF%" /machine:%ARCH% /out:"%OUT%\TextureAtlasBuilder.lib" >>"%OUT%\lib.log" 2>&1
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
 "%BF2%\RendDX9\ResourceTracer.cpp" ^
 "%BF2%\RendDX9\VideoOptions.cpp" ^
 "%BF2%\RendDX9\Effect.cpp" ^
 "%BF2%\RendDX9\DiceEffect.cpp" ^
 "%BF2%\RendDX9\FXParameter.cpp" ^
 "%BF2%\RendDX9\ShaderManager.cpp" ^
 "%BF2%\RendDX9\StateManager.cpp" ^
 "%BF2%\RendDX9\RendDX9Draw.cpp" ^
 "%BF2%\RendDX9\MovieRenderer.cpp" ^
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
 "%BF2%\Core\Debug.cpp" ^
 "%BF2%\Core\StringUtil.cpp" ^
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
 "%BF2%\Geom\LightningManager.cpp" ^
 "%BF2%\Geom\WeatherManager.cpp" ^
 "%BF2%\Geom\Rain\RainManager.cpp" ^
 "%BF2%\Geom\UndergrowthSystemLoad.cpp" ^
 "%BF2%\Geom\UndergrowthSystemRender.cpp" ^
 "%BF2%\Geom\Terrain.cpp" ^
 "%BF2%\Geom\QuadPatch.cpp" ^
 "%BF2%\Geom\RaQuadPatchAdapter.cpp" ^
 "%BF2%\Geom\SurroundingPatch.cpp" ^
 "%BF2%\Geom\TerrainDraw.cpp" ^
 "%BF2%\Geom\TerrainEditable.cpp"
rem The list is split in two `set` lines (SOURCES, SOURCES2), each with its own loop below: expanded at
rem a checkout such as C:\rendbuild\src, one line holding all of it passed cmd.exe's 8191-character
rem limit, cmd dropped the whole assignment, no file of the list compiled, and the link lost initDll.
set SOURCES2=^
 "%BF2%\Geom\TerrainEditableGetSets.cpp" ^
 "%BF2%\Geom\QuadPatchEditable.cpp" ^
 "%BF2%\Geom\SurroundingPatchEditable.cpp" ^
 "%BF2%\Geom\TerrainLightSampling.cpp" ^
 "%BF2%\Geom\LightmapGeneration\LightmapGeneration.cpp" ^
 "%BF2%\Geom\LightmapGeneration\MeshLightSampling.cpp" ^
 "%BF2%\Geom\ObjectDrawer.cpp" ^
 "%BF2%\Geom\XpLightManager.cpp" ^
 "%BF2%\Geom\LightMerger.cpp" ^
 "%BF2%\Geom\LightGeometry.cpp" ^
 "%BF2%\Geom\MeshOptimizer.cpp" ^
 "%BF2%\Geom\BundledMeshOptimizer.cpp" ^
 "%BF2%\Geom\StaticMeshOptimizer.cpp" ^
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
 "%BF2%\Geom\DecalSystem\DecalShadowManager.cpp" ^
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
 "%BF2%\GeomMisc\Debug.cpp" ^
 "%BF2%\GeomMisc\DebugBoxMesh.cpp" ^
 "%BF2%\GeomMisc\DebugCylinderMesh.cpp" ^
 "%BF2%\GeomMisc\DebugSphereMesh.cpp" ^
 "%BF2%\GeomMisc\DebugPlaneMesh.cpp" ^
 "%BF2%\GeomMisc\DebugPolygonMesh.cpp" ^
 "%BF2%\GeomMisc\DebugTorusMesh.cpp" ^
 "%BF2%\GeomMisc\DebugXMesh.cpp" ^
 "%BF2%\GeomMisc\DebugRenderer.cpp" ^
 "%BF2%\GeomMisc\DebugTextDrawer.cpp" ^
 "%BF2%\GeomMisc\DebugCircleDrawer.cpp" ^
 "%BF2%\GeomMisc\DebugLineDrawer.cpp" ^
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
 "%BF2%\GeomMisc\MenuRend\QuadDrawer.cpp" ^
 "%BF2%\GeomMisc\MenuRend\QuadOutputCache.cpp" ^
 "%BF2%\GeomMisc\DebugBlockGraphD3D.cpp" ^
 "%BF2%\GeomMisc\LoadingScreen\LoadingScreen.cpp"

rem These sources use the rebuilt host's native std::string/list/map ABI.
rem Keep them in a separate list and loop: appending them to SOURCES made that one `set` line
rem exceed cmd.exe's 8191-character limit at deeper checkout paths, which silently dropped the
rem whole assignment and left every ConsoleObject symbol unresolved at link time.
set CONSOLE_SOURCES= "%BF2%\Core\ConsoleObjectBase.cpp" "%ABI%\NativeConsoleObjects.cpp" "%BF2%\Geom\LightManagerConsole.cpp" "%BF2%\Geom\HemiMapManager\HemiMapManagerConsole.cpp" "%BF2%\Geom\SkyDomeConsole.cpp" "%BF2%\Geom\TerrainConsole.cpp" "%BF2%\RendDX9\RendererConsole.cpp" "%BF2%\Geom\NametagManagerConsole.cpp" "%BF2%\Geom\Overgrowth\OverGrowthConsole.cpp" "%BF2%\Geom\UndergrowthSystemConsole.cpp" "%BF2%\Geom\DecalSystem\DecalManagerConsole.cpp" "%BF2%\Geom\WeatherManagerConsole.cpp"
rem RD1 (2026-09-30): the `Object.geometry.loadMesh` row - native console ABI, like the list above.
set CONSOLE_SOURCES=!CONSOLE_SOURCES! "%BF2%\Geom\ObjectGeometryConsole.cpp"
rem The 2006 editor's `lightmapGeneration` rows - native console ABI; the TU is empty unless
rem BF2_IMAGE_EDITOR (the BF2_RENDDX9_EDITOR=1 flavour) is set.
set CONSOLE_SOURCES=!CONSOLE_SOURCES! "%BF2%\Geom\LightmapGeneration\LightmapGenerationConsole.cpp"
rem The StaticMeshRenderer atlas rows (loadAllLightMapFiles / loadAllLightMapFilesDLL) - native console ABI.
set CONSOLE_SOURCES=!CONSOLE_SOURCES! "%BF2%\Geom\StaticMeshRendererConsole.cpp"
set CONSOLE_SOURCES=!CONSOLE_SOURCES! "%BF2%\Geom\TerrainCullerConsole.cpp"
rem Roads step 3 (2026-10-01): the native host's EventNode shape for the HeightmapEventNode
rem RoadEditable::projectToTerrain posts.
set CONSOLE_SOURCES=!CONSOLE_SOURCES! "%BF2%\Geom\HeightmapEventNode.cpp"
rem [CHOSEN, not recovered] D3DX fallback: DXGetErrorDescription9A without the SDK's DxErr9.lib.
if defined BF2_D3DX_FALLBACK set CONSOLE_SOURCES=!CONSOLE_SOURCES! "%ABI%\DxErr9StandIn.cpp"

rem [CHOSEN, not recovered] x64 only: the Bink stand-in (owner decision 2026-11-07, Code\BF2\External\ffmpeg-bink\README.md).
rem The C++ part compiles with FLAGS; FFmpeg 7.1.1's Bink decoders (LGPL-2.1+, Code\BF2\External\
rem ffmpeg-bink) compile as C11, optimized in every configuration as FFmpeg is. x86 keeps binkw32.dll.
set "BINK_PORT_SOURCES="
set "FFBINK_SOURCES="
if "%ARCH%"=="x64" (
    set "FFB=%BF2%\External\ffmpeg-bink"
    set BINK_PORT_SOURCES="%ABI%\BinkFFmpeg.cpp" "%ABI%\BinkAudioDirectSound.cpp"
    set FFBINK_SOURCES="!FFB!\libavcodec\bink.c" "!FFB!\libavcodec\binkdsp.c" "!FFB!\libavcodec\binkaudio.c" "!FFB!\libavcodec\wma_freqs.c" "!FFB!\shim\ffbink_shim.c"
)
set FFBINK_FLAGS=/nologo /c %CRT% /std:c11 /O2 /W3 /wd4244 /wd4267 /wd4018 /wd4146 /D_CRT_SECURE_NO_WARNINGS ^
 /I"%BF2%\External\ffmpeg-bink\shim" /Zi /FS /Fd"%OUT%\RendDX9.pdb"

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
for %%F in (%SOURCES2%) do (
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
for %%F in (%BINK_PORT_SOURCES%) do (
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
for %%F in (%FFBINK_SOURCES%) do (
    set /a OBJN+=1
    cl %FFBINK_FLAGS% /Fo"%OUT%\!OBJN!_%%~nF.obj" "%%F" >"%OUT%\cl_%%~nF.log" 2>&1
    if errorlevel 1 (
        set /a BAD+=1
        echo   COMPILE FAIL %%~nxF
        type "%OUT%\cl_%%~nF.log"
    ) else (
        set /a OK+=1
    )
)
rem [CHOSEN, not recovered] G2w forwarding backend. Generated by
rem tools\macos\gen_d3d9_forward.py and committed; compiled against the SDK, not the subset.
if /i "%BF2_GFX_SEAM%"=="ON" (
    set "FWD=%BF2%\Platform\Gfx\D3D9Forward"
    for %%F in ("!FWD!\Generated\ForwardWrappers.cpp" "!FWD!\Generated\ForwardFunctions.cpp" "!FWD!\ForwardLoader.cpp") do (
        set /a OBJN+=1
        cl !FWD_FLAGS! /Fo"%OUT%\!OBJN!_%%~nF.obj" "%%F" >"%OUT%\cl_%%~nF.log" 2>&1
        if errorlevel 1 (
            set /a BAD+=1
            echo   COMPILE FAIL %%~nxF
            type "%OUT%\cl_%%~nF.log"
        ) else (
            set /a OK+=1
        )
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
rem /BASE:0x10000000 is the shipped x86 image base; the x64 image takes the linker's default.
set "LINK_BASE=/BASE:0x10000000"
if "%ARCH%"=="x64" set "LINK_BASE="
link /nologo /DLL /MACHINE:%ARCH% %LINK_BASE% /DEBUG %D3DX_LINK% ^
     /DEF:"%ABI%\RendDX9.def" ^
     /OUT:"%BIN%\RendDX9.dll" ^
     /PDB:"%BIN%\RendDX9.pdb" ^
     /IMPLIB:"%OUT%\RendDX9.lib" ^
     /MAP:"%OUT%\RendDX9.map" ^
     "%OUT%\*.obj" %GFX_LIBS% %BINK_LIB% "%OUT%\TextureAtlasBuilder.lib" kernel32.lib user32.lib gdi32.lib %GFX_LIBS_TAIL% %D3DX_DELAY% > "%OUT%\link.log" 2>&1
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
rem Retail RendDX9 imports d3dx9_25.dll. A different D3DX DLL here means the link took another
rem SDK's d3dx9.lib, with a different effect compiler behind it.
findstr /i /c:"d3dx9_25.dll" "%OUT%\dependents.txt" >nul
if /i "%BF2_GFX_SEAM%"=="ON" (
    echo   imports:      d3dx9_25.dll bound at run time by the forwarding backend ^(not a static import^)
) else if errorlevel 1 (
    echo   imports:      d3dx9_25.dll MISSING - D3DX must come from the April 2005 SDK
    set /a ABI_BAD+=1
)
if defined D3DX_DELAY (
    findstr /i /c:"delay load dependencies" "%OUT%\dependents.txt" >nul
    if errorlevel 1 (
        echo   imports:      d3dx9_25.dll NOT delay-loaded
        set /a ABI_BAD+=1
    ) else (
        echo   imports:      d3dx9_25.dll delay-loaded
    )
)
rem [CHOSEN, not recovered] The D3DX fallback's import names must be d3dx9_25.dll's plain export names.
rem A decorated name (Name@bytes) links cleanly and then fails to load with ERROR_PROC_NOT_FOUND.
if defined BF2_D3DX_FALLBACK (
    dumpbin /nologo /imports:d3dx9_25.dll "%BIN%\RendDX9.dll" > "%OUT%\imports_d3dx.txt"
    findstr /r /c:"D3DX[A-Za-z0-9_]*@" "%OUT%\imports_d3dx.txt" >nul
    if not errorlevel 1 (
        echo   imports:      d3dx9_25.dll import names DECORATED - see "%OUT%\imports_d3dx.txt"
        set /a ABI_BAD+=1
    ) else (
        echo   imports:      d3dx9_25.dll by plain name ^(D3DX fallback^)
    )
)
if not "!ABI_BAD!"=="0" (
    echo   NATIVE ABI CHECK FAILED - !ABI_BAD! invariant^(s^) violated
    exit /b 1
)

rem TextureAtlasBuilder.dll (2026-10-01): the shipped image imports exactly its two creates, by name.
rem A miss stops the script (a generator that cannot bind is a broken image).
dumpbin /nologo /imports:TextureAtlasBuilder.dll "%BIN%\RendDX9.dll" > "%OUT%\imports_tab.txt"
set TAB_BAD=0
set "TAB_CREATE=?create@ITextureAtlas@textureatlas@@SIPAV12@PAUIDirect3DDevice9@@@Z"
set "TAB_CONFIG=?create@ITextureAtlasConfiguration@textureatlas@@SIPAV12@XZ"
if "%ARCH%"=="x64" set "TAB_CREATE=?create@ITextureAtlas@textureatlas@@SAPEAV12@PEAUIDirect3DDevice9@@@Z"
if "%ARCH%"=="x64" set "TAB_CONFIG=?create@ITextureAtlasConfiguration@textureatlas@@SAPEAV12@XZ"
findstr /l /c:"%TAB_CREATE%" "%OUT%\imports_tab.txt" >nul
if errorlevel 1 set TAB_BAD=1
findstr /l /c:"%TAB_CONFIG%" "%OUT%\imports_tab.txt" >nul
if errorlevel 1 set TAB_BAD=1
if "%TAB_BAD%"=="0" (echo   imports:      TextureAtlasBuilder.dll  OK - its two creates, by name, as the shipped image) else (echo   imports:      TextureAtlasBuilder.dll creates MISSING - the atlas generator cannot bind)
if "%TAB_BAD%"=="1" exit /b 1

echo.
echo   built: %BIN%\RendDX9.dll
echo   NOTE: this is NOT installed anywhere. See docs\building.md before using this DLL with a client.
exit /b 0

:usage
echo Usage: renddx9.cmd [native^|x64]
exit /b 2
