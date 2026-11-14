@echo off
rem [TEMP] fourway: run a command line in the signed-in user's session (through the rendfw_user task) and
rem keep its output in C:\rendfw\asuser.out. usage (from fourway.py): the task runs asuser.cmd, which runs
rem C:\rendfw\asuser.txt (one command line).
set /p CMDLINE=<C:\rendfw\asuser.txt
cmd /c %CMDLINE% > C:\rendfw\asuser.out 2>&1
echo [done] >> C:\rendfw\asuser.out
