@echo off
setlocal
rem [CHOSEN, not recovered] Builds and runs caps_win.cpp: the real D3D9 HAL caps of adapter 0 into
rem caps_reference.txt next to this script. Needs MSVC (vsdevcmd x86) and d3d9.lib only.
call "%~dp0..\..\Build\vsdevcmd.cmd" x86
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%~dp0..\..\Build\obj_caps_reference"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"
cl /nologo /MD /EHsc /O2 "%~dp0caps_win.cpp" /Fo"%BF2_BUILD_OUT%\\" /Fe"%BF2_BUILD_OUT%\caps_win.exe" d3d9.lib >"%BF2_BUILD_OUT%\compile.log" 2>&1
if errorlevel 1 (
  type "%BF2_BUILD_OUT%\compile.log"
  exit /b 1
)
"%BF2_BUILD_OUT%\caps_win.exe" "%~dp0caps_reference.txt"
exit /b %errorlevel%
