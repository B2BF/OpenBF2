@echo off
setlocal
rem [TEMP] Strict isolated synthetic stream/texture regression; no /FORCE.
call "%~dp0..\Build\vsdevcmd.cmd" x86
call "%~dp0..\Build\dxsdk.cmd" x86
if errorlevel 1 exit /b 1
set "ROOT=%~dp0..\BF2"
set "OUT=%~dp0..\Build\bundled_state_runtime_20260923"
if defined BF2_BUILD_OUT set "OUT=%BF2_BUILD_OUT%"
if not exist "%OUT%" mkdir "%OUT%"
set "FLAGS=/nologo /c /MD /std:c++14 /permissive /EHsc /O2 /Gy /DBF2_IMAGE_RENDDX9 /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0 /DDEDICATED=0 /FI%ROOT%\Core\BuildConfig.h /D_CRT_SECURE_NO_WARNINGS"
cl %FLAGS% "%~dp0bundled_mesh_state_runtime.cpp" /Fo"%OUT%\fixture.obj" >"%OUT%\compile.log" 2>&1
if errorlevel 1 (type "%OUT%\compile.log" & exit /b 1)
link /nologo /OPT:REF /SUBSYSTEM:CONSOLE /OUT:"%OUT%\fixture.exe" "%OUT%\fixture.obj" >"%OUT%\link.log" 2>&1
if errorlevel 1 (type "%OUT%\link.log" & exit /b 1)
"%OUT%\fixture.exe" >"%OUT%\runtime.log" 2>&1
set "RESULT=%errorlevel%"
type "%OUT%\runtime.log"
exit /b %RESULT%
