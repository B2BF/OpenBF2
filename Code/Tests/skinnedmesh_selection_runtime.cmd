@echo off
setlocal
rem [TEMP] Actual x86 SkinnedMesh selection fixture; no original DLL or game is loaded.
rem Uses the native plugin ABI with an isolated CRT heap.
rem BF2_TEST_SOURCE_FLAGS can define BF2_SKINNEDMESH_SOURCE to a saved source snapshot.
rem All links are strict: no /FORCE and no replaced SkinnedMesh implementation or vtable.
if defined BF2_TEST_ABI_NATIVE if not "%BF2_TEST_ABI_NATIVE%"=="1" (echo Only the native plugin ABI is supported.& exit /b 2)
call "%~dp0..\Build\vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1
call "%~dp0..\Build\dxsdk.cmd" x86
if errorlevel 1 exit /b 1
set "TASK_ROOT=%~dp0.."
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%~dp0..\Build\zero_radius_20260922\tests"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"
set "TASK_FLAGS=/nologo /c /MD /std:c++14 /permissive /EHsc /Od /Z7 /Gy /Gw /DBF2_IMAGE_RENDDX9 /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0 /DDEDICATED=0 /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS"
cl %TASK_FLAGS% %BF2_TEST_SOURCE_FLAGS% /I"%TASK_ROOT%\BF2\Geom" /FI"%TASK_ROOT%\BF2\Core\BuildConfig.h" "%~dp0skinnedmesh_selection_runtime.cpp" /Fo"%BF2_BUILD_OUT%\fixture.obj"
if errorlevel 1 exit /b 1
for %%F in (Vc71String) do (
    cl %TASK_FLAGS% /FI"%TASK_ROOT%\BF2\Core\BuildConfig.h" "%TASK_ROOT%\RendDX9Abi\%%F.cpp" /Fo"%BF2_BUILD_OUT%\%%F.obj"
    if errorlevel 1 exit /b 1
)
cl %TASK_FLAGS% /FI"%TASK_ROOT%\BF2\Core\BuildConfig.h" "%TASK_ROOT%\BF2\Math\Mat4.cpp" /Fo"%BF2_BUILD_OUT%\Mat4.obj"
if errorlevel 1 exit /b 1
cl %TASK_FLAGS% /FI"%TASK_ROOT%\BF2\Core\BuildConfig.h" "%TASK_ROOT%\BF2\Math\Vec3.cpp" /Fo"%BF2_BUILD_OUT%\Vec3.obj"
if errorlevel 1 exit /b 1
link /nologo /OPT:REF /INCREMENTAL:NO /DEBUG /SUBSYSTEM:CONSOLE /OUT:"%BF2_BUILD_OUT%\skinnedmesh_selection_runtime.exe" /PDB:"%BF2_BUILD_OUT%\fixture.pdb" /MAP:"%BF2_BUILD_OUT%\fixture.map" "%BF2_BUILD_OUT%\fixture.obj" "%BF2_BUILD_OUT%\Vc71String.obj" "%BF2_BUILD_OUT%\Mat4.obj" "%BF2_BUILD_OUT%\Vec3.obj" user32.lib >"%BF2_BUILD_OUT%\link.log" 2>&1
set "TASK_RESULT=%errorlevel%"
if not "%TASK_RESULT%"=="0" type "%BF2_BUILD_OUT%\link.log"
if not "%TASK_RESULT%"=="0" exit /b %TASK_RESULT%
"%BF2_BUILD_OUT%\skinnedmesh_selection_runtime.exe" >"%BF2_BUILD_OUT%\runtime.log" 2>&1
set "TASK_RESULT=%errorlevel%"
type "%BF2_BUILD_OUT%\runtime.log"
exit /b %TASK_RESULT%
