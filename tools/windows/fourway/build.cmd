@echo off
rem [TEMP] fourway comparison: content sync from a Mac worktree into C:\rendbuild\src, then the
rem bf2_client builds (C:\rendbuild\build-x64 / build-x86) and staging into C:\rendx64 / C:\rendx86.
rem usage: build.cmd [SRC] [ARCHS]
rem   SRC    the worktree share, e.g. \\Mac\Home\<path of the worktree under the Mac home> (required)
rem   ARCHS  "x64 x86" (default), x86 or x64
rem Results: C:\rendbuild\logs\fwall.txt (step + exit code), fwsync.log, fwclient64.log, fwclient86.log.
setlocal
set "FW=%~dp0"
set "SRC=%~1"
if "%SRC%"=="" (echo build.cmd: pass the worktree share as SRC & exit /b 2)
set "ARCHS=%~2"
if "%ARCHS%"=="" set "ARCHS=x64 x86"
echo start %DATE% %TIME% src=%SRC% archs=%ARCHS% > C:\rendbuild\logs\fwall.txt
powershell -NoProfile -ExecutionPolicy Bypass -File "%FW%hsync.ps1" -src "%SRC%" -dst C:\rendbuild\src > C:\rendbuild\logs\fwsync.log 2>&1
echo sync %errorlevel% %TIME% >> C:\rendbuild\logs\fwall.txt
echo %ARCHS%| findstr /c:x64 >nul && cmd /c C:\rendbuild\w.cmd x64 fwcfg64 cmake --preset msvc-x64 -B C:\rendbuild\build-x64 "-DBF2_GAME_DIR=C:/Battlefield 2"
echo %ARCHS%| findstr /c:x64 >nul && cmd /c C:\rendbuild\w.cmd x64 fwclient64 cmake --build C:\rendbuild\build-x64 --target bf2_client -j %NUMBER_OF_PROCESSORS%
echo %ARCHS%| findstr /c:x64 >nul && echo b64 %errorlevel% %TIME% >> C:\rendbuild\logs\fwall.txt
echo %ARCHS%| findstr /c:x86 >nul && cmd /c C:\rendbuild\w.cmd x86 fwcfg86 cmake --preset msvc-x86 -B C:\rendbuild\build-x86 "-DBF2_GAME_DIR=C:/Battlefield 2"
echo %ARCHS%| findstr /c:x86 >nul && cmd /c C:\rendbuild\w.cmd x86 fwclient86 cmake --build C:\rendbuild\build-x86 --target bf2_client -j %NUMBER_OF_PROCESSORS%
echo %ARCHS%| findstr /c:x86 >nul && echo b86 %errorlevel% %TIME% >> C:\rendbuild\logs\fwall.txt
echo %ARCHS%| findstr /c:x64 >nul && cmd /c C:\rendbuild\rendx64_setup.cmd > C:\rendbuild\logs\fwstage64.log 2>&1
echo %ARCHS%| findstr /c:x86 >nul && cmd /c C:\rendbuild\rendx86_setup.cmd > C:\rendbuild\logs\fwstage86.log 2>&1
echo staged %TIME% >> C:\rendbuild\logs\fwall.txt
echo done >> C:\rendbuild\logs\fwall.txt
