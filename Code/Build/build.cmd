@echo off
setlocal EnableDelayedExpansion
rem ===========================================================================
rem THE REAL BUILD - generates code and LINKS. Not a syntax check.
rem
rem   Code\Tests\syntaxcheck.cmd   parses only (/Zs). It CANNOT see a missing
rem                                 definition, an undefined static, or a
rem                                 declared-but-unwritten member.
rem   Code\Build\build.cmd         THIS. Compiles every recovered .cpp for coverage,
rem                                 links the engine-image manifest, and REPORTS THE GAPS.
rem
rem The first real compile of BitStream.cpp produced SIX unresolved externals
rem that /Zs had been passing for weeks: setBitSize, clear, and all four static
rem members. Every one was a genuine hole. All six are now filled.
rem
rem ** AN UNRESOLVED EXTERNAL IS A FUNCTION DECLARED FROM THE BINARY BUT NOT
rem    YET WRITTEN. The link error list IS the to-do list. **
rem
rem   usage:  build.cmd            compile all, link, report the gap count
rem           build.cmd <file>     one translation unit
rem ===========================================================================
call "%~dp0vsdevcmd.cmd" x86
for %%I in ("%~dp0..\BF2") do set ROOT=%%~fI
set OUT=%~dp0obj
rem Optional isolated-output seam for concurrent reconstruction agents.  The default remains the
rem historical shared obj directory, but a caller can set BF2_BUILD_OUT to prevent another full
rem build from cleaning or rewriting its objects and linker census mid-run.
if defined BF2_BUILD_OUT set OUT=%BF2_BUILD_OUT%
rem The CPython 2.3.4 headers from External/. Harmless for the files that do not include Python.h,
rem and required by Game\Python\. Py_NO_ENABLE_SHARED must match how the library is built - see
rem Code\Build\python\build_python.cmd for why both sides need it.
rem Py_NO_ENABLE_SHARED is NOT defined (2026-08-18): dice_py is a real DLL, so PyAPI_FUNC must
rem resolve to __declspec(dllimport) here. /MD below for the same split - ONE CRT per image,
rem shared with the DLL. See Code\Build\python\build_python.cmd.
set PYINC=/I"%~dp0..\BF2\External\python-embedded\py-src-2.3.4\Include" /I"%~dp0..\BF2\External\python-embedded\py-src-2.3.4\PC"
rem /Zi added 2026-07-30: the boot harness now resolves faults to FILE:LINE via DbgHelp instead of
rem hand-decoding RVAs against bf2_w32ded.map. /Zi alone is not enough - bootrun.cmd must link /DEBUG.
rem Objects go to a shared obj dir, so /FS serialises PDB writes.
rem ZIPINC added 2026-08-14: IO/File/ZipArchive.{h,cpp} wrap the VENDORED stock zlib 1.2.3 +
rem minizip 1.01e at External\zlib-1.2.3 (the exact versions the ELF links - unzip.h says 1.01e).
set ZIPINC=/I"%ROOT%\External\zlib-1.2.3" /I"%ROOT%\External\zlib-1.2.3\contrib\minizip"
rem /DDEDICATED=1 + /FI Core\BuildConfig.h, 2026-09-02: THE IMAGE DEFINE. bf2_w32ded.exe is the dedicated
rem configuration of a source that also builds BF2.exe. Client-only
rem regions of shared code sit under #if !DEDICATED, and the force-included header refuses to compile when
rem the define is missing, so no compile line can silently build client code into the server image.
set FLAGS=/nologo /c /MD /std:c++14 /permissive /EHsc /W3 /wd4996 /Zi /FS /Fd"%OUT%\bf2_w32ded.pdb" %PYINC% %ZIPINC% /DDEDICATED=1 /FI"%ROOT%\Core\BuildConfig.h"
if not exist "%OUT%" mkdir "%OUT%"

