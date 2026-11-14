@echo off
setlocal
rem [CHOSEN, not recovered] Builds and runs effect_check_win.cpp: D3DXCreateEffect, ValidateTechnique and
rem FindNextValidTechnique over the effects `nativefx export-effects` wrote to EFFDIR (first argument).
rem Writes EFFDIR\effect_check_result.txt; --compile-only writes effect_compile_result.txt without a device.
rem Second argument (unless -) or D3DX9_DLL: the d3dx9 DLL (e.g. the retail
rem d3dx9_25.dll). Needs MSVC (vsdevcmd x86) and d3d9.lib. Inputs and result are game-derived.
if "%~1"=="" (
  echo usage: effect_check.cmd EFFDIR [d3dx9.dll^|-] [--compile-only [--fresh-process]]
  exit /b 2
)
call "%~dp0..\..\Build\vsdevcmd.cmd" x86
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%~dp0..\..\Build\obj_effect_check"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"
cl /nologo /MD /EHsc /O2 "%~dp0effect_check_win.cpp" /Fo"%BF2_BUILD_OUT%\\" /Fe"%BF2_BUILD_OUT%\effect_check.exe" /link /LARGEADDRESSAWARE d3d9.lib user32.lib >"%BF2_BUILD_OUT%\compile.log" 2>&1
if errorlevel 1 (
  type "%BF2_BUILD_OUT%\compile.log"
  exit /b 1
)
"%BF2_BUILD_OUT%\effect_check.exe" "%~1" %2 %3 %4
exit /b %errorlevel%
