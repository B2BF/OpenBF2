@echo off
setlocal EnableDelayedExpansion
rem TEMP actual movie/shader/state/buffer/Video integration, isolated output only.
if "%~3"=="" (echo Usage: %~nx0 ^<fresh-output^> ^<native-renderer-obj-dir^> ^<game-dir^>& exit /b 2)
if not "%~4"=="" exit /b 2
for %%I in ("%~1") do set "TASK_OUT=%%~fI"
for %%I in ("%~2") do set "TASK_NATIVE=%%~fI"
for %%I in ("%~3") do set "TASK_GAME=%%~fI"
for %%I in ("%~dp0..") do set "TASK_CODE=%%~fI"
if exist "%TASK_OUT%" (echo Refusing to overwrite existing output directory.& exit /b 2)
if not exist "%TASK_NATIVE%\22_ShaderManager.obj" exit /b 2
if not exist "%TASK_NATIVE%\TextureAtlasBuilder.lib" exit /b 2
call "%~dp0..\Build\vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1
call "%~dp0..\Build\dxsdk.cmd" x86
if errorlevel 1 exit /b 1
mkdir "%TASK_OUT%"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0fsmovie_stock_shader.ps1" -GameDirectory "%TASK_GAME%" -OutputFile "%TASK_OUT%\LoadingScreen.fx"
if errorlevel 1 exit /b 1
cl /nologo /c /MD /std:c++14 /permissive /EHsc /Od /Z7 /Gy /DBF2_IMAGE_RENDDX9 /DDEDICATED=0 /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0 /D_CRT_SECURE_NO_WARNINGS /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%~dp0fsmovie_d3d_runtime.cpp" /Fo"%TASK_OUT%\fixture.obj" >"%TASK_OUT%\compile.log" 2>&1
if errorlevel 1 (type "%TASK_OUT%\compile.log" & exit /b 1)
type nul >"%TASK_OUT%\objects.rsp"
for %%F in ("%TASK_NATIVE%\*.obj") do if /i not "%%~nF"=="22_ShaderManager" echo "%%~fF">>"%TASK_OUT%\objects.rsp"
link /nologo /MACHINE:X86 /DEBUG /OPT:REF /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /OUT:"%TASK_OUT%\fsmovie_d3d_runtime.exe" /PDB:"%TASK_OUT%\fixture.pdb" /MAP:"%TASK_OUT%\fixture.map" "%TASK_OUT%\fixture.obj" @"%TASK_OUT%\objects.rsp" "%BF2_DXSDK_LIB%\d3dx9.lib" "%TASK_NATIVE%\binkw32.lib" "%TASK_NATIVE%\TextureAtlasBuilder.lib" kernel32.lib user32.lib d3d9.lib >"%TASK_OUT%\link.log" 2>&1
if errorlevel 1 (type "%TASK_OUT%\link.log" & exit /b 1)
set "PATH=%TASK_GAME%;%PATH%"
set "BF2_RENDDX9_FRONTIER="
pushd "%TASK_OUT%"
.\fsmovie_d3d_runtime.exe "%TASK_GAME%" "%TASK_OUT%\LoadingScreen.fx" "%TASK_OUT%\movie-frame.bmp" >runtime.log 2>&1
set "TASK_RESULT=!errorlevel!"
type runtime.log
popd
exit /b %TASK_RESULT%
