@echo off
setlocal
rem [CHOSEN, not recovered] Builds and runs the fixed-function vertex reference generator with the
rem real d3d9 runtime (0.3.0 pieces 1-3). Writes ff_reference.txt next to this script; the macOS
rem checks compare against it. Needs MSVC (vsdevcmd x86) and the Windows SDK's d3d9.lib only.
call "%~dp0..\..\Build\vsdevcmd.cmd" x86
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%~dp0..\..\Build\obj_ff_reference"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"
cl /nologo /MD /std:c++14 /EHsc /O2 "%~dp0ff_reference_win.cpp" "%~dp0ff_cases.cpp" /Fo"%BF2_BUILD_OUT%\\" /Fe"%BF2_BUILD_OUT%\ff_reference.exe" d3d9.lib user32.lib >"%BF2_BUILD_OUT%\compile.log" 2>&1
if errorlevel 1 (
  type "%BF2_BUILD_OUT%\compile.log"
  exit /b 1
)
"%BF2_BUILD_OUT%\ff_reference.exe" "%~dp0ff_reference.txt"
exit /b %errorlevel%
