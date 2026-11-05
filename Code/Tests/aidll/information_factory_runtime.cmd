@echo off
setlocal
rem [TEMP] InformationFactory + courier + recovered cover/mobile/physical/armament/weapon paths +
rem full IPIUnit lifecycle, whole production TUs with the real pools and SCurve, strict x86 link.
call "%~dp0..\..\Build\vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1
set "TASK_CODE=%~dp0..\.."
if not defined BF2_INFORMATION_FACTORY_OUT set "BF2_INFORMATION_FACTORY_OUT=%TASK_CODE%\Build\claude_ai_c07info_20260929\information_factory"
if not exist "%BF2_INFORMATION_FACTORY_OUT%" mkdir "%BF2_INFORMATION_FACTORY_OUT%"
python -I "%~dp0information_factory_runtime.py" generate "%BF2_INFORMATION_FACTORY_OUT%"
if errorlevel 1 exit /b 1
set "TASK_FLAGS=/nologo /c /O2 /MD /std:c++14 /permissive /EHsc /W3 /Gy /DDEDICATED=1 /DBF2_IMAGE_AIDLL"
type nul >"%BF2_INFORMATION_FACTORY_OUT%\compile.log"
for %%F in (InformationFactory BFInformationCourier Information InformationReal IPIPhysicalReal IPIArmament IPIArmamentReal IPICover IPIMobile IPIMobileReal IPIMobileKnown IPIUnit InformationManager WeaponInfo WeaponInfoReal WeaponInfoManager) do (
  cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\AIKnowledge\%%F.cpp" /Fo"%BF2_INFORMATION_FACTORY_OUT%\%%F.obj" >>"%BF2_INFORMATION_FACTORY_OUT%\compile.log" 2>&1
  if errorlevel 1 goto failed
)
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIInterface\BFKnowledge\PlanePolygon.cpp" /Fo"%BF2_INFORMATION_FACTORY_OUT%\PlanePolygon.obj" >>"%BF2_INFORMATION_FACTORY_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\Main\IAIPathfinding.cpp" /Fo"%BF2_INFORMATION_FACTORY_OUT%\IAIPathfinding.obj" >>"%BF2_INFORMATION_FACTORY_OUT%\compile.log" 2>&1
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\Main\IAIPathfindingSetup.cpp" /Fo"%BF2_INFORMATION_FACTORY_OUT%\IAIPathfindingSetup.obj" >>"%BF2_INFORMATION_FACTORY_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\Math\Vec3.cpp" /Fo"%BF2_INFORMATION_FACTORY_OUT%\Vec3.obj" >>"%BF2_INFORMATION_FACTORY_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\AIUtils\CustomCurve.cpp" /Fo"%BF2_INFORMATION_FACTORY_OUT%\CustomCurve.obj" >>"%BF2_INFORMATION_FACTORY_OUT%\compile.log" 2>&1
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\AIUtils\SCurve.cpp" /Fo"%BF2_INFORMATION_FACTORY_OUT%\SCurve.obj" >>"%BF2_INFORMATION_FACTORY_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
for %%F in (Debug StringUtil) do (
  cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\Core\%%F.cpp" /Fo"%BF2_INFORMATION_FACTORY_OUT%\%%F.obj" >>"%BF2_INFORMATION_FACTORY_OUT%\compile.log" 2>&1
  if errorlevel 1 goto failed
)
cl %TASK_FLAGS% /I"%BF2_INFORMATION_FACTORY_OUT%" /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%~dp0information_factory_runtime.cpp" /Fo"%BF2_INFORMATION_FACTORY_OUT%\fixture.obj" >>"%BF2_INFORMATION_FACTORY_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
set "O=%BF2_INFORMATION_FACTORY_OUT%"
link /nologo /OPT:REF /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /OUT:"%O%\information_factory_runtime.exe" "%O%\InformationFactory.obj" "%O%\BFInformationCourier.obj" "%O%\Information.obj" "%O%\InformationReal.obj" "%O%\IPIPhysicalReal.obj" "%O%\IPIArmament.obj" "%O%\IPIArmamentReal.obj" "%O%\IPICover.obj" "%O%\IPIMobile.obj" "%O%\IPIMobileReal.obj" "%O%\IPIMobileKnown.obj" "%O%\IPIUnit.obj" "%O%\InformationManager.obj" "%O%\WeaponInfo.obj" "%O%\WeaponInfoReal.obj" "%O%\WeaponInfoManager.obj" "%O%\PlanePolygon.obj" "%O%\IAIPathfinding.obj" "%O%\IAIPathfindingSetup.obj" "%O%\Vec3.obj" "%O%\CustomCurve.obj" "%O%\SCurve.obj" "%O%\Debug.obj" "%O%\StringUtil.obj" "%O%\fixture.obj" >"%O%\link.log" 2>&1
if errorlevel 1 goto failed
python -I "%~dp0information_factory_runtime.py" run "%BF2_INFORMATION_FACTORY_OUT%\information_factory_runtime.exe"
exit /b %errorlevel%
:failed
type "%BF2_INFORMATION_FACTORY_OUT%\compile.log"
if exist "%BF2_INFORMATION_FACTORY_OUT%\link.log" type "%BF2_INFORMATION_FACTORY_OUT%\link.log"
exit /b 1
