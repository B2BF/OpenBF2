@echo off
setlocal
rem [CHOSEN, not recovered] Builds and runs d3dx_jobs_win.cpp: D3DXCompileShader over the shader jobs
rem `nativefx export-jobs` wrote to JOBSDIR (first argument; jobs.tsv plus KEY.hlsl files). Writes
rem JOBSDIR\d3dx_jobs_result.txt. Second argument or %%D3DX9_DLL%%: the d3dx9 DLL (e.g. the retail
rem d3dx9_25.dll). Needs MSVC (vsdevcmd x86) only. Inputs and result are game-derived: not committed.
if "%~1"=="" (
  echo usage: d3dx_jobs.cmd JOBSDIR [d3dx9.dll]
  exit /b 2
)
call "%~dp0..\..\Build\vsdevcmd.cmd" x86
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%~dp0..\..\Build\obj_d3dx_jobs"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"
cl /nologo /MD /EHsc /O2 "%~dp0d3dx_jobs_win.cpp" /Fo"%BF2_BUILD_OUT%\\" /Fe"%BF2_BUILD_OUT%\d3dx_jobs.exe" >"%BF2_BUILD_OUT%\compile.log" 2>&1
if errorlevel 1 (
  type "%BF2_BUILD_OUT%\compile.log"
  exit /b 1
)
"%BF2_BUILD_OUT%\d3dx_jobs.exe" "%~1" %2
exit /b %errorlevel%
