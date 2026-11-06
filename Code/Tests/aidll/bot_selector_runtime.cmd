@echo off
setlocal
rem [TEMP] Exact selector, native chosen lifetimes and typed observers, strict x86.
call "%~dp0..\..\Build\vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1
set "TASK_CODE=%~dp0..\.."
if not defined BF2_BOT_SELECTOR_OUT set "BF2_BOT_SELECTOR_OUT=%TASK_CODE%\Build\ai_radius_20260927\selector_regression"
if not exist "%BF2_BOT_SELECTOR_OUT%" mkdir "%BF2_BOT_SELECTOR_OUT%"
python -I "%~dp0bot_selector_runtime.py" generate "%BF2_BOT_SELECTOR_OUT%"
if errorlevel 1 exit /b 1
set "TASK_FLAGS=/nologo /c /O2 /MD /std:c++14 /permissive /EHsc /W3 /Gy /DDEDICATED=1 /DBF2_IMAGE_AIDLL"
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%BF2_BOT_SELECTOR_OUT%\bot_selector_actual.cpp" /Fo"%BF2_BOT_SELECTOR_OUT%\actual.obj" >"%BF2_BOT_SELECTOR_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /I"%BF2_BOT_SELECTOR_OUT%" /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%~dp0bot_selector_runtime.cpp" /Fo"%BF2_BOT_SELECTOR_OUT%\fixture.obj" >>"%BF2_BOT_SELECTOR_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
for %%F in (Information IPIUnit WeaponInfoManager) do (
  cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\AIKnowledge\%%F.cpp" /Fo"%BF2_BOT_SELECTOR_OUT%\%%F.obj" >>"%BF2_BOT_SELECTOR_OUT%\compile.log" 2>&1
  if errorlevel 1 goto failed
)
rem IPIUnit's recovered nonvirtual occupancy helpers require its whole implementation and helpers.
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\AIUtils\CustomCurve.cpp" /Fo"%BF2_BOT_SELECTOR_OUT%\CustomCurve.obj" >>"%BF2_BOT_SELECTOR_OUT%\compile.log" 2>&1
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\AIUtils\SCurve.cpp" /Fo"%BF2_BOT_SELECTOR_OUT%\SCurve.obj" >>"%BF2_BOT_SELECTOR_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
for %%F in (Debug StringUtil) do (
  cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\Core\%%F.cpp" /Fo"%BF2_BOT_SELECTOR_OUT%\%%F.obj" >>"%BF2_BOT_SELECTOR_OUT%\compile.log" 2>&1
  if errorlevel 1 goto failed
)
link /nologo /OPT:REF /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /OUT:"%BF2_BOT_SELECTOR_OUT%\bot_selector_runtime.exe" "%BF2_BOT_SELECTOR_OUT%\actual.obj" "%BF2_BOT_SELECTOR_OUT%\fixture.obj" "%BF2_BOT_SELECTOR_OUT%\Information.obj" "%BF2_BOT_SELECTOR_OUT%\IPIUnit.obj" "%BF2_BOT_SELECTOR_OUT%\WeaponInfoManager.obj" "%BF2_BOT_SELECTOR_OUT%\CustomCurve.obj" "%BF2_BOT_SELECTOR_OUT%\SCurve.obj" "%BF2_BOT_SELECTOR_OUT%\Debug.obj" "%BF2_BOT_SELECTOR_OUT%\StringUtil.obj" >"%BF2_BOT_SELECTOR_OUT%\link.log" 2>&1
if errorlevel 1 goto failed
python -I "%~dp0bot_selector_runtime.py" run "%BF2_BOT_SELECTOR_OUT%\bot_selector_runtime.exe"
exit /b %errorlevel%
:failed
type "%BF2_BOT_SELECTOR_OUT%\compile.log"
if exist "%BF2_BOT_SELECTOR_OUT%\link.log" type "%BF2_BOT_SELECTOR_OUT%\link.log"
exit /b 1
