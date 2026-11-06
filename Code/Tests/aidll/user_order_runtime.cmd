@echo off
setlocal
rem [TEMP] [CHOSEN, not recovered] Real UserOrderManager.cpp, strict isolated x86 runtime fixture.
call "%~dp0..\..\Build\vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1
set "TASK_CODE=%~dp0..\.."
if not defined BF2_USER_ORDER_OUT set "BF2_USER_ORDER_OUT=%TASK_CODE%\Build\ai_user_order_validation_20260927"
if not exist "%BF2_USER_ORDER_OUT%" mkdir "%BF2_USER_ORDER_OUT%"
set "TASK_FLAGS=/nologo /c /O2 /MD /std:c++14 /permissive /EHsc /W3 /Gy /DDEDICATED=1 /DBF2_IMAGE_AIDLL"
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\AIStrategicEntity\UserOrders\UserOrderManager.cpp" /Fo"%BF2_USER_ORDER_OUT%\UserOrderManager.obj"
if errorlevel 1 exit /b 1
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%~dp0user_order_runtime.cpp" /Fo"%BF2_USER_ORDER_OUT%\fixture.obj"
if errorlevel 1 exit /b 1
link /nologo /OPT:REF /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /OUT:"%BF2_USER_ORDER_OUT%\user_order_runtime.exe" "%BF2_USER_ORDER_OUT%\UserOrderManager.obj" "%BF2_USER_ORDER_OUT%\fixture.obj"
if errorlevel 1 exit /b 1
python "%~dp0user_order_runtime.py" "%BF2_USER_ORDER_OUT%\user_order_runtime.exe"
exit /b %errorlevel%
