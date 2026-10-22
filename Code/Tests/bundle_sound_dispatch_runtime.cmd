@rem [TEMP] Canonical bundle/sound interface regression; strict link, no server launch.
@echo off
setlocal
call "%~dp0..\Build\vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%~dp0..\Build\obj_bundle_sound_dispatch_tests"
python "%~dp0bundle_sound_dispatch_runtime.py" "%BF2_BUILD_OUT%" %*
exit /b %errorlevel%
