@echo off
setlocal
rem [TEMP] SCAFFOLDING - runs the REAL StaticMeshTemplate::loadOccluders (PE 0x1011A0C0) over the
rem .occ files BF2 1.50 ships. See the .cpp header for why /FORCE:UNRESOLVED is safe here.
rem
rem   usage:  static_mesh_occluders_runtime.cmd
rem
rem The .occ files are extracted from the stock archives on first run into
rem %BF2_BUILD_OUT%\occ\. BF2_GAME_DIR overrides the game folder.
call "%~dp0..\Build\vsdevcmd.cmd" x86
call "%~dp0..\Build\dxsdk.cmd" x86
if errorlevel 1 exit /b 1
set "TASK_ROOT=%~dp0.."
if not defined BF2_GAME_DIR set "BF2_GAME_DIR=D:\B2BF Launcher\Battlefield2"
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%~dp0..\Build\occluders_20260926"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"
if not exist "%BF2_BUILD_OUT%\occ" mkdir "%BF2_BUILD_OUT%\occ"

rem ---- extract the stock .occ files (idempotent) -------------------------------------------
python "%~dp0static_mesh_occluders_extract.py" "%BF2_GAME_DIR%" "%BF2_BUILD_OUT%\occ" || goto failed

rem ---- the same flags renddx9.cmd uses, native flavour -------------------------------------
set "TASK_FLAGS=/nologo /c /std:c++14 /permissive /EHsc /MD /Zi /FS /DBF2_IMAGE_RENDDX9 /DBF2_PLUGIN_ABI_NATIVE=1 /DDEDICATED=0 /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS /D_ITERATOR_DEBUG_LEVEL=0 /FI"%TASK_ROOT%\BF2\Core\BuildConfig.h" /Fd"%BF2_BUILD_OUT%\fixture.pdb""

cl %TASK_FLAGS% "%~dp0static_mesh_occluders_runtime.cpp" /Fo"%BF2_BUILD_OUT%\fixture.obj" >"%BF2_BUILD_OUT%\build.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% "%TASK_ROOT%\BF2\Geom\StaticMeshTemplate.cpp" /Fo"%BF2_BUILD_OUT%\StaticMeshTemplate.obj" >>"%BF2_BUILD_OUT%\build.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% "%TASK_ROOT%\RendDX9Abi\Vc71String.cpp" /Fo"%BF2_BUILD_OUT%\Vc71String.obj" >>"%BF2_BUILD_OUT%\build.log" 2>&1
if errorlevel 1 goto failed

rem /FORCE:UNRESOLVED - deliberate, and bounded: loadOccluders reaches engineAlloc, engineFree,
rem logf and the stream's two slots, all supplied by the fixture. Everything else in the TU
rem becomes an RVA-0 that this fixture never calls.
link /nologo /DEBUG /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /FORCE:UNRESOLVED ^
 /OUT:"%BF2_BUILD_OUT%\static_mesh_occluders_runtime.exe" ^
 "%BF2_BUILD_OUT%\fixture.obj" "%BF2_BUILD_OUT%\StaticMeshTemplate.obj" "%BF2_BUILD_OUT%\Vc71String.obj" ^
 >>"%BF2_BUILD_OUT%\build.log" 2>&1
if errorlevel 1 goto failed

"%BF2_BUILD_OUT%\static_mesh_occluders_runtime.exe" "%BF2_BUILD_OUT%\occ" >"%BF2_BUILD_OUT%\runtime.log" 2>&1
set "TASK_RESULT=%errorlevel%"
type "%BF2_BUILD_OUT%\runtime.log"
exit /b %TASK_RESULT%

:failed
type "%BF2_BUILD_OUT%\build.log"
exit /b 1
