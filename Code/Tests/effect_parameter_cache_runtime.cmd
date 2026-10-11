@echo off
setlocal
rem [TEMP] Strict cache lookup/logging fixture, native x86, actual production logger.
rem BF2_EFFECT_SOURCE can name a saved Effect.cpp snapshot. Timings are informational.
rem Only a baseline run sets BF2_EXPECT_LOOKUP_TRACE=1; normal runs assert zero trace growth.
if not "%~1"=="" (echo Usage: %~nx0& exit /b 2)
if not defined BF2_RENDERER_OBJECTS (echo Set BF2_RENDERER_OBJECTS to an isolated native renddx9.cmd object directory.& exit /b 2)
if not exist "%BF2_RENDERER_OBJECTS%\20_Effect.obj" (echo Native renderer objects are missing or the numbered manifest changed.& exit /b 2)
call "%~dp0..\Build\vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1
call "%~dp0..\Build\dxsdk.cmd" x86
if errorlevel 1 exit /b 1
set "TASK_CODE=%~dp0.."
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%TASK_CODE%\Build\obj_effect_parameter_cache"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"
set "TASK_FLAGS=/nologo /c /MD /std:c++14 /permissive /EHsc /Od /Z7 /Gy /Gw /DBF2_IMAGE_RENDDX9 /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0 /DDEDICATED=0 /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS"
set "TASK_SOURCE_FLAG="
if defined BF2_EFFECT_SOURCE set TASK_SOURCE_FLAG=/DBF2_EFFECT_SOURCE=\"%BF2_EFFECT_SOURCE%\"
if "%BF2_EXPECT_LOOKUP_TRACE%"=="1" set "TASK_SOURCE_FLAG=%TASK_SOURCE_FLAG% /DBF2_TEST_EXPECT_LOOKUP_TRACE=1"
cl %TASK_FLAGS% %TASK_SOURCE_FLAG% /I"%TASK_CODE%\BF2\RendDX9" /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%~dp0effect_parameter_cache_runtime.cpp" /Fo"%BF2_BUILD_OUT%\fixture.obj" >"%BF2_BUILD_OUT%\compile.log" 2>&1
if errorlevel 1 (type "%BF2_BUILD_OUT%\compile.log" & exit /b 1)
for %%F in (BfMemory Vc71String Frontier) do (
    cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\RendDX9Abi\%%F.cpp" /Fo"%BF2_BUILD_OUT%\%%F.obj" >>"%BF2_BUILD_OUT%\compile.log" 2>&1
    if errorlevel 1 (type "%BF2_BUILD_OUT%\compile.log" & exit /b 1)
)
type nul >"%BF2_BUILD_OUT%\renderer.rsp"
for %%F in ("%BF2_RENDERER_OBJECTS%\*.obj") do if /i not "%%~nF"=="20_Effect" if /i not "%%~nF"=="1_BfMemory" if /i not "%%~nF"=="3_Vc71String" if /i not "%%~nF"=="4_Frontier" echo "%%~fF">>"%BF2_BUILD_OUT%\renderer.rsp"
link /nologo /OPT:REF /INCREMENTAL:NO /DEBUG /SUBSYSTEM:CONSOLE /OUT:"%BF2_BUILD_OUT%\effect_parameter_cache_runtime.exe" /PDB:"%BF2_BUILD_OUT%\fixture.pdb" /MAP:"%BF2_BUILD_OUT%\fixture.map" "%BF2_BUILD_OUT%\fixture.obj" "%BF2_BUILD_OUT%\BfMemory.obj" "%BF2_BUILD_OUT%\Vc71String.obj" "%BF2_BUILD_OUT%\Frontier.obj" @"%BF2_BUILD_OUT%\renderer.rsp" "%BF2_DXSDK_LIB%\d3dx9.lib" "%BF2_RENDERER_OBJECTS%\binkw32.lib" "%BF2_RENDERER_OBJECTS%\TextureAtlasBuilder.lib" kernel32.lib user32.lib d3d9.lib >"%BF2_BUILD_OUT%\link.log" 2>&1
if errorlevel 1 (type "%BF2_BUILD_OUT%\link.log" & exit /b 1)
pushd "%BF2_BUILD_OUT%"
effect_parameter_cache_runtime.exe >runtime.log 2>&1
set "TASK_RESULT=%errorlevel%"
type runtime.log
popd
exit /b %TASK_RESULT%
