@echo off
setlocal
rem [TEMP] [CHOSEN, not recovered] x86 ABI fixture; no renderer or live particle system.
call "%~dp0..\Build\vsdevcmd.cmd" x86
call "%~dp0..\Build\dxsdk.cmd" x86
if errorlevel 1 exit /b 1
set "TASK_ROOT=%~dp0..\.."
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%~dp0..\Build\particle_system_manager_abi_20260915"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"

call :build_and_run engine "/MD /DDEDICATED=0"
if errorlevel 1 exit /b 1
call :build_and_run renddx9_native "/MD /DDEDICATED=0 /DBF2_IMAGE_RENDDX9 /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0"
exit /b %errorlevel%

:build_and_run
set "TASK_NAME=%~1"
set "TASK_FLAGS=%~2"
cl /nologo /EHsc /O2 /std:c++14 /permissive %TASK_FLAGS% ^
  /FI"%TASK_ROOT%\Code\BF2\Core\BuildConfig.h" ^
  "%TASK_ROOT%\Code\Tests\particle_system_manager_abi.cpp" ^
  /Fe"%BF2_BUILD_OUT%\particle_system_manager_abi_%TASK_NAME%.exe" ^
  /link /INCREMENTAL:NO >"%BF2_BUILD_OUT%\%TASK_NAME%_build.log" 2>&1
if errorlevel 1 (
  type "%BF2_BUILD_OUT%\%TASK_NAME%_build.log"
  exit /b 1
)
"%BF2_BUILD_OUT%\particle_system_manager_abi_%TASK_NAME%.exe"
exit /b %errorlevel%
