@echo off
setlocal
rem [TEMP] Verbatim helpers/caller, real information pool; other receivers/lifetimes chosen.
call "%~dp0..\..\Build\vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1
set "TASK_CODE=%~dp0..\.."
if not defined BF2_RANDOM_KIT_OUT set "BF2_RANDOM_KIT_OUT=%TASK_CODE%\Build\ai_kit_scoring_20260927\caller"
if not exist "%BF2_RANDOM_KIT_OUT%" mkdir "%BF2_RANDOM_KIT_OUT%"
python -I "%~dp0random_kit_runtime.py" generate "%BF2_RANDOM_KIT_OUT%"
if errorlevel 1 exit /b 1
set "TASK_FLAGS=/nologo /c /O2 /MD /std:c++14 /permissive /EHsc /W3 /Gy /DDEDICATED=1 /DBF2_IMAGE_AIDLL"
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%BF2_RANDOM_KIT_OUT%\random_kit_actual.cpp" /Fo"%BF2_RANDOM_KIT_OUT%\actual.obj" >"%BF2_RANDOM_KIT_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /I"%BF2_RANDOM_KIT_OUT%" /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%~dp0random_kit_runtime.cpp" /Fo"%BF2_RANDOM_KIT_OUT%\fixture.obj" >>"%BF2_RANDOM_KIT_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\Main\InformationManager.cpp" /Fo"%BF2_RANDOM_KIT_OUT%\pool.obj" >>"%BF2_RANDOM_KIT_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
link /nologo /OPT:REF /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /OUT:"%BF2_RANDOM_KIT_OUT%\random_kit_runtime.exe" "%BF2_RANDOM_KIT_OUT%\actual.obj" "%BF2_RANDOM_KIT_OUT%\fixture.obj" "%BF2_RANDOM_KIT_OUT%\pool.obj" >"%BF2_RANDOM_KIT_OUT%\link.log" 2>&1
if errorlevel 1 goto failed
python -I "%~dp0random_kit_runtime.py" run "%BF2_RANDOM_KIT_OUT%\random_kit_runtime.exe"
exit /b %errorlevel%
:failed
type "%BF2_RANDOM_KIT_OUT%\compile.log"
if exist "%BF2_RANDOM_KIT_OUT%\link.log" type "%BF2_RANDOM_KIT_OUT%\link.log"
exit /b 1
