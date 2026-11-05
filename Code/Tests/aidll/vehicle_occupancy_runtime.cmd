@echo off
setlocal
rem [TEMP] Whole actual occupancy/manager TUs, strict MSVC x86; no /FORCE.
call "%~dp0..\..\Build\vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1
set "TASK_CODE=%~dp0..\.."
if not defined BF2_VEHICLE_OCCUPANCY_OUT set "BF2_VEHICLE_OCCUPANCY_OUT=%TASK_CODE%\Build\ai_kit_scan_20260927\occupancy\validation"
if not exist "%BF2_VEHICLE_OCCUPANCY_OUT%" mkdir "%BF2_VEHICLE_OCCUPANCY_OUT%"
python -I "%~dp0vehicle_occupancy_runtime.py" record "%BF2_VEHICLE_OCCUPANCY_OUT%"
if errorlevel 1 exit /b 1
set "TASK_FLAGS=/nologo /c /O2 /MD /std:c++14 /permissive /EHsc /W3 /Gy /DDEDICATED=1 /DBF2_IMAGE_AIDLL"
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" /FAs /Fa"%BF2_VEHICLE_OCCUPANCY_OUT%\actual.asm" "%TASK_CODE%\BF2\AI\AIDLL\AI\AIUtils\VehicleOccupancy.cpp" /Fo"%BF2_VEHICLE_OCCUPANCY_OUT%\actual.obj" >"%BF2_VEHICLE_OCCUPANCY_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\Main\InformationManager.cpp" /Fo"%BF2_VEHICLE_OCCUPANCY_OUT%\manager.obj" >>"%BF2_VEHICLE_OCCUPANCY_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%~dp0vehicle_occupancy_runtime.cpp" /Fo"%BF2_VEHICLE_OCCUPANCY_OUT%\fixture.obj" >>"%BF2_VEHICLE_OCCUPANCY_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
link /nologo /OPT:REF /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /OUT:"%BF2_VEHICLE_OCCUPANCY_OUT%\vehicle_occupancy_runtime.exe" "%BF2_VEHICLE_OCCUPANCY_OUT%\actual.obj" "%BF2_VEHICLE_OCCUPANCY_OUT%\manager.obj" "%BF2_VEHICLE_OCCUPANCY_OUT%\fixture.obj" >"%BF2_VEHICLE_OCCUPANCY_OUT%\link.log" 2>&1
if errorlevel 1 goto failed
python -I "%~dp0vehicle_occupancy_runtime.py" run "%BF2_VEHICLE_OCCUPANCY_OUT%\vehicle_occupancy_runtime.exe"
exit /b %errorlevel%
:failed
type "%BF2_VEHICLE_OCCUPANCY_OUT%\compile.log"
if exist "%BF2_VEHICLE_OCCUPANCY_OUT%\link.log" type "%BF2_VEHICLE_OCCUPANCY_OUT%\link.log"
exit /b 1
