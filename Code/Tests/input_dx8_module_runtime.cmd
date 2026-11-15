@echo off
setlocal
rem [TEMP] Native InputDX8Module lists/factory checks; no requested hardware device.
call "%~dp0..\Build\vsdevcmd.cmd" x86
if not defined BF2_TEST_DEDICATED set "BF2_TEST_DEDICATED=0"
if not defined BF2_TEST_OPT set "BF2_TEST_OPT=/O2"
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%~dp0..\Build\input_module_publication_20260906\client_o2"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"
set "TASK_ROOT=%~dp0..\BF2"
set "TASK_FLAGS=/nologo /EHsc /std:c++14 /permissive /MD /Z7 /Gy /DDEDICATED=%BF2_TEST_DEDICATED% /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0 /DPy_NO_ENABLE_SHARED /I"%TASK_ROOT%\External\zlib-1.2.3\contrib\minizip" /I"%TASK_ROOT%\External\zlib-1.2.3" /I"%TASK_ROOT%\External\python-embedded\py-src-2.3.4\Include" /I"%TASK_ROOT%\External\python-embedded\py-src-2.3.4\PC" %BF2_TEST_OPT%"
set "TASK_SOURCES=Core\ModuleRegistry"
set "TASK_LIBS=user32.lib"
if "%BF2_TEST_DEDICATED%"=="0" (
  set "TASK_SOURCES=%TASK_SOURCES% IO\Input\DX8InputDeviceManager IO\Input\InputManager IO\Input\KeyboardDevice IO\Input\MouseDevice IO\Input\GameControllerDevice Core\Debug Core\StringUtil"
  set "TASK_LIBS=%TASK_LIBS% dinput8.lib dxguid.lib"
)
set "TASK_OBJECTS="
type nul >"%BF2_BUILD_OUT%\compile.log"
for %%F in (%TASK_SOURCES%) do (
  for %%D in ("%BF2_BUILD_OUT%\%%F.obj") do if not exist "%%~dpD" mkdir "%%~dpD"
  cl %TASK_FLAGS% /FI"%TASK_ROOT%\Core\BuildConfig.h" /c "%TASK_ROOT%\%%F.cpp" /Fo"%BF2_BUILD_OUT%\%%F.obj" >>"%BF2_BUILD_OUT%\compile.log" 2>&1
  if errorlevel 1 (type "%BF2_BUILD_OUT%\compile.log" & exit /b 1)
  call set "TASK_OBJECTS=%%TASK_OBJECTS%% "%BF2_BUILD_OUT%\%%F.obj""
)
cl %TASK_FLAGS% /FI"%TASK_ROOT%\Core\BuildConfig.h" /c "%~dp0input_dx8_module_runtime.cpp" /Fo"%BF2_BUILD_OUT%\fixture.obj" >>"%BF2_BUILD_OUT%\compile.log" 2>&1
if errorlevel 1 (type "%BF2_BUILD_OUT%\compile.log" & exit /b 1)
link /nologo /FORCE:UNRESOLVED /OPT:REF /INCREMENTAL:NO /DEBUG /SUBSYSTEM:CONSOLE /OUT:"%BF2_BUILD_OUT%\input_dx8_module_runtime.exe" /PDB:"%BF2_BUILD_OUT%\fixture.pdb" %TASK_OBJECTS% "%BF2_BUILD_OUT%\fixture.obj" %TASK_LIBS% >"%BF2_BUILD_OUT%\link_force.log" 2>&1
if errorlevel 1 (type "%BF2_BUILD_OUT%\link_force.log" & exit /b 1)
set "PATH=D:\B2BF Launcher\Battlefield2;%PATH%"
"%BF2_BUILD_OUT%\input_dx8_module_runtime.exe" >"%BF2_BUILD_OUT%\runtime.log" 2>&1
set "TASK_RESULT=%errorlevel%"
type "%BF2_BUILD_OUT%\runtime.log"
exit /b %TASK_RESULT%