rem NOTE: !errorlevel!, NOT %errorlevel%. Inside a parenthesised block %VAR% expands when the
rem block is PARSED - before cl runs - so exit /b %errorlevel% always returned a stale value.
rem That produced a bogus "48/48 compile" reading before it was caught.
rem NOTE (2026-08-20): the single-file path is a COMPILE CHECK and nothing more - it writes one
rem throwaway object and deletes it. It used to leave <name>.obj in %OUT%, which the link steps
rem glob; now that the full build NUMBERS its objects, a leftover unnumbered copy would be a
rem SECOND definition of the same translation unit at link time. Re-run the full build to relink.
if not "%~1"=="" (
    cl %FLAGS% /Fo"%OUT%\_check.obj" "%~1"
    set RC=!errorlevel!
    del /q "%OUT%\_check.obj" >nul 2>&1
    exit /b !RC!
)

rem âš  CLEAN FIRST. Added 2026-07-27 after this script was found reporting 108 object files for 55
rem source files: the obj directory had never been emptied, so objects from files that were since
rem RENAMED OR MOVED (ObjectTemplateManager's path change, among others) were still being linked.
rem They contributed their own unresolved externals, which means every gap count this script printed
rem before today was inflated by an unknown amount. The counts are only comparable from here on.
rem
rem The full-build path cleans; the single-file path above deliberately does not, so that
rem `build.cmd <one file>` stays fast.
del /q "%OUT%\*.obj" >nul 2>&1
del /q "%OUT%\*.log" >nul 2>&1
set ENGINE_RSP=%OUT%\engine_objects.rsp
type nul > "%ENGINE_RSP%"

rem *** OBJECT NAMES ARE NUMBERED, AND THAT IS A BUG FIX, NOT A STYLE CHOICE (2026-08-20). ***
rem
rem This loop used to compile every source into %OUT% under its own BASENAME. The obj directory
rem is FLAT, so two sources with the same file name in different directories produced the same
rem .obj and the second SILENTLY OVERWROTE THE FIRST. No warning, no error - the losing
rem translation unit simply vanished from the link, and every symbol it defined turned up as an
rem "unresolved external", i.e. as a function that looked UNWRITTEN.
rem
rem That is exactly what happened the day Animation\BoneAnimation\Bundle.cpp was added beside
rem the long-standing Game\Objects\Base\Bundle.cpp: a fully written BundleManager compiled
rem clean, the script reported 255/0, and the drive then faulted at RVA 0. The link log named
rem the symbols; nothing named the cause. Numbering the objects makes the collision impossible.
rem
rem The counter is per-build and the directory is cleaned above, so the names are stable within
rem a run and meaningless between runs. The engine response manifest records those generated names
rem while excluding source that belongs only to AIDLL_w32ded.dll.
set OK=0
set BAD=0
set /a OBJN=0
rem CLIENT-ONLY IMAGE MANIFEST, 2026-09-02: a translation unit that exists only in BF2.exe is excluded from
rem the dedicated image inside the loop below, one line per file exactly like the seven AIDLL units, and is
rem listed in CMakeLists.txt as bf2_client_only_sources. The first: Game\Main\BF2Window.cpp, 2026-09-02.
for /r "%ROOT%" %%F in (*.cpp) do (
    echo %%F | find /i "\External\" >nul
    if errorlevel 1 (
        set /a OBJN+=1
        cl %FLAGS% /Fo"%OUT%\!OBJN!_%%~nF.obj" "%%F" >nul 2>&1
        if errorlevel 1 (
            set /a BAD+=1
            echo   COMPILE FAIL %%~nxF
        ) else (
            set /a OK+=1
            rem The repository contains source for more than one shipped image.  Compile every TU
            rem for coverage, but never link the seven AIDLL-only units into bf2_w32ded.exe.
            set "ENGINE_OBJECT=1"
            rem Code\BF2\VoipServer is a THIRD server-side image (BF2VoipServer_w32ded.dll - see
            rem bf2_voipserver in CMakeLists.txt). No unit in it is an engine member, so the whole
            rem directory is excluded here; CMakeLists.txt mirrors this with bf2_voipserver_only_sources.
            echo %%F | find /i "\BF2\VoipServer\" >nul
            if not errorlevel 1 set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Core\DllModule.cpp" set "ENGINE_OBJECT=0"
            rem The side-DLL image owns its globals translation unit.
            rem It defines the nineteen names initDll fills, zero-initialised; Core\Globals.cpp is
            rem the engine's definition site for most of them and the two would collide here.
            if /i "%%~fF"=="%ROOT%\Core\DllGlobals.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Math\Mat4Inverse.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BetweenRoundsLogic.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManagerConfiguration.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManagerState.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManagerChatInput.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManagerCamera.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManagerOrders.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManagerRadio.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManagerAbilities.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManagerLifecycle.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManagerUpdate.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManagerPointer.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManagerPlayerFilter.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManagerGameFilter.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManagerInit.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManagerEvents.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManagerHudEvents.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManagerGameLogicEvents.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\HudAwardCatalogue.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BfOccupiedVehicleData.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManagerSpotted.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManagerSpottedTarget.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManagerCommander.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManagerLists.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManagerProjection.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BF2HudManagerInput.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\HudScoreboard.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\Bf2MenuMapNode.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\Bf2HudBuilder.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\Bf2SplitNode.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\HudLoadingScreen.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\HudRadio.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\HudChat.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\HudColors.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\HudKitSelection.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\HudQuestionManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\HudVoteMap.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\HudMessages.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\HudCommander.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\HudSquad.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\HudAmmo.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\HudVehicleInformation.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BfEditData.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BfEditNode.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BfNewListBoxNode.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\BfListBoxData.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\HudBlinker.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\Ref2System.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\Font.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\DifFontMetadata.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\FontLoader.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\DifFontMetadataFile.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\HudInformationAdapter.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Game\HudInformationLayer.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\IO\Input\KeyboardDevice.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\IO\Input\MouseDevice.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\IO\Input\GameControllerDevice.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\IO\Input\FalconDevice.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\IO\Input\DX8InputDeviceManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIBot\BotMain.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIBot\BotNavigation.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIBot\BotSquad.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIBot\SensingAgent.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIBot\CommunicationAgent.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIBot\BotSenses.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIPlanInterpreter\BAPContext.cpp" set "ENGINE_OBJECT=0"
            rem 2026-09-08, chunk 02: both managers are libaidll-only (CLASS_CENSUS_LIBAIDLL.tsv rows
            rem WeaponInfoManager 0x36CF40 and the fully inlined InformationManager); neither has a
            rem counterpart in bf2. Core\SpHandlePool.h is a header-only template and needs no row.
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIKnowledge\InformationManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIKnowledge\Information.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIKnowledge\WeaponInfo.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIKnowledge\IPIArmament.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIKnowledge\IPIArmamentReal.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIKnowledge\IPICover.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIKnowledge\IPIMobile.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIKnowledge\IPIMobileReal.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIKnowledge\IPIMobileKnown.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIKnowledge\IPIUnit.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIKnowledge\InformationReal.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIKnowledge\WeaponInfoReal.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIKnowledge\IPIPhysicalReal.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIUtils\VehicleOccupancy.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIKnowledge\BFInformationEnvironment.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIKnowledge\AIInformationGrid.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIUtils\DebugRendering\AIRenderUtil.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIKnowledge\WeaponInfoManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIKnowledge\BFInformationCourier.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIKnowledge\InformationFactory.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIStrategicMap\BFStrategicEnvironment.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIStrategicMap\BotSpawner.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\DrawPrimitives\DPManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\Main\AICollisionHandler.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\Main\AIEventCourier.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\Main\AIMineManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\Main\AIPlayerCourier.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\Main\BFPathfindingCourier.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIPathFinding\Material\MaterialManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIPathFinding\NavMesh\MapProperties.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIPathFinding\NavMesh\QuadTreeManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIPathFinding\NavMesh\QuadTree.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIPathFinding\NavMesh\QuadTreeNode.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIPathFinding\Cluster\ClusterGraph.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIPathFinding\Cluster\ClusterNodeManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIPathFinding\NavMesh\QTSearchNodeManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIStrategicMap\AerialHeightMap.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIPlanInterpreter\BAPEntries.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIBot\SensingAgentPatterns.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIBot\SquadManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIBot\SquadCoordinator.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIBot\SquadFormation.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIBot\SquadMember.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIBotBehaviour\CustomCurve.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIBot\BotMemory.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIBot\BotSubObjects.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIBotBehaviour\UrgencyMerger.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\Main\AIConsole.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIBotBehaviour\UrgeCurve.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIStrategicEntity\CoverPosition\CPNodeManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIStrategicEntity\CoverPosition\CoverPositionManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIStrategicEntity\AIStrategicObjectManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIStrategicEntity\AILandingZoneManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIStrategicEntity\AIStrategicObject.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIStrategicEntity\AIStrategicArea.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIStrategicEntity\SAWayPoint.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\AIStrategic\SAI.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\Main\BotBehaviourManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\Main\CommunicationHub.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\Main\StrategyManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\Main\AIPathfinding.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\Main\IAIPathfinding.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\Main\UserOrderManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\Main\AIMain.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\Main\AIModule.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\Main\AIRandom.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\Main\AILODManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\Main\IBotManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\Main\AISettings.cpp" set "ENGINE_OBJECT=0"
            rem 2026-09-08: AISingletonServer exists ONLY in libaidll.so (0x2EAC60); the engine has
            rem BFSingletonServer instead. CommonSingletonServer.cpp keeps the half that is in BOTH
            rem images and stays in this link. See that file's header for the census rows.
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\Main\AISingletonServer.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\Main\BF2AI.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\AI\AIDLL\AI\Main\BotManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Game\Main\BF2Window.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Game\Main\BF2FrameInterpolator.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Game\Main\BF2Render.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Game\Main\BF2RenderConsole.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Game\SettingsConsole.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Sound\BfSound\MusicPlayer.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\MenuTeamManager.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\MenuTeamManagerData.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\MenuTeamManagerConsole.cpp" set "ENGINE_OBJECT=0"
            if /i "%%~fF"=="%ROOT%\Menu\SwiffHostConsole.cpp" set "ENGINE_OBJECT=0"
            if "!ENGINE_OBJECT!"=="1" echo "%OUT%\!OBJN!_%%~nF.obj">>"%ENGINE_RSP%"
        )
    )
)
rem Third-party C (2026-08-14): the vendored zlib inflate half + EndOfRoundInfo's bounded
rem deflate/trees/compress closure + minizip reader. Stock library, same rule as libdice_py - link it,
rem do not reimplement it. Compiled with
rem C-appropriate flags (no /std:c++14, /EHsc, or PYINC); the *.cpp walk above keeps excluding
rem External\. zip.c (minizip's writer) is linked-but-unreferenced in the ELF and stays unvendored
rem until something recovered calls it.
set CFLAGS=/nologo /c /MD /W3 /wd4996 /Zi /FS /Fd"%OUT%\bf2_w32ded.pdb" %ZIPINC% /DDEDICATED=1 /FI"%ROOT%\Core\BuildConfig.h"
for %%F in ("%ROOT%\External\zlib-1.2.3\adler32.c" "%ROOT%\External\zlib-1.2.3\crc32.c"
            "%ROOT%\External\zlib-1.2.3\inffast.c" "%ROOT%\External\zlib-1.2.3\inflate.c"
            "%ROOT%\External\zlib-1.2.3\inftrees.c" "%ROOT%\External\zlib-1.2.3\zutil.c"
            "%ROOT%\External\zlib-1.2.3\deflate.c" "%ROOT%\External\zlib-1.2.3\trees.c"
            "%ROOT%\External\zlib-1.2.3\compress.c"
            "%ROOT%\External\zlib-1.2.3\uncompr.c"
            "%ROOT%\External\zlib-1.2.3\contrib\minizip\ioapi.c"
            "%ROOT%\External\zlib-1.2.3\contrib\minizip\unzip.c") do (
    cl %CFLAGS% /Fo"%OUT%\\" "%%~F" >nul 2>&1
    if errorlevel 1 (
        set /a BAD+=1
        echo   COMPILE FAIL %%~nxF
    ) else (
        set /a OK+=1
        echo "%OUT%\%%~nF.obj">>"%ENGINE_RSP%"
    )
)

rem Third-party C (2026-08-24): the GameSpy gcdkey SDK, for the CD-key sub-tranche. Stock library,
rem same rule as zlib/libdice_py - LINK the upstream source, do not reverse it (AGENTS.md). gcdkeys.c
rem defines gcd_getkeyhash (GameServer::challengeResponse ban check) and gcd_authenticate_user
rem (connectPlayer internet branch). On a LAN server (sv.internet 0) neither GameSpy availability/HTTP
rem nor qr2 query-reporting executes: connectPlayer takes the local statusCallback branch and
rem gcd_getkeyhash scans an empty product list -> "". So the deferred master-server modules (ghttp,
rem qr2 + ghttp ARE compiled now (2026-08-24, the real GameSpy backend). Availability HTTP + query
rem the LAN path. The bounded generic common below IS linked so gcdkeys resolves what the LAN path touches.
set GSDIR=%ROOT%\External\GameSpy
set CFLAGS=/nologo /c /MD /W3 /wd4996 /Zi /FS /Fd"%OUT%\bf2_w32ded.pdb" /DDEDICATED=1 /FI"%ROOT%\Core\BuildConfig.h"
for %%F in ("%GSDIR%\gcdkey\gcdkeys.c" "%GSDIR%\darray.c"
            "%GSDIR%\common\gsAvailable.c" "%GSDIR%\common\gsPlatformUtil.c"
            "%GSDIR%\common\gsCore.c" "%GSDIR%\common\gsMemory.c"
            "%GSDIR%\common\gsDebug.c" "%GSDIR%\common\gsAssert.c"
            "%GSDIR%\common\gsPlatform.c" "%GSDIR%\common\gsPlatformSocket.c"
            "%GSDIR%\common\gsCrypt.c" "%GSDIR%\common\gsRC4.c"
            "%GSDIR%\common\gsSHA1.c" "%GSDIR%\common\gsLargeInt.c"
            "%GSDIR%\common\gsPlatformThread.c"
            "%GSDIR%\common\gsXML.c" "%GSDIR%\common\gsSSL.c"
            "%GSDIR%\common\gsStringUtil.c" "%GSDIR%\md5c.c"
            "%GSDIR%\gstats\gstats.c" "%GSDIR%\gstats\gbucket.c"
            "%GSDIR%\qr2\qr2.c" "%GSDIR%\qr2\qr2regkeys.c" "%GSDIR%\hashtable.c"
            "%GSDIR%\ghttp\ghttpBuffer.c" "%GSDIR%\ghttp\ghttpCallbacks.c"
            "%GSDIR%\ghttp\ghttpCommon.c" "%GSDIR%\ghttp\ghttpConnection.c"
            "%GSDIR%\ghttp\ghttpEncryption.c" "%GSDIR%\ghttp\ghttpMain.c"
            "%GSDIR%\ghttp\ghttpPost.c" "%GSDIR%\ghttp\ghttpProcess.c"
            "%GSDIR%\natneg\natneg.c" "%GSDIR%\natneg\NATify.c") do (
    cl %CFLAGS% /Fo"%OUT%\\" "%%~F" >nul 2>&1
    if errorlevel 1 (
        set /a BAD+=1
        echo   COMPILE FAIL %%~nxF
    ) else (
        set /a OK+=1
        echo "%OUT%\%%~nF.obj">>"%ENGINE_RSP%"
    )
)

rem Third-party C++ (2026-08-21): TinyXML, which dice::hfe::XMLDocument wraps and which
rem ModInfo's two .desc readers reach through it. Stock library, same rule as zlib and libdice_py -
rem link it, do not reimplement it. DICE's own build paths are baked into the ELF as __FILE__
rem strings (External/TinyXML/tinyxml.cpp, tinyxml.h, tinyxmlparser.cpp), so this IS where it lived.
rem
rem ⚠ TIXML_USE_STL IS DELIBERATELY NOT DEFINED. The binary uses TiXmlString/StringToBuffer rather
rem than std::string, which is the non-STL build; defining it changes every signature and the
rem recovered wrapper stops matching.
rem
rem C++ flags, not the CFLAGS block below/above - these are .cpp. tinyxmlerror.cpp carries the error
rem string table that tinyxmlparser.cpp references.
for %%F in ("%ROOT%\External\TinyXML\tinystr.cpp" "%ROOT%\External\TinyXML\tinyxml.cpp"
            "%ROOT%\External\TinyXML\tinyxmlerror.cpp" "%ROOT%\External\TinyXML\tinyxmlparser.cpp") do (
    cl %FLAGS% /Fo"%OUT%\\" "%%~F" >nul 2>&1
    if errorlevel 1 (
        set /a BAD+=1
        echo   COMPILE FAIL %%~nxF
    ) else (
        set /a OK+=1
        echo "%OUT%\%%~nF.obj">>"%ENGINE_RSP%"
    )
)

echo compiled: !OK!   failed: !BAD!

rem The link is EXPECTED to fail while gaps remain. Its output IS the report.
rem âš  python23.lib is DELIBERATELY NOT ON THIS LINE, and it was tried.
rem
rem Putting it here took the gap count from 394 to 586. The reason is that this link is a DIAGNOSTIC,
rem not a real link: it is expected to fail, and its output is the to-do list. Feeding a 3.3 MB
rem external library into a link that cannot yet succeed drags in every object pythoncore needs for
rem its own internals and reports all of those as gaps too - drowning the engine's own signal in
rem another project's.
rem
rem The four CPython entry points Game\Python\ uses (PyTuple_New, PyTuple_SetItem, PyInt_FromLong,
rem PyFloat_FromDouble) therefore stay visible as gaps here. They ARE satisfied by
rem External\python-embedded\OBJ\python\release\dice_py.lib - build_python.cmd links and runs a
rem program against it - so they are known-good, not unknown.
rem
rem Add the library here on the day the engine's own count reaches zero, not before.
rem /NODEFAULTLIB:python23.lib - PC\pyconfig.h emits `#pragma comment(lib,"python23.lib")` into
rem every TU that does not define Py_BUILD_CORE, so once dice_py became a real DLL (2026-08-18)
rem THIS DIAGNOSTIC LINK started dying on LNK1104 before it counted a single gap. The report is a
rem measurement tool - it has to survive.
link /nologo /NODEFAULTLIB:python23.lib /out:"%OUT%\bf2.exe" /subsystem:console @"%ENGINE_RSP%" Ws2_32.lib user32.lib > "%OUT%\link.log" 2>&1
find /c "unresolved external symbol" < "%OUT%\link.log" > "%OUT%\count.txt"
set /p NRAW=<"%OUT%\count.txt"
set "NFINAL=0"
set "NSUMMARY="
for /f "delims=" %%L in ('findstr /c:"fatal error LNK1120:" "%OUT%\link.log"') do set "NSUMMARY=%%L"
if defined NSUMMARY for /f "tokens=1" %%N in ("!NSUMMARY:*LNK1120:=!") do set "NFINAL=%%N"
echo unresolved diagnostics: !NRAW!
echo unique unresolved externals: !NFINAL!
echo   full list: %OUT%\link.log

rem Factory-free diagnostic. ClassFactoryInstantiations.cpp deliberately exposes many incomplete
rem vtables at once, so the main link report greatly amplifies the number of independent gaps. This
rem second link omits only that scaffolding object: real ModuleRegistry factory references remain and
rem therefore stay visible. A response file avoids cmd.exe's command-line length limit.
> "%OUT%\factory_free.rsp" (
    rem NOTE: a SUBSTRING match, not equality - objects carry a numeric prefix now (see the
    rem compile loop above), so "N_ClassFactoryInstantiations.obj" would slip past an == test.
    for /f "usebackq delims=" %%F in ("%ENGINE_RSP%") do (
        echo %%~nxF | find /i "ClassFactoryInstantiations" >nul
        if errorlevel 1 echo "%%~fF"
    )
)
link /nologo /NODEFAULTLIB:python23.lib /out:"%OUT%\bf2_factory_free.exe" /subsystem:console @"%OUT%\factory_free.rsp" Ws2_32.lib user32.lib > "%OUT%\link_factory_free.log" 2>&1
find /c "unresolved external symbol" < "%OUT%\link_factory_free.log" > "%OUT%\count_factory_free.txt"
set /p NFRAW=<"%OUT%\count_factory_free.txt"
set "NFFINAL=0"
set "NFSUMMARY="
for /f "delims=" %%L in ('findstr /c:"fatal error LNK1120:" "%OUT%\link_factory_free.log"') do set "NFSUMMARY=%%L"
if defined NFSUMMARY for /f "tokens=1" %%N in ("!NFSUMMARY:*LNK1120:=!") do set "NFFINAL=%%N"
echo factory-free unresolved diagnostics: !NFRAW!
echo factory-free unique externals: !NFFINAL!
echo   factory-free list: %OUT%\link_factory_free.log

if !BAD!==0 if !NRAW!==0 (echo ALL LINKED - THIS IS AN EXECUTABLE & exit /b 0)
exit /b 1
