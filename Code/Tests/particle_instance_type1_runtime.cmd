@echo off
setlocal EnableDelayedExpansion
rem [TEMP] strict x86 actual-source mesh-particle (type 1) fixture, native plugin ABI; no game or GPU.
rem 2026-09-26 (G1). The manager TU and the REAL type-1 descriptor TU (ParticleSystemTemplateType1.cpp)
rem are linked; the descriptor's constructor and setters build the template the instance runs on.
if not defined BF2_BUILD_OUT exit /b 2
call "%~dp0..\Build\vsdevcmd.cmd" x86
call "%~dp0..\Build\dxsdk.cmd" x86
if errorlevel 1 exit /b 1
set "TASK_ROOT=%~dp0..\.."
if not defined BF2_TEST_OPT set "BF2_TEST_OPT=/O2"
for %%A in (1) do (
  set "OUT=%BF2_BUILD_OUT%\abi%%A"
  if not exist "!OUT!" mkdir "!OUT!"
  cl /nologo %BF2_TEST_OPT% /EHsc /std:c++14 /permissive /MD /Gy /Gw /Z7 ^
   /DDEDICATED=0 /DBF2_IMAGE_RENDDX9 /DBF2_PLUGIN_ABI_NATIVE=%%A /D_ITERATOR_DEBUG_LEVEL=0 ^
   /FI"%TASK_ROOT%\Code\BF2\Core\BuildConfig.h" /Fo"!OUT!\\" ^
   /Fe"!OUT!\particle_instance_type1_runtime.exe" ^
   "%~dp0particle_instance_type1_runtime.cpp" ^
   "%TASK_ROOT%\Code\BF2\Geom\ParticleSystem\ParticleSystemTemplateType1.cpp" ^
   "%TASK_ROOT%\Code\BF2\Math\Mat4.cpp" "%TASK_ROOT%\Code\BF2\Math\Frustum.cpp" ^
   "%TASK_ROOT%\Code\BF2\Math\Mat4Inverse.cpp" "%TASK_ROOT%\Code\BF2\Math\Vec3.cpp" ^
   "%TASK_ROOT%\Code\BF2\Math\Plane3.cpp" ^
   /link /OPT:REF /INCREMENTAL:NO /SUBSYSTEM:CONSOLE > "!OUT!\build.log" 2>&1
  if errorlevel 1 (echo abi%%A BUILD FAILED & type "!OUT!\build.log" & goto :fail)
  "!OUT!\particle_instance_type1_runtime.exe" > "!OUT!\run.txt" 2>&1
  set "RC=!errorlevel!"
  type "!OUT!\run.txt"
  if not "!RC!"=="0" (echo abi%%A run exited !RC! & goto :fail)
)
echo Mesh particle (type 1) fixture: native ABI passed
exit /b 0

rem !! Under `cmd /c`, an `exit /b 1` INSIDE a parenthesized block (or after `||` in a for body)
rem reaches the caller as exit code 0 (measured 2026-09-26). Every failure jumps here first.
:fail
exit /b 1
