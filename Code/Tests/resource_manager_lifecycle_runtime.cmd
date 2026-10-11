@echo off
setlocal
rem [TEMP] Strict x86 ResourceManager lifecycle fixture; no actual GPU, files or map loading.
if not defined BF2_BUILD_OUT exit /b 2
call "%~dp0..\Build\vsdevcmd.cmd" x86
call "%~dp0..\Build\dxsdk.cmd" x86
if errorlevel 1 exit /b 1
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"
set "TASK_ROOT=%~dp0..\.."
if not defined BF2_TEST_OPT set "BF2_TEST_OPT=/O2"
cl /nologo %BF2_TEST_OPT% /EHsc /std:c++14 /permissive /MD /Gy /Gw /Z7 ^
 /DDEDICATED=0 /DBF2_IMAGE_RENDDX9 /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0 ^
 /FI"%TASK_ROOT%\Code\BF2\Core\BuildConfig.h" /Fo"%BF2_BUILD_OUT%\\" ^
 /Fe"%BF2_BUILD_OUT%\resource_manager_lifecycle_runtime.exe" ^
 "%~dp0resource_manager_lifecycle_runtime.cpp" "%TASK_ROOT%\Code\RendDX9Abi\Vc71String.cpp" ^
 /link /OPT:REF /INCREMENTAL:NO /SUBSYSTEM:CONSOLE
if errorlevel 1 exit /b 1
"%BF2_BUILD_OUT%\resource_manager_lifecycle_runtime.exe"
exit /b %errorlevel%
