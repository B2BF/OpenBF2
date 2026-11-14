@echo off
setlocal
rem [TEMP] Actual lifetime, native prefix and named caller diagnostics; strict by default.
rem Pass discovery for real filesystem/console constructors with typed observers;
rem only that additional mode permits uncalled /FORCE dependencies.
call "%~dp0..\Build\vsdevcmd.cmd" x86
set "TASK_ROOT=%~dp0..\BF2"
set "TASK_MODE=lifetime"
set "TASK_DISCOVERY_FLAGS="
set "TASK_EXTERNAL_TUS="
set "TASK_LINK_MODE="
if /i "%~1"=="discovery" (
  set "TASK_MODE=discovery"
  set "TASK_DISCOVERY_FLAGS=/DBF2_PROFILE_DISCOVERY_FIXTURE=1"
  set "TASK_EXTERNAL_TUS=IO\File\FileManager IO\Console\Console"
  set "TASK_LINK_MODE=/FORCE:UNRESOLVED"
)
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%~dp0..\Build\local_profile_validation_20260906\runtime_%TASK_MODE%"
if not defined BF2_TEST_OPT set "BF2_TEST_OPT=/O2"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"
set TASK_FLAGS=/nologo %BF2_TEST_OPT% /EHsc /std:c++14 /permissive /MD /Z7 /Gy /DDEDICATED=0 /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0 %TASK_DISCOVERY_FLAGS% /I"%TASK_ROOT%\External\zlib-1.2.3" /I"%TASK_ROOT%\External\zlib-1.2.3\contrib\minizip"
type nul >"%BF2_BUILD_OUT%\compile.log"
type nul >"%BF2_BUILD_OUT%\objects.rsp"
for %%F in (Game\LocalProfileManager Game\LocalProfileManagerInit Game\LocalProfileManagerOperations Game\LocalProfileManagerSelection Game\LocalProfileManagerDiscovery Game\LocalProfileManagerFields Game\LocalProfileManagerSuffix Game\PasswordCodec Game\MapList Core\ClassManager Core\EventManager Core\Debug Core\StringUtil Core\SettingsRepostitory %TASK_EXTERNAL_TUS%) do (
  cl %TASK_FLAGS% /FI"%TASK_ROOT%\Core\BuildConfig.h" /c "%TASK_ROOT%\%%F.cpp" /Fo"%BF2_BUILD_OUT%\%%~nF.obj" >>"%BF2_BUILD_OUT%\compile.log" 2>&1
  if errorlevel 1 (type "%BF2_BUILD_OUT%\compile.log" & exit /b 1)
  echo "%BF2_BUILD_OUT%\%%~nF.obj" >>"%BF2_BUILD_OUT%\objects.rsp"
)
cl %TASK_FLAGS% /FI"%TASK_ROOT%\Core\BuildConfig.h" /c "%~dp0local_profile_lifetime_runtime.cpp" /Fo"%BF2_BUILD_OUT%\fixture.obj" >>"%BF2_BUILD_OUT%\compile.log" 2>&1
if errorlevel 1 (type "%BF2_BUILD_OUT%\compile.log" & exit /b 1)
link /nologo %TASK_LINK_MODE% /OPT:REF /INCREMENTAL:NO /DEBUG /SUBSYSTEM:CONSOLE /OUT:"%BF2_BUILD_OUT%\local_profile_lifetime_runtime.exe" /PDB:"%BF2_BUILD_OUT%\fixture.pdb" /MAP:"%BF2_BUILD_OUT%\fixture.map" @"%BF2_BUILD_OUT%\objects.rsp" "%BF2_BUILD_OUT%\fixture.obj" kernel32.lib user32.lib crypt32.lib >"%BF2_BUILD_OUT%\link.log" 2>&1
if errorlevel 1 (type "%BF2_BUILD_OUT%\link.log" & exit /b 1)
"%BF2_BUILD_OUT%\local_profile_lifetime_runtime.exe" >"%BF2_BUILD_OUT%\runtime.log" 2>&1
set "TASK_RESULT=%errorlevel%"
type "%BF2_BUILD_OUT%\runtime.log"
rem A run without its result line proves nothing (a crash can also leave errorlevel 0 here): fail it.
findstr /R /C:"^local profile lifetime [0-9]*/[0-9]* checks passed" "%BF2_BUILD_OUT%\runtime.log" >nul
if errorlevel 1 (echo FAIL: no "local profile lifetime N/N checks passed" line in runtime.log & exit /b 1)
exit /b %TASK_RESULT%
