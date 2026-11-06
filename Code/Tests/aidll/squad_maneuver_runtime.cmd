@echo off
setlocal
rem [TEMP] Exact chooser/dispatcher and real leaves; separate dispatcher observer variant.
call "%~dp0..\..\Build\vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1
set "TASK_CODE=%~dp0..\.."
if not defined BF2_SQUAD_MANEUVER_OUT set "BF2_SQUAD_MANEUVER_OUT=%TASK_CODE%\Build\ai_squad_maneuver_20260927\validation"
if not exist "%BF2_SQUAD_MANEUVER_OUT%" mkdir "%BF2_SQUAD_MANEUVER_OUT%"
python -I "%~dp0squad_maneuver_runtime.py" generate "%BF2_SQUAD_MANEUVER_OUT%"
if errorlevel 1 exit /b 1
set "TASK_FLAGS=/nologo /c /O2 /MD /std:c++14 /permissive /EHsc /W3 /Gy /DDEDICATED=1 /DBF2_IMAGE_AIDLL"
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\Main\InformationManager.cpp" /Fo"%BF2_SQUAD_MANEUVER_OUT%\manager.obj" >"%BF2_SQUAD_MANEUVER_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%BF2_SQUAD_MANEUVER_OUT%\actual.cpp" /Fo"%BF2_SQUAD_MANEUVER_OUT%\actual.obj" >>"%BF2_SQUAD_MANEUVER_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /I"%BF2_SQUAD_MANEUVER_OUT%" /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%~dp0squad_maneuver_runtime.cpp" /Fo"%BF2_SQUAD_MANEUVER_OUT%\fixture.obj" >>"%BF2_SQUAD_MANEUVER_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
link /nologo /OPT:REF /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /MANIFEST:EMBED /MANIFESTUAC:"level='asInvoker' uiAccess='false'" /OUT:"%BF2_SQUAD_MANEUVER_OUT%\squad_maneuver_runtime.exe" "%BF2_SQUAD_MANEUVER_OUT%\actual.obj" "%BF2_SQUAD_MANEUVER_OUT%\fixture.obj" "%BF2_SQUAD_MANEUVER_OUT%\manager.obj" >"%BF2_SQUAD_MANEUVER_OUT%\link.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /DDISPATCH_OBSERVERS /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%BF2_SQUAD_MANEUVER_OUT%\actual.cpp" /Fo"%BF2_SQUAD_MANEUVER_OUT%\observer_actual.obj" >>"%BF2_SQUAD_MANEUVER_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /DDISPATCH_OBSERVERS /I"%BF2_SQUAD_MANEUVER_OUT%" /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%~dp0squad_maneuver_runtime.cpp" /Fo"%BF2_SQUAD_MANEUVER_OUT%\observer_fixture.obj" >>"%BF2_SQUAD_MANEUVER_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
link /nologo /OPT:REF /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /MANIFEST:EMBED /MANIFESTUAC:"level='asInvoker' uiAccess='false'" /OUT:"%BF2_SQUAD_MANEUVER_OUT%\dispatcher_observers.exe" "%BF2_SQUAD_MANEUVER_OUT%\observer_actual.obj" "%BF2_SQUAD_MANEUVER_OUT%\observer_fixture.obj" "%BF2_SQUAD_MANEUVER_OUT%\manager.obj" >>"%BF2_SQUAD_MANEUVER_OUT%\link.log" 2>&1
if errorlevel 1 goto failed
python -I "%~dp0squad_maneuver_runtime.py" run "%BF2_SQUAD_MANEUVER_OUT%"
exit /b %errorlevel%
:failed
type "%BF2_SQUAD_MANEUVER_OUT%\compile.log"
if exist "%BF2_SQUAD_MANEUVER_OUT%\link.log" type "%BF2_SQUAD_MANEUVER_OUT%\link.log"
exit /b 1
