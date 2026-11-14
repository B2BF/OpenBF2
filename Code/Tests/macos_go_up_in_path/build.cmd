@echo off
setlocal
rem [CHOSEN, not recovered] Engine-free goUpInPath leading-'/' check, Windows twin of build.sh
rem (client and dedicated defines; both follow PE BF2.exe 0x00431BE0 on Windows).
call "%~dp0..\..\Build\vsdevcmd.cmd" x86
set "ROOT=%~dp0..\.."
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%~dp0..\..\Build\obj_go_up_in_path"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"
for %%d in (0 1) do (
  cl /nologo /MD /std:c++14 /permissive /EHsc /O2 /DDEDICATED=%%d /FI"%ROOT%\BF2\Core\BuildConfig.h" "%~dp0go_up_in_path_check.cpp" "%ROOT%\BF2\Core\StringUtil.cpp" /Fo"%BF2_BUILD_OUT%\\" /Fe"%BF2_BUILD_OUT%\check_%%d.exe" >"%BF2_BUILD_OUT%\compile_%%d.log" 2>&1
  if errorlevel 1 (
    type "%BF2_BUILD_OUT%\compile_%%d.log"
    exit /b 1
  )
  "%BF2_BUILD_OUT%\check_%%d.exe"
  if errorlevel 1 exit /b 1
)
exit /b 0
