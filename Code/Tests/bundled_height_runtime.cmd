@echo off
setlocal
rem [TEMP] Strict optimized x86 renderer-only fixture, actual production height/filter TU.
if defined BF2_TEST_ABI_NATIVE if not "%BF2_TEST_ABI_NATIVE%"=="1" (echo Only the native plugin ABI is supported.& exit /b 2)
call "%~dp0..\Build\vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1
call "%~dp0..\Build\dxsdk.cmd" x86
if errorlevel 1 exit /b 1
set "TASK_OUT=%~dp0..\Build\bundled_height_20260923\test1"
if defined BF2_BUILD_OUT set "TASK_OUT=%BF2_BUILD_OUT%"
if not exist "%TASK_OUT%" mkdir "%TASK_OUT%"
cl /nologo /MD /std:c++14 /permissive /EHsc /Gy /O2 /DBF2_IMAGE_RENDDX9 /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0 /DDEDICATED=0 /FI"%~dp0..\BF2\Core\BuildConfig.h" "%~dp0bundled_height_runtime.cpp" /Fo"%TASK_OUT%\fixture.obj" /Fe"%TASK_OUT%\fixture.exe" /link /OPT:REF /INCREMENTAL:NO >"%TASK_OUT%\build.log" 2>&1
if errorlevel 1 (
 type "%TASK_OUT%\build.log"
 exit /b 1
)
"%TASK_OUT%\fixture.exe"
