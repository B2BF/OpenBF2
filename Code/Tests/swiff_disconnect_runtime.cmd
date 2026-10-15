@echo off
setlocal EnableDelayedExpansion
rem [TEMP] Actual registered SwiffPlayer callback, with external scalar host observers.
rem Usage: swiff_disconnect_runtime.cmd plugin-object-directory output-directory [native]
rem Supply fresh objects from the matching isolated Code/Build/swiffplayer.cmd build.
rem BF2_TEST_SOURCE_FLAGS may define BF2_SWIFFPLAYER_SOURCE to a pre-fix snapshot.
rem No /FORCE, original DLL, movie, renderer, socket or live game is used.
if "%~2"=="" exit /b 2
if not "%~3"=="" if /i not "%~3"=="native" (echo Only the native plugin ABI is supported.& exit /b 2)
call "%~dp0..\Build\vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1
for %%P in ("%~1") do set "OBJECTS=%%~fP"
for %%P in ("%~2") do set "OUT=%%~fP"
set "ROOT=%~dp0.."
set "EXT=%ROOT%\BF2\External"
set "SWF=%EXT%\gameswf\SwiffPlayer"
if not exist "%OUT%" mkdir "%OUT%"
set FLAGS=/nologo /c /MD /std:c++14 /permissive /EHsc /Od /Z7 /Gy /Gw /DBF2_IMAGE_SWIFFPLAYER /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0 /DDEDICATED=0 /DWIN32 /D_WINDOWS /DNDEBUG /D_CRT_SECURE_NO_WARNINGS
cl %FLAGS% %BF2_TEST_SOURCE_FLAGS% /I"%EXT%" /I"%EXT%\gameswf" /I"%SWF%" /FI"%ROOT%\BF2\Core\BuildConfig.h" "%~dp0swiff_disconnect_runtime.cpp" /Fo"%OUT%\fixture.obj" >"%OUT%\compile.log" 2>&1
if errorlevel 1 (type "%OUT%\compile.log"& exit /b 1)
rem Only gameswf/base and the gameswf allocator adapter are linked.
rem The fixture includes the actual SwiffPlayer TU and supplies isolated CRT heap providers.
type nul >"%OUT%\objects.rsp"
set /a COUNT=0
for %%O in ("%OBJECTS%\*.obj") do (
    set "SKIP=0"
    for %%S in (36_BfMemory 37_DllMain 38_PluginEntry 40_SwiffPlayer 41_SwiffCrashLog 42_SwiffPlayerModule 43_Frontier) do if /i "%%~nO"=="%%S" set "SKIP=1"
    if "!SKIP!"=="0" (echo "%%~fO">>"%OUT%\objects.rsp"& set /a COUNT+=1)
)
if not "%COUNT%"=="36" (echo Expected 36 dependency objects, found %COUNT%. Check the plugin manifest.& exit /b 1)
link /nologo /MACHINE:X86 /OPT:REF /INCREMENTAL:NO /DEBUG /SUBSYSTEM:CONSOLE /OUT:"%OUT%\swiff_disconnect_runtime.exe" /PDB:"%OUT%\fixture.pdb" "%OUT%\fixture.obj" @"%OUT%\objects.rsp" kernel32.lib user32.lib winmm.lib >"%OUT%\link.log" 2>&1
if errorlevel 1 (type "%OUT%\link.log"& exit /b 1)
rem Argument tracing legitimately reads arguments; this fixture tests the callback itself.
set "BF2_SWIFFPLAYER_ASLOG="
set "BF2_SWIFFPLAYER_STREAMLOG="
"%OUT%\swiff_disconnect_runtime.exe" >"%OUT%\runtime.log" 2>&1
set "RESULT=%errorlevel%"
type "%OUT%\runtime.log"
exit /b %RESULT%
