@echo off
setlocal
rem [TEMP] Actual DemoLibrary owner, refresh, entries and the zlib stream pair over the vendored
rem zlib. The file manager observer derives from the real FileManager, whose unreached members are
rem forced (/FORCE:UNRESOLVED, listed in link.log); nothing DemoLibrary owns is shadowed. The
rem HTTPRequest base is real; the fixture stands in for the four GameSpy HTTP calls it reaches.
call "%~dp0..\Build\vsdevcmd.cmd" x86
set "TASK_ROOT=%~dp0..\BF2"
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%~dp0..\Build\demo_library_20260908\runtime_demo"
if not defined BF2_TEST_OPT set "BF2_TEST_OPT=/O2"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"
set "ZIPINC=/I"%TASK_ROOT%\External\zlib-1.2.3" /I"%TASK_ROOT%\External\zlib-1.2.3\contrib\minizip""
set TASK_FLAGS=/nologo %BF2_TEST_OPT% /EHsc /std:c++14 /permissive /MD /Z7 /Gy /DDEDICATED=0 /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0 %ZIPINC%
set CFLAGS=/nologo %BF2_TEST_OPT% /MD /Z7 /W3 /wd4996 %ZIPINC% /DDEDICATED=0 /FI"%TASK_ROOT%\Core\BuildConfig.h"
type nul >"%BF2_BUILD_OUT%\compile.log"
type nul >"%BF2_BUILD_OUT%\objects.rsp"
for %%F in (Game\DemoRecorder\DemoLibrary Game\Persistence\HTTPRequest Game\DemoRecorder\DemoHeader IO\Stream\ZStream IO\ZUtil IO\File\FileManager Core\ClassManager Core\EventManager Core\Debug Core\StringUtil) do (
  cl %TASK_FLAGS% /FI"%TASK_ROOT%\Core\BuildConfig.h" /c "%TASK_ROOT%\%%F.cpp" /Fo"%BF2_BUILD_OUT%\%%~nF.obj" >>"%BF2_BUILD_OUT%\compile.log" 2>&1
  if errorlevel 1 (type "%BF2_BUILD_OUT%\compile.log" & exit /b 1)
  echo "%BF2_BUILD_OUT%\%%~nF.obj" >>"%BF2_BUILD_OUT%\objects.rsp"
)
for %%F in (adler32 crc32 inffast inflate inftrees zutil deflate trees) do (
  cl %CFLAGS% /c "%TASK_ROOT%\External\zlib-1.2.3\%%F.c" /Fo"%BF2_BUILD_OUT%\z_%%F.obj" >>"%BF2_BUILD_OUT%\compile.log" 2>&1
  if errorlevel 1 (type "%BF2_BUILD_OUT%\compile.log" & exit /b 1)
  echo "%BF2_BUILD_OUT%\z_%%F.obj" >>"%BF2_BUILD_OUT%\objects.rsp"
)
cl %TASK_FLAGS% /FI"%TASK_ROOT%\Core\BuildConfig.h" /c "%~dp0demo_library_runtime.cpp" /Fo"%BF2_BUILD_OUT%\fixture.obj" >>"%BF2_BUILD_OUT%\compile.log" 2>&1
if errorlevel 1 (type "%BF2_BUILD_OUT%\compile.log" & exit /b 1)
link /nologo /FORCE:UNRESOLVED /OPT:REF /INCREMENTAL:NO /DEBUG /SUBSYSTEM:CONSOLE /OUT:"%BF2_BUILD_OUT%\demo_library_runtime.exe" /PDB:"%BF2_BUILD_OUT%\fixture.pdb" /MAP:"%BF2_BUILD_OUT%\fixture.map" @"%BF2_BUILD_OUT%\objects.rsp" "%BF2_BUILD_OUT%\fixture.obj" kernel32.lib user32.lib >"%BF2_BUILD_OUT%\link.log" 2>&1
if errorlevel 1 (type "%BF2_BUILD_OUT%\link.log" & exit /b 1)
"%BF2_BUILD_OUT%\demo_library_runtime.exe" >"%BF2_BUILD_OUT%\runtime.log" 2>&1
set "TASK_RESULT=%errorlevel%"
type "%BF2_BUILD_OUT%\runtime.log"
exit /b %TASK_RESULT%
