@echo off
setlocal
rem [TEMP] [CHOSEN, not recovered] x86 VC7.1 set-layout topology/allocation fixture.
call "%~dp0..\Build\vsdevcmd.cmd" x86
call "%~dp0..\Build\dxsdk.cmd" x86
if errorlevel 1 exit /b 1
set "TASK_ROOT=%~dp0..\.."
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%~dp0..\Build\obj_vc71_set_runtime_source_audit"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"

call :build_and_run native "/MD /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0"
exit /b %errorlevel%

:build_and_run
set "TASK_NAME=%~1"
set "TASK_ABI_FLAGS=%~2"
cl /nologo /EHsc /O2 /std:c++14 /permissive /DDEDICATED=0 /DBF2_IMAGE_RENDDX9 %TASK_ABI_FLAGS% ^
  /FI"%TASK_ROOT%\Code\BF2\Core\BuildConfig.h" ^
  "%TASK_ROOT%\Code\Tests\vc71_set_runtime.cpp" ^
  /Fe"%BF2_BUILD_OUT%\vc71_set_runtime_%TASK_NAME%.exe" ^
  /link /INCREMENTAL:NO >"%BF2_BUILD_OUT%\%TASK_NAME%_build.log" 2>&1
if errorlevel 1 (
  type "%BF2_BUILD_OUT%\%TASK_NAME%_build.log"
  exit /b 1
)
"%BF2_BUILD_OUT%\vc71_set_runtime_%TASK_NAME%.exe"
exit /b %errorlevel%
