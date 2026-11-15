@echo off
setlocal
call "%~dp0..\Build\vsdevcmd.cmd" x86
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%~dp0..\Build\obj_dx8_input_manager_runtime"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"
set "TASK_ROOT=%~dp0..\.."
set "TASK_FLAGS=/nologo /EHsc /std:c++14 /permissive /MD /Z7 /Gy /DDEDICATED=0 /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0"
for %%S in (KeyboardDevice MouseDevice GameControllerDevice DX8InputDeviceManager) do (
    cl %TASK_FLAGS% /FI"%TASK_ROOT%\Code\BF2\Core\BuildConfig.h" /c "%TASK_ROOT%\Code\BF2\IO\Input\%%S.cpp" /Fo"%BF2_BUILD_OUT%\%%S.obj"
    if errorlevel 1 exit /b 1
)
for %%S in (Debug StringUtil) do (
    cl %TASK_FLAGS% /FI"%TASK_ROOT%\Code\BF2\Core\BuildConfig.h" /c "%TASK_ROOT%\Code\BF2\Core\%%S.cpp" /Fo"%BF2_BUILD_OUT%\%%S.obj"
    if errorlevel 1 exit /b 1
)
cl %TASK_FLAGS% /FI"%TASK_ROOT%\Code\BF2\Core\BuildConfig.h" /c "%~dp0dx8_input_manager_runtime.cpp" /Fo"%BF2_BUILD_OUT%\fixture.obj"
if errorlevel 1 exit /b 1
link /nologo /FORCE /OPT:REF /INCREMENTAL:NO /DEBUG /SUBSYSTEM:CONSOLE /OUT:"%BF2_BUILD_OUT%\dx8_input_manager_runtime.exe" /PDB:"%BF2_BUILD_OUT%\dx8_input_manager_runtime.pdb" "%BF2_BUILD_OUT%\DX8InputDeviceManager.obj" "%BF2_BUILD_OUT%\KeyboardDevice.obj" "%BF2_BUILD_OUT%\MouseDevice.obj" "%BF2_BUILD_OUT%\GameControllerDevice.obj" "%BF2_BUILD_OUT%\Debug.obj" "%BF2_BUILD_OUT%\StringUtil.obj" "%BF2_BUILD_OUT%\fixture.obj" dinput8.lib dxguid.lib user32.lib
if errorlevel 1 exit /b 1
set "PATH=D:\B2BF Launcher\Battlefield2;%PATH%"
"%BF2_BUILD_OUT%\dx8_input_manager_runtime.exe"
exit /b %errorlevel%
