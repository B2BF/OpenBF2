@rem [TEMP] Canonical Soldier dispatch regression; strict link, no game or server.
@echo off
setlocal
call "%~dp0..\Build\vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%~dp0..\Build\obj_spawnpoint_soldier_tests"
python "%~dp0spawnpoint_soldier_dispatch_runtime.py" "%BF2_BUILD_OUT%" %*
exit /b %errorlevel%
