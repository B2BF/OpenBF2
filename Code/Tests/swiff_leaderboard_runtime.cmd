@echo off
setlocal EnableDelayedExpansion
rem [TEMP] The SwiffHost leaderboard / awards cache fixture (swiff_leaderboard_runtime.cpp): Menu\SwiffHost.cpp
rem with the REAL LeaderBoard, PlayerAwards, HTTPRequest, KeyParser and the GameSpy SDK set CMake's
rem bf2_gamespy_objects compiles, against the local stats fixture on 127.0.0.1:18124 (never 16567).
rem Linked /FORCE:UNRESOLVED: SwiffHost's other collaborators stay unresolved and are never reached; the
rem list of them is printed after the run. Client image (DEDICATED=0).
call "%~dp0..\Build\vsdevcmd.cmd" x86
set "TASK_ROOT=%~dp0..\BF2"
set "TASK_GS=%~dp0..\BF2\External\GameSpy"
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%~dp0..\Build\swiff_phase4_20260929\leaderboard_runtime"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"
set "TASK_PY=%~dp0..\BF2\External\python-embedded\py-src-2.3.4"
set TASK_FLAGS=/nologo /O2 /EHsc /DPy_NO_ENABLE_SHARED /I"%TASK_PY%\Include" /I"%TASK_PY%\PC" /std:c++14 /permissive /MD /Z7 /Gy /DDEDICATED=0 /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0 /D_CRT_SECURE_NO_WARNINGS /FI"%TASK_ROOT%\Core\BuildConfig.h"
set TASK_CFLAGS=/nologo /O2 /MD /Z7 /W0 /DDEDICATED=0 /D_CRT_SECURE_NO_WARNINGS /FI"%TASK_ROOT%\Core\BuildConfig.h"
type nul >"%BF2_BUILD_OUT%\compile.log"
set TASK_OBJS=
for %%F in ("Menu\SwiffHost.cpp" "Game\Persistence\LeaderBoard.cpp" "Game\Persistence\PlayerAwards.cpp" "Game\Persistence\HTTPRequest.cpp" "Game\MatchMaking\OnlineProvider.cpp" "Game\Persistence\KeyParser.cpp" "Core\StringUtil.cpp") do (
  cl %TASK_FLAGS% /c "%TASK_ROOT%\%%~F" /Fo"%BF2_BUILD_OUT%\%%~nF.obj" >>"%BF2_BUILD_OUT%\compile.log" 2>&1
  if errorlevel 1 goto :compilefail
  set TASK_OBJS=!TASK_OBJS! "%BF2_BUILD_OUT%\%%~nF.obj"
)
cl %TASK_FLAGS% /c "%~dp0swiff_leaderboard_runtime.cpp" /Fo"%BF2_BUILD_OUT%\fixture.obj" >>"%BF2_BUILD_OUT%\compile.log" 2>&1
if errorlevel 1 goto :compilefail
for %%F in ("gcdkey\gcdkeys.c" "darray.c" "common\gsAvailable.c" "common\gsPlatformUtil.c" "common\gsCore.c" "common\gsMemory.c" "common\gsDebug.c" "common\gsAssert.c" "common\gsPlatform.c" "common\gsPlatformSocket.c" "common\gsCrypt.c" "common\gsRC4.c" "common\gsSHA1.c" "common\gsLargeInt.c" "common\gsPlatformThread.c" "common\gsXML.c" "common\gsSSL.c" "common\gsStringUtil.c" "md5c.c" "gstats\gstats.c" "gstats\gbucket.c" "qr2\qr2.c" "qr2\qr2regkeys.c" "hashtable.c" "ghttp\ghttpBuffer.c" "ghttp\ghttpCallbacks.c" "ghttp\ghttpCommon.c" "ghttp\ghttpConnection.c" "ghttp\ghttpEncryption.c" "ghttp\ghttpMain.c" "ghttp\ghttpPost.c" "ghttp\ghttpProcess.c" "natneg\natneg.c" "natneg\NATify.c") do (
  cl %TASK_CFLAGS% /c "%TASK_GS%\%%~F" /Fo"%BF2_BUILD_OUT%\gs_%%~nF.obj" >>"%BF2_BUILD_OUT%\compile.log" 2>&1
  if errorlevel 1 goto :compilefail
  set TASK_OBJS=!TASK_OBJS! "%BF2_BUILD_OUT%\gs_%%~nF.obj"
)
link /nologo /FORCE:UNRESOLVED /OPT:REF /INCREMENTAL:NO /DEBUG /SUBSYSTEM:CONSOLE /OUT:"%BF2_BUILD_OUT%\swiff_leaderboard_runtime.exe" /PDB:"%BF2_BUILD_OUT%\fixture.pdb" /MAP:"%BF2_BUILD_OUT%\fixture.map" "%BF2_BUILD_OUT%\fixture.obj" %TASK_OBJS% ws2_32.lib advapi32.lib user32.lib shell32.lib winmm.lib crypt32.lib kernel32.lib >"%BF2_BUILD_OUT%\link.log" 2>&1
findstr /C:"unresolved external symbol" "%BF2_BUILD_OUT%\link.log" >"%BF2_BUILD_OUT%\unresolved.log"
if not exist "%BF2_BUILD_OUT%\swiff_leaderboard_runtime.exe" (type "%BF2_BUILD_OUT%\link.log" & goto :fail)
"%BF2_BUILD_OUT%\swiff_leaderboard_runtime.exe" "%~dp0stats_fixture" >"%BF2_BUILD_OUT%\runtime.log" 2>&1
set "TASK_RESULT=%errorlevel%"
type "%BF2_BUILD_OUT%\runtime.log"
for /f %%N in ('find /c /v "" ^<"%BF2_BUILD_OUT%\unresolved.log"') do echo forced unresolved externals: %%N (listed in unresolved.log)
if not "%TASK_RESULT%"=="0" goto :fail
echo SWIFF-LEADERBOARD-RUNTIME OK
exit /b 0
:compilefail
type "%BF2_BUILD_OUT%\compile.log" | findstr /I /C:"error"
:fail
echo SWIFF-LEADERBOARD-RUNTIME FAILED
exit /b 1
