@echo off
setlocal
rem TEMP native x86 actual FSMoviePlayer contract fixture; no DLL or game launch.
if not "%~1"=="" (echo Usage: %~nx0& exit /b 2)
call "%~dp0..\Build\vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1
call "%~dp0..\Build\dxsdk.cmd" x86
if errorlevel 1 exit /b 1
set "TASK_CODE=%~dp0.."
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%TASK_CODE%\Build\obj_fsmovie_runtime"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"
rem Extract only the unchanged contiguous ShaderParameter methods. Both anchors
rem must occur once. This keeps real typed forwarding while recording its owner.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0fsmovie_shaderparameter_extract.ps1" -CodeDirectory "%TASK_CODE%" -OutputDirectory "%BF2_BUILD_OUT%"
if errorlevel 1 exit /b 1
set "TASK_FLAGS=/nologo /c /MD /std:c++14 /permissive /EHsc /Od /Z7 /Gy /Gw /DBF2_IMAGE_RENDDX9 /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0 /DDEDICATED=0 /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS"
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" /FAs /Fa"%BF2_BUILD_OUT%\fixture.asm" "%~dp0fsmovie_runtime.cpp" /Fo"%BF2_BUILD_OUT%\fixture.obj" >"%BF2_BUILD_OUT%\compile.log" 2>&1
if errorlevel 1 (type "%BF2_BUILD_OUT%\compile.log" & exit /b 1)
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%BF2_BUILD_OUT%\ShaderParameter.cpp" /Fo"%BF2_BUILD_OUT%\ShaderParameter.obj" >>"%BF2_BUILD_OUT%\compile.log" 2>&1
if errorlevel 1 (type "%BF2_BUILD_OUT%\compile.log" & exit /b 1)
for %%F in (BfMemory) do (
    cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\RendDX9Abi\%%F.cpp" /Fo"%BF2_BUILD_OUT%\%%F.obj" >>"%BF2_BUILD_OUT%\compile.log" 2>&1
    if errorlevel 1 (type "%BF2_BUILD_OUT%\compile.log" & exit /b 1)
)
link /nologo /MACHINE:X86 /OPT:REF /INCREMENTAL:NO /DEBUG /SUBSYSTEM:CONSOLE /OUT:"%BF2_BUILD_OUT%\fsmovie_runtime.exe" /PDB:"%BF2_BUILD_OUT%\fixture.pdb" /MAP:"%BF2_BUILD_OUT%\fixture.map" "%BF2_BUILD_OUT%\fixture.obj" "%BF2_BUILD_OUT%\ShaderParameter.obj" "%BF2_BUILD_OUT%\BfMemory.obj" >"%BF2_BUILD_OUT%\link.log" 2>&1
if errorlevel 1 (type "%BF2_BUILD_OUT%\link.log" & exit /b 1)
pushd "%BF2_BUILD_OUT%"
.\fsmovie_runtime.exe >runtime.log 2>&1
set "TASK_RESULT=%errorlevel%"
type runtime.log
popd
exit /b %TASK_RESULT%
