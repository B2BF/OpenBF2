@echo off
setlocal
rem [TEMP] Strict MSVC x86; exact plan-executor/stall bodies, real pool and PlayerInput (2026-09-28).
call "%~dp0..\..\Build\vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1
set "TASK_CODE=%~dp0..\.."
if not defined BF2_PLAN_EXECUTOR_OUT set "BF2_PLAN_EXECUTOR_OUT=%TASK_CODE%\Build\claude_ai_plan_20260928\plan_executor"
if not exist "%BF2_PLAN_EXECUTOR_OUT%" mkdir "%BF2_PLAN_EXECUTOR_OUT%"
python -I "%~dp0plan_executor_runtime.py" generate "%BF2_PLAN_EXECUTOR_OUT%"
if errorlevel 1 exit /b 1
set "TASK_FLAGS=/nologo /c /O2 /MD /std:c++14 /permissive /EHsc /W3 /Gy /DDEDICATED=1 /DBF2_IMAGE_AIDLL"
set "TASK_FI=/FI"%TASK_CODE%\BF2\Core\BuildConfig.h""
cl %TASK_FLAGS% %TASK_FI% "%TASK_CODE%\BF2\AI\AIDLL\AI\Main\InformationManager.cpp" /Fo"%BF2_PLAN_EXECUTOR_OUT%\manager.obj" >"%BF2_PLAN_EXECUTOR_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% %TASK_FI% "%TASK_CODE%\BF2\IO\Input\PlayerInput.cpp" /Fo"%BF2_PLAN_EXECUTOR_OUT%\input.obj" >>"%BF2_PLAN_EXECUTOR_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% %TASK_FI% "%TASK_CODE%\BF2\Core\Debug.cpp" /Fo"%BF2_PLAN_EXECUTOR_OUT%\debug.obj" >>"%BF2_PLAN_EXECUTOR_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% %TASK_FI% "%TASK_CODE%\BF2\Core\StringUtil.cpp" /Fo"%BF2_PLAN_EXECUTOR_OUT%\stringutil.obj" >>"%BF2_PLAN_EXECUTOR_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% %TASK_FI% "%BF2_PLAN_EXECUTOR_OUT%\actual_manager.cpp" /Fo"%BF2_PLAN_EXECUTOR_OUT%\actual_manager.obj" >>"%BF2_PLAN_EXECUTOR_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% %TASK_FI% "%BF2_PLAN_EXECUTOR_OUT%\actual_main.cpp" /Fo"%BF2_PLAN_EXECUTOR_OUT%\actual_main.obj" >>"%BF2_PLAN_EXECUTOR_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /I"%BF2_PLAN_EXECUTOR_OUT%" %TASK_FI% "%~dp0plan_executor_runtime.cpp" /Fo"%BF2_PLAN_EXECUTOR_OUT%\fixture.obj" >>"%BF2_PLAN_EXECUTOR_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
link /nologo /OPT:REF /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /OUT:"%BF2_PLAN_EXECUTOR_OUT%\plan_executor_runtime.exe" "%BF2_PLAN_EXECUTOR_OUT%\actual_manager.obj" "%BF2_PLAN_EXECUTOR_OUT%\actual_main.obj" "%BF2_PLAN_EXECUTOR_OUT%\fixture.obj" "%BF2_PLAN_EXECUTOR_OUT%\manager.obj" "%BF2_PLAN_EXECUTOR_OUT%\input.obj" "%BF2_PLAN_EXECUTOR_OUT%\debug.obj" "%BF2_PLAN_EXECUTOR_OUT%\stringutil.obj" >"%BF2_PLAN_EXECUTOR_OUT%\link.log" 2>&1
if errorlevel 1 goto failed
python -I "%~dp0plan_executor_runtime.py" run "%BF2_PLAN_EXECUTOR_OUT%"
exit /b %errorlevel%
:failed
type "%BF2_PLAN_EXECUTOR_OUT%\compile.log"
if exist "%BF2_PLAN_EXECUTOR_OUT%\link.log" type "%BF2_PLAN_EXECUTOR_OUT%\link.log"
exit /b 1
