@echo off
setlocal
rem [TEMP] Strict fixture, real owner source with synthetic host dependencies.
call "%~dp0..\Build\vsdevcmd.cmd" x86
call "%~dp0..\Build\dxsdk.cmd" x86
if errorlevel 1 exit /b 1
set "ROOT=%~dp0..\BF2"
set "OUT=%~dp0..\Build\env_map_runtime_20260923"
if not exist "%OUT%" mkdir "%OUT%"
set "FLAGS=/nologo /c /MD /std:c++14 /permissive /EHsc /O2 /Gy /DBF2_IMAGE_RENDDX9 /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0 /DDEDICATED=0 /FI%ROOT%\Core\BuildConfig.h /D_CRT_SECURE_NO_WARNINGS"
cl %FLAGS% "%~dp0env_map_runtime.cpp" /Fo"%OUT%\fixture.obj" >"%OUT%\compile.log" 2>&1
if errorlevel 1 (type "%OUT%\compile.log" & exit /b 1)
cl %FLAGS% "%ROOT%\..\RendDX9Abi\Vc71String.cpp" /Fo"%OUT%\string.obj" >>"%OUT%\compile.log" 2>&1
if errorlevel 1 (type "%OUT%\compile.log" & exit /b 1)
link /nologo /OPT:REF /SUBSYSTEM:CONSOLE /OUT:"%OUT%\fixture.exe" "%OUT%\fixture.obj" "%OUT%\string.obj" >"%OUT%\link.log" 2>&1
if errorlevel 1 (type "%OUT%\link.log" & exit /b 1)
"%OUT%\fixture.exe"
exit /b %errorlevel%
