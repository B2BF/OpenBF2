@echo off
setlocal
rem [TEMP] Strict x86 client-image lifecycle check; output belongs to the caller.
call "%~dp0..\Build\vsdevcmd.cmd" x86
call "%~dp0..\Build\dxsdk.cmd" x86
if errorlevel 1 exit /b 1
set "TASK_ROOT=%~dp0.."
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%TASK_ROOT%\Build\hemi_draw_20260922\overgrowth_test"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"
cl /nologo /EHsc /O2 /std:c++14 /permissive /MD /D_ITERATOR_DEBUG_LEVEL=0 ^
 /DDEDICATED=0 /DBF2_IMAGE_RENDDX9 /DBF2_PLUGIN_ABI_NATIVE=1 ^
 /FI"%TASK_ROOT%\BF2\Core\BuildConfig.h" ^
 "%TASK_ROOT%\Tests\overgrowth_runtime.cpp" ^
 "%TASK_ROOT%\BF2\Geom\Overgrowth\OverGrowth.cpp" ^
 /Fe"%BF2_BUILD_OUT%\overgrowth_runtime.exe" /link /INCREMENTAL:NO ^
 >"%BF2_BUILD_OUT%\build.log" 2>&1
if errorlevel 1 (
 type "%BF2_BUILD_OUT%\build.log"
 exit /b 1
)
"%BF2_BUILD_OUT%\overgrowth_runtime.exe"
exit /b %errorlevel%
