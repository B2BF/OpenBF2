@echo off
rem [TEMP] fourway comparison: one client run against the C:\rendsrv server (127.0.0.1:16890).
rem usage: cli.cmd ret|x86|x64 px|is join|menu TAG SCRIPT
rem   px  the dinput8 proxy (C:\rendfw\proxy86|64) drives the client: DI8PROXY_SCRIPT=SCRIPT, wait-log
rem       on the server's playtest_pose.log; the proxy is copied next to a rebuilt client for the run only
rem   is  the rebuilt client's own BF2_INPUT_SCRIPT=SCRIPT (with BF2_PLAY_TRACE=1; wait-log on stdout)
rem Every run starts from a fresh copy of C:\rendfw\home-template. Captures, logs: C:\rendfw\shots\TAG.
rem Run through the rendfw_cli task (interactive session).
setlocal
set "K=%~1"
set "MODE=%~2"
set "JOIN=%~3"
set "T=%~4"
set "SC=%~5"
set "OUTD=C:\rendfw\shots\%T%"
if exist "%OUTD%" rmdir /s /q "%OUTD%"
mkdir "%OUTD%"
set "USERPROFILE=C:\rendfw\home-%K%"
if exist "%USERPROFILE%" rmdir /s /q "%USERPROFILE%"
robocopy C:\rendfw\home-template "%USERPROFILE%" /E /NFL /NDL /NJH /NJS /NP >nul
rem DPI: every compared exe path has the per-user layer "~ HIGHDPIAWARE" (HKCU AppCompatFlags\Layers), so all windows are 800x600 1:1.
if /i "%K%"=="ret" (set "DIR=C:\rendret" & set "PX=C:\rendfw\proxy86")
if /i "%K%"=="x86" (set "DIR=C:\rendx86" & set "PX=C:\rendfw\proxy86")
if /i "%K%"=="x64" (set "DIR=C:\rendx64" & set "PX=C:\rendfw\proxy64")
set "ARGS=+modPath mods/bf2 +menu 1 +fullscreen 0 +szx 800 +szy 600 +restart 1"
if /i "%JOIN%"=="join" set "ARGS=%ARGS% +joinServer 127.0.0.1 +port 16890"
set "ARGS=%ARGS% +playerName LoopBot"
if /i "%MODE%"=="px" (
  copy /y "%PX%\dinput8.dll" "%DIR%\dinput8.dll" >nul
  set "DI8PROXY_SCRIPT=%SC%"
  set "DI8PROXY_CAPTURE_DIR=%OUTD%"
  set "DI8PROXY_REPORT=%OUTD%\proxy.log"
  set "DI8PROXY_LOG=C:\rendsrv\mods\bf2\python\game\playtest_pose.log"
) else (
  set "BF2_INPUT_SCRIPT=%SC%"
  set "BF2_PRESENT_PNG=%OUTD%"
  set "BF2_PLAY_TRACE=1"
)
copy /y "%SC%" "%OUTD%\script.txt" >nul
cd /d "%DIR%\root"
echo [run] %DATE% %TIME% %K% %MODE% %JOIN% %SC% > "%OUTD%\client.out"
"%DIR%\BF2.exe" %ARGS% >> "%OUTD%\client.out" 2>&1
echo [exit] %errorlevel% %DATE% %TIME% >> "%OUTD%\client.out"
if /i "%MODE%"=="px" if /i not "%K%"=="ret" del "%DIR%\dinput8.dll"
echo done > "%OUTD%\done.txt"
exit /b 0
