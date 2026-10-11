@echo off
setlocal EnableDelayedExpansion
rem TEMP real D3D component; requires an isolated native build and stock shader input.
if not defined BF2_EFFECT_FIXTURE_DIR (
  echo Set BF2_EFFECT_FIXTURE_DIR to a directory containing native_obj and stock_PortedMenuShader.fx.
  exit /b 2
)
call "%~dp0..\Build\vsdevcmd.cmd" x86
call "%~dp0..\Build\dxsdk.cmd" x86
if errorlevel 1 exit /b 1
for %%I in ("%~dp0..") do set "TASK_CODE=%%~fI"
set "TASK_EVIDENCE=%BF2_EFFECT_FIXTURE_DIR%"
set "TASK_NATIVE=%TASK_EVIDENCE%\native_obj"
set "TASK_OUT=%TASK_EVIDENCE%\d3d_runtime"
if not exist "%TASK_OUT%" mkdir "%TASK_OUT%"
if not exist "%TASK_NATIVE%\49_QuadDrawer.obj" exit /b 2
cl /nologo /c /MD /std:c++14 /permissive /EHsc /Od /Z7 /Gy /DBF2_IMAGE_RENDDX9 /DDEDICATED=0 /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0 /D_CRT_SECURE_NO_WARNINGS /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%~dp0effect_stock_d3d_runtime.cpp" /Fo"%TASK_OUT%\fixture.obj" >"%TASK_OUT%\compile.log" 2>&1
if errorlevel 1 (type "%TASK_OUT%\compile.log" & exit /b 1)
type nul >"%TASK_OUT%\objects.rsp"
for %%F in ("%TASK_NATIVE%\*.obj") do if /i not "%%~nF"=="19_Effect" echo "%%~fF">>"%TASK_OUT%\objects.rsp"
link /nologo /MACHINE:X86 /DEBUG /OPT:REF /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /OUT:"%TASK_OUT%\effect_stock_d3d_runtime.exe" /PDB:"%TASK_OUT%\fixture.pdb" "%TASK_OUT%\fixture.obj" @"%TASK_OUT%\objects.rsp" "%BF2_DXSDK_LIB%\d3dx9.lib" "%TASK_NATIVE%\binkw32.lib" kernel32.lib user32.lib d3d9.lib >"%TASK_OUT%\link.log" 2>&1
set "TASK_LINK_RESULT=!errorlevel!"
type "%TASK_OUT%\link.log"
if not "%TASK_LINK_RESULT%"=="0" exit /b %TASK_LINK_RESULT%
set "PATH=D:\B2BF Launcher\Battlefield2;%PATH%"
set "BF2_RENDDX9_FRONTIER="
pushd "%TASK_OUT%"
effect_stock_d3d_runtime.exe "%TASK_EVIDENCE%\stock_PortedMenuShader.fx" "%TASK_OUT%\stock_menu_frame.bmp" >runtime.log 2>&1
set "TASK_RUNTIME_RESULT=!errorlevel!"
type runtime.log
popd
exit /b %TASK_RUNTIME_RESULT%
