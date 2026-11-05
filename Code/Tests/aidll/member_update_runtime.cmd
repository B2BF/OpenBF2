@echo off
setlocal
rem [TEMP] Exact member caller with real headers/pool; two position-helper variants.
call "%~dp0..\..\Build\vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1
set "TASK_CODE=%~dp0..\.."
if not defined BF2_MEMBER_UPDATE_OUT set "BF2_MEMBER_UPDATE_OUT=%TASK_CODE%\Build\ai_member_update_20260927\validation"
if not exist "%BF2_MEMBER_UPDATE_OUT%" mkdir "%BF2_MEMBER_UPDATE_OUT%"
python -I "%~dp0member_update_runtime.py" generate "%BF2_MEMBER_UPDATE_OUT%"
if errorlevel 1 exit /b 1
set "TASK_FLAGS=/nologo /c /O2 /MD /std:c++14 /permissive /EHsc /W3 /Gy /DDEDICATED=1 /DBF2_IMAGE_AIDLL"
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\Main\InformationManager.cpp" /Fo"%BF2_MEMBER_UPDATE_OUT%\manager.obj" >"%BF2_MEMBER_UPDATE_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%BF2_MEMBER_UPDATE_OUT%\actual.cpp" /Fo"%BF2_MEMBER_UPDATE_OUT%\actual.obj" >>"%BF2_MEMBER_UPDATE_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /I"%BF2_MEMBER_UPDATE_OUT%" /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%~dp0member_update_runtime.cpp" /Fo"%BF2_MEMBER_UPDATE_OUT%\fixture.obj" >>"%BF2_MEMBER_UPDATE_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
link /nologo /OPT:REF /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /MANIFEST:EMBED /MANIFESTUAC:"level='asInvoker' uiAccess='false'" /OUT:"%BF2_MEMBER_UPDATE_OUT%\member_update_runtime.exe" "%BF2_MEMBER_UPDATE_OUT%\actual.obj" "%BF2_MEMBER_UPDATE_OUT%\fixture.obj" "%BF2_MEMBER_UPDATE_OUT%\manager.obj" >"%BF2_MEMBER_UPDATE_OUT%\link.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /DREAL_POSITION /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%BF2_MEMBER_UPDATE_OUT%\actual.cpp" /Fo"%BF2_MEMBER_UPDATE_OUT%\real_actual.obj" >>"%BF2_MEMBER_UPDATE_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /DREAL_POSITION /I"%BF2_MEMBER_UPDATE_OUT%" /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%~dp0member_update_runtime.cpp" /Fo"%BF2_MEMBER_UPDATE_OUT%\real_fixture.obj" >>"%BF2_MEMBER_UPDATE_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
link /nologo /OPT:REF /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /MANIFEST:EMBED /MANIFESTUAC:"level='asInvoker' uiAccess='false'" /OUT:"%BF2_MEMBER_UPDATE_OUT%\real_position.exe" "%BF2_MEMBER_UPDATE_OUT%\real_actual.obj" "%BF2_MEMBER_UPDATE_OUT%\real_fixture.obj" "%BF2_MEMBER_UPDATE_OUT%\manager.obj" >>"%BF2_MEMBER_UPDATE_OUT%\link.log" 2>&1
if errorlevel 1 goto failed
python -I "%~dp0member_update_runtime.py" run "%BF2_MEMBER_UPDATE_OUT%"
exit /b %errorlevel%
:failed
type "%BF2_MEMBER_UPDATE_OUT%\compile.log"
if exist "%BF2_MEMBER_UPDATE_OUT%\link.log" type "%BF2_MEMBER_UPDATE_OUT%\link.log"
exit /b 1
