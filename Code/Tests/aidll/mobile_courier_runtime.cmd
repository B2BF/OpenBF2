@echo off
setlocal
rem [TEMP] Whole-production-TU, strict MSVC x86 validation for IPIMobile, courier polygon state,
rem and InformationFactory type 3. This output directory is isolated from every shared build.
call "%~dp0..\..\Build\vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1
set "TASK_CODE=%~dp0..\.."
if not defined BF2_MOBILE_COURIER_OUT set "BF2_MOBILE_COURIER_OUT=%TASK_CODE%\Build\ai_map_query_20261001\mobile_courier_validation"
if not exist "%BF2_MOBILE_COURIER_OUT%" mkdir "%BF2_MOBILE_COURIER_OUT%"
python -I "%~dp0mobile_courier_runtime.py" generate "%BF2_MOBILE_COURIER_OUT%"
if errorlevel 1 exit /b 1
set "TASK_FLAGS=/nologo /c /O2 /MD /std:c++14 /permissive /EHsc /W3 /Gy /Gw /Zc:inline /DDEDICATED=1 /DBF2_IMAGE_AIDLL"
type nul >"%BF2_MOBILE_COURIER_OUT%\compile.log"
for %%F in (InformationFactory BFInformationCourier Information InformationReal IPIPhysicalReal IPIArmament IPIArmamentReal IPIUnit IPIMobile IPIMobileReal IPIMobileKnown InformationManager WeaponInfo WeaponInfoReal WeaponInfoManager) do (
  cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\AIKnowledge\%%F.cpp" /Fo"%BF2_MOBILE_COURIER_OUT%\%%F.obj" >>"%BF2_MOBILE_COURIER_OUT%\compile.log" 2>&1
  if errorlevel 1 goto failed
)
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\Main\IAIPathfinding.cpp" /Fo"%BF2_MOBILE_COURIER_OUT%\IAIPathfinding.obj" >>"%BF2_MOBILE_COURIER_OUT%\compile.log" 2>&1
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\Main\IAIPathfindingSetup.cpp" /Fo"%BF2_MOBILE_COURIER_OUT%\IAIPathfindingSetup.obj" >>"%BF2_MOBILE_COURIER_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\AIUtils\CustomCurve.cpp" /Fo"%BF2_MOBILE_COURIER_OUT%\CustomCurve.obj" >>"%BF2_MOBILE_COURIER_OUT%\compile.log" 2>&1
cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\AI\AIDLL\AI\AIUtils\SCurve.cpp" /Fo"%BF2_MOBILE_COURIER_OUT%\SCurve.obj" >>"%BF2_MOBILE_COURIER_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
for %%F in (Debug StringUtil) do (
  cl %TASK_FLAGS% /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%TASK_CODE%\BF2\Core\%%F.cpp" /Fo"%BF2_MOBILE_COURIER_OUT%\%%F.obj" >>"%BF2_MOBILE_COURIER_OUT%\compile.log" 2>&1
  if errorlevel 1 goto failed
)
cl %TASK_FLAGS% /I"%BF2_MOBILE_COURIER_OUT%" /FI"%TASK_CODE%\BF2\Core\BuildConfig.h" "%~dp0mobile_courier_runtime.cpp" /Fo"%BF2_MOBILE_COURIER_OUT%\fixture.obj" >>"%BF2_MOBILE_COURIER_OUT%\compile.log" 2>&1
if errorlevel 1 goto failed
set "O=%BF2_MOBILE_COURIER_OUT%"
link /nologo /OPT:REF /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /OUT:"%O%\mobile_courier_runtime.exe" "%O%\InformationFactory.obj" "%O%\BFInformationCourier.obj" "%O%\Information.obj" "%O%\InformationReal.obj" "%O%\IPIPhysicalReal.obj" "%O%\IPIArmament.obj" "%O%\IPIArmamentReal.obj" "%O%\IPIUnit.obj" "%O%\IPIMobile.obj" "%O%\IPIMobileReal.obj" "%O%\IPIMobileKnown.obj" "%O%\InformationManager.obj" "%O%\WeaponInfo.obj" "%O%\WeaponInfoReal.obj" "%O%\WeaponInfoManager.obj" "%O%\IAIPathfinding.obj" "%O%\IAIPathfindingSetup.obj" "%O%\CustomCurve.obj" "%O%\SCurve.obj" "%O%\Debug.obj" "%O%\StringUtil.obj" "%O%\fixture.obj" >"%O%\link.log" 2>&1
if errorlevel 1 goto failed
python -I "%~dp0mobile_courier_runtime.py" run "%O%\mobile_courier_runtime.exe"
exit /b %errorlevel%

:failed
type "%BF2_MOBILE_COURIER_OUT%\compile.log"
if exist "%BF2_MOBILE_COURIER_OUT%\link.log" type "%BF2_MOBILE_COURIER_OUT%\link.log"
exit /b 1
