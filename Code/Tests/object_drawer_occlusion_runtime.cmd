@echo off
setlocal
rem [TEMP] Strict native x86 actual-ObjectDrawer regression; no game or original DLL.
if not "%~1"=="" (echo Usage: %~nx0& exit /b 2)
call "%~dp0..\Build\vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1
call "%~dp0..\Build\dxsdk.cmd" x86
if errorlevel 1 exit /b 1
set "TASK_ROOT=%~dp0.."
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%~dp0..\Build\obj_culling_recovery_tests"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"
set "TASK_FLAGS=/nologo /c /MD /std:c++14 /permissive /EHsc /Od /Z7 /Gy /Gw /DBF2_IMAGE_RENDDX9 /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0 /DDEDICATED=0 /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS"
cl %TASK_FLAGS% /FI"%TASK_ROOT%\BF2\Core\BuildConfig.h" "%~dp0object_drawer_occlusion_runtime.cpp" /Fo"%BF2_BUILD_OUT%\fixture.obj"
if errorlevel 1 exit /b 1
link /nologo /OPT:REF /INCREMENTAL:NO /DEBUG /SUBSYSTEM:CONSOLE /OUT:"%BF2_BUILD_OUT%\object_drawer_occlusion_runtime.exe" /PDB:"%BF2_BUILD_OUT%\fixture.pdb" /MAP:"%BF2_BUILD_OUT%\fixture.map" "%BF2_BUILD_OUT%\fixture.obj" >"%BF2_BUILD_OUT%\link.log" 2>&1
set "TASK_RESULT=%errorlevel%"
if not "%TASK_RESULT%"=="0" type "%BF2_BUILD_OUT%\link.log"
if not "%TASK_RESULT%"=="0" exit /b %TASK_RESULT%
"%BF2_BUILD_OUT%\object_drawer_occlusion_runtime.exe" >"%BF2_BUILD_OUT%\runtime.log" 2>&1
set "TASK_RESULT=%errorlevel%"
type "%BF2_BUILD_OUT%\runtime.log"
exit /b %TASK_RESULT%
