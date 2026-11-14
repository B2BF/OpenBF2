@echo off
rem [TEMP] fourway: the retail server under cdb (debug output and the faulting stack into C:\rendfw\srv\JOB\cdb.log).
setlocal
set "J=C:\rendfw\srv\%~1"
set /p AI=<"%J%\ai.txt"
cd /d C:\rendsrv
"C:\Program Files (x86)\Windows Kits\10\Debuggers\x86\cdb.exe" -g -logo "%J%\cdb.log" -c "kb 20;q" C:\rendsrv\bf2_w32ded.exe +modPath mods/bf2 +config "%J%\ServerSettings.con" +mapList "%J%\maplist.con" +ai %AI% +noStatusMonitor 1
