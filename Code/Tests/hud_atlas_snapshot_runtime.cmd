@echo off
setlocal EnableDelayedExpansion
rem [TEMP] Atlas-copy helper fixture; strict native renderer object link.
if not defined BF2_HUD_ATLAS_FIXTURE_DIR (
  echo Set BF2_HUD_ATLAS_FIXTURE_DIR to a fixture directory containing the matching native_obj build.
  exit /b 2
)
for %%I in ("%~dp0..") do set "TASK_CODE=%%~fI"
set "TASK_NATIVE=%BF2_HUD_ATLAS_FIXTURE_DIR%\native_obj"
set "TASK_OUT=%BF2_HUD_ATLAS_FIXTURE_DIR%\atlas_snapshot_runtime"
for %%F in (49_QuadDrawer.obj 15_TextureManager.obj binkw32.lib) do (
  if not exist "%TASK_NATIVE%\%%F" (
    echo Missing native renderer fixture input: "%TASK_NATIVE%\%%F"
    exit /b 2
  )
)
call "%~dp0..\Build\vsdevcmd.cmd" x86
call "%~dp0..\Build\dxsdk.cmd" x86
if errorlevel 1 exit /b 1
if not exist "%TASK_OUT%" mkdir "%TASK_OUT%"
cl /nologo /c /MD /std:c++14 /permissive /EHsc /Od /Z7 /Gy /DBF2_IMAGE_RENDDX9 /DDEDICATED=0 /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0 /D_CRT_SECURE_NO_WARNINGS /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%~dp0hud_atlas_snapshot_runtime.cpp" /Fo"%TASK_OUT%\fixture.obj" >"%TASK_OUT%\compile.log" 2>&1
if errorlevel 1 (type "%TASK_OUT%\compile.log" & exit /b 1)
rem Use the matching 49-TU native renderer object set; this fixture supplies TextureManager.
type nul >"%TASK_OUT%\objects.rsp"
for %%F in ("%TASK_NATIVE%\*.obj") do if /i not "%%~nF"=="15_TextureManager" echo "%%~fF">>"%TASK_OUT%\objects.rsp"
link /nologo /MACHINE:X86 /DEBUG /OPT:REF /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /OUT:"%TASK_OUT%\hud_atlas_snapshot_runtime.exe" /PDB:"%TASK_OUT%\fixture.pdb" "%TASK_OUT%\fixture.obj" @"%TASK_OUT%\objects.rsp" "%BF2_DXSDK_LIB%\d3dx9.lib" "%TASK_NATIVE%\binkw32.lib" kernel32.lib user32.lib d3d9.lib >"%TASK_OUT%\link.log" 2>&1
set "TASK_LINK_RESULT=!errorlevel!"
type "%TASK_OUT%\link.log"
if not "%TASK_LINK_RESULT%"=="0" exit /b %TASK_LINK_RESULT%
set "PATH=D:\B2BF Launcher\Battlefield2;%PATH%"
"%TASK_OUT%\hud_atlas_snapshot_runtime.exe" >"%TASK_OUT%\runtime.log" 2>&1
set "TASK_RUNTIME_RESULT=!errorlevel!"
type "%TASK_OUT%\runtime.log"
exit /b %TASK_RUNTIME_RESULT%

