@echo off
setlocal
rem [TEMP] Whole Information.cpp + recovered IPIUnit.cpp + real pools against typed observers,
rem strict x86 link. (Until 2026-09-29: the four Information frontier stops.)
call "%~dp0..\..\Build\vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1
set "TASK_CODE=%~dp0..\.."
if not defined BF2_INFORMATION_FRONTIERS_OUT set "BF2_INFORMATION_FRONTIERS_OUT=%TASK_CODE%\Build\ai_kit_scan_20260927\information\validation"
if not exist "%BF2_INFORMATION_FRONTIERS_OUT%" mkdir "%BF2_INFORMATION_FRONTIERS_OUT%"
python -I "%~dp0information_frontiers_runtime.py" generate "%BF2_INFORMATION_FRONTIERS_OUT%"
if errorlevel 1 exit /b 1
set "TASK_FLAGS=/nologo /c /O2 /MD /std:c++14 /permissive /EHsc /W3 /Gy /DDEDICATED=1 /DBF2_IMAGE_AIDLL"
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\AIKnowledge\Information.cpp" /Fo"%BF2_INFORMATION_FRONTIERS_OUT%\actual.obj" >"%BF2_INFORMATION_FRONTIERS_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\Main\InformationManager.cpp" /Fo"%BF2_INFORMATION_FRONTIERS_OUT%\manager.obj" >>"%BF2_INFORMATION_FRONTIERS_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\AIKnowledge\IPIUnit.cpp" /Fo"%BF2_INFORMATION_FRONTIERS_OUT%\IPIUnit.obj" >>"%BF2_INFORMATION_FRONTIERS_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\AIKnowledge\IPICover.cpp" /Fo"%BF2_INFORMATION_FRONTIERS_OUT%\IPICover.obj" >>"%BF2_INFORMATION_FRONTIERS_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\AIKnowledge\IPIMobile.cpp" /Fo"%BF2_INFORMATION_FRONTIERS_OUT%\IPIMobile.obj" >>"%BF2_INFORMATION_FRONTIERS_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIInterface\BFKnowledge\PlanePolygon.cpp" /Fo"%BF2_INFORMATION_FRONTIERS_OUT%\PlanePolygon.obj" >>"%BF2_INFORMATION_FRONTIERS_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\Math\Vec3.cpp" /Fo"%BF2_INFORMATION_FRONTIERS_OUT%\Vec3.obj" >>"%BF2_INFORMATION_FRONTIERS_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\AIUtils\WeaponInfoManager.cpp" /Fo"%BF2_INFORMATION_FRONTIERS_OUT%\WeaponInfoManager.obj" >>"%BF2_INFORMATION_FRONTIERS_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\AIUtils\CustomCurve.cpp" /Fo"%BF2_INFORMATION_FRONTIERS_OUT%\CustomCurve.obj" >>"%BF2_INFORMATION_FRONTIERS_OUT%\compile.log" 2>&1
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\AIUtils\SCurve.cpp" /Fo"%BF2_INFORMATION_FRONTIERS_OUT%\SCurve.obj" >>"%BF2_INFORMATION_FRONTIERS_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
for %%F in (Debug StringUtil) do (
  cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\Core\%%F.cpp" /Fo"%BF2_INFORMATION_FRONTIERS_OUT%\%%F.obj" >>"%BF2_INFORMATION_FRONTIERS_OUT%\compile.log" 2>&1
  if errorlevel 1 goto failed
)
cl %TASK_FLAGS% /I"%BF2_INFORMATION_FRONTIERS_OUT%" /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%~dp0information_frontiers_runtime.cpp" /Fo"%BF2_INFORMATION_FRONTIERS_OUT%\fixture.obj" >>"%BF2_INFORMATION_FRONTIERS_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
link /nologo /MACHINE:X86 /OPT:REF /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /OUT:"%BF2_INFORMATION_FRONTIERS_OUT%\information_frontiers_runtime.exe" "%BF2_INFORMATION_FRONTIERS_OUT%\actual.obj" "%BF2_INFORMATION_FRONTIERS_OUT%\manager.obj" "%BF2_INFORMATION_FRONTIERS_OUT%\IPIUnit.obj" "%BF2_INFORMATION_FRONTIERS_OUT%\IPICover.obj" "%BF2_INFORMATION_FRONTIERS_OUT%\IPIMobile.obj" "%BF2_INFORMATION_FRONTIERS_OUT%\PlanePolygon.obj" "%BF2_INFORMATION_FRONTIERS_OUT%\Vec3.obj" "%BF2_INFORMATION_FRONTIERS_OUT%\WeaponInfoManager.obj" "%BF2_INFORMATION_FRONTIERS_OUT%\CustomCurve.obj" "%BF2_INFORMATION_FRONTIERS_OUT%\SCurve.obj" "%BF2_INFORMATION_FRONTIERS_OUT%\Debug.obj" "%BF2_INFORMATION_FRONTIERS_OUT%\StringUtil.obj" "%BF2_INFORMATION_FRONTIERS_OUT%\fixture.obj" >"%BF2_INFORMATION_FRONTIERS_OUT%\link.log" 2>&1
if errorlevel 1 goto failed
python -I "%~dp0information_frontiers_runtime.py" run "%BF2_INFORMATION_FRONTIERS_OUT%\information_frontiers_runtime.exe"
exit /b %errorlevel%
:failed
type "%BF2_INFORMATION_FRONTIERS_OUT%\compile.log"
if exist "%BF2_INFORMATION_FRONTIERS_OUT%\link.log" type "%BF2_INFORMATION_FRONTIERS_OUT%\link.log"
exit /b 1
