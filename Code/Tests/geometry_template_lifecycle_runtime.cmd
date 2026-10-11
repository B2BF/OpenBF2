@echo off
setlocal
rem [TEMP] Strict x86 actual GeometryTemplateManager lifecycle fixture, no game or DLL launch.
if defined BF2_TEST_ABI_NATIVE if not "%BF2_TEST_ABI_NATIVE%"=="1" (echo Only the native plugin ABI is supported.& exit /b 2)
call "%~dp0..\Build\vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1
call "%~dp0..\Build\dxsdk.cmd" x86
if errorlevel 1 exit /b 1
set "TASK_ROOT=%~dp0.."
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%~dp0..\Build\hemi_draw_20260922\gtm_tests"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"
set "TASK_FLAGS=/nologo /c /EHsc /O2 /std:c++14 /permissive /Gy /MD /DDEDICATED=0 /DBF2_IMAGE_RENDDX9 /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0"
cl %TASK_FLAGS% /FI"%TASK_ROOT%\BF2\Core\BuildConfig.h" "%~dp0geometry_template_lifecycle_runtime.cpp" /Fo"%BF2_BUILD_OUT%\fixture.obj" >"%BF2_BUILD_OUT%\build.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /FI"%TASK_ROOT%\BF2\Core\BuildConfig.h" "%TASK_ROOT%\RendDX9Abi\Vc71String.cpp" /Fo"%BF2_BUILD_OUT%\Vc71String.obj" >>"%BF2_BUILD_OUT%\build.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /FI"%TASK_ROOT%\BF2\Core\BuildConfig.h" "%TASK_ROOT%\BF2\Core\ConsoleObjectBase.cpp" /Fo"%BF2_BUILD_OUT%\ConsoleObjectBase.obj" >>"%BF2_BUILD_OUT%\build.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /FI"%TASK_ROOT%\BF2\Core\BuildConfig.h" "%TASK_ROOT%\RendDX9Abi\NativeConsoleObjects.cpp" /Fo"%BF2_BUILD_OUT%\NativeConsoleObjects.obj" >>"%BF2_BUILD_OUT%\build.log" 2>&1
if errorlevel 1 goto failed
set "TASK_CONSOLE_OBJS="%BF2_BUILD_OUT%\ConsoleObjectBase.obj" "%BF2_BUILD_OUT%\NativeConsoleObjects.obj""
link /nologo /OPT:REF /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /OUT:"%BF2_BUILD_OUT%\geometry_template_lifecycle_runtime.exe" "%BF2_BUILD_OUT%\fixture.obj" "%BF2_BUILD_OUT%\Vc71String.obj" %TASK_CONSOLE_OBJS% >>"%BF2_BUILD_OUT%\build.log" 2>&1
if errorlevel 1 goto failed
"%BF2_BUILD_OUT%\geometry_template_lifecycle_runtime.exe" >"%BF2_BUILD_OUT%\runtime.log" 2>&1
set "TASK_RESULT=%errorlevel%"
type "%BF2_BUILD_OUT%\runtime.log"
exit /b %TASK_RESULT%
:failed
type "%BF2_BUILD_OUT%\build.log"
exit /b 1
