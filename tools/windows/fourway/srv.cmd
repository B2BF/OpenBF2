@echo off
rem [TEMP] fourway comparison: (re)start the retail dedicated server in C:\rendsrv with job files that
rem fourway.py put in C:\rendfw\srv\JOB\ (ServerSettings.con, maplist.con, playtest_pose.cfg, ai.txt).
rem usage: srv.cmd JOB      Port 16890 (never 16567). Run through the rendfw_srv task (interactive session).
setlocal
set "J=C:\rendfw\srv\%~1"
set "GAME=C:\rendsrv\mods\bf2\python\game"
powershell -NoProfile -Command "Get-Process bf2_w32ded -ErrorAction SilentlyContinue | Where-Object { $_.Path -like 'C:\rendsrv\*' } | Stop-Process -Force"
ping -n 3 127.0.0.1 >nul
rem playtest_pose.py: copied into C:\rendfw by "fourway.py push" from the worktree that drives the run
copy /y C:\rendfw\playtest_pose.py "%GAME%\playtest_pose.py" >nul
copy /y "%J%\playtest_pose.cfg" "%GAME%\playtest_pose.cfg" >nul
findstr /c:"import playtest_pose" "%GAME%\__init__.py" >nul 2>&1 || (echo.& echo import playtest_pose) >> "%GAME%\__init__.py"
if exist "%GAME%\playtest_pose.log" move /y "%GAME%\playtest_pose.log" "%J%\playtest_pose.prev.log" >nul
set /p AI=<"%J%\ai.txt"
cd /d C:\rendsrv
echo [srv] %DATE% %TIME% job=%~1 ai=%AI% > "%J%\srv.out"
start "rendfw-server" C:\rendsrv\bf2_w32ded.exe +modPath mods/bf2 +config "%J%\ServerSettings.con" +mapList "%J%\maplist.con" +ai %AI% +noStatusMonitor 1
exit /b 0
