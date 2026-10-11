@echo off
setlocal EnableDelayedExpansion
rem [TEMP] strict x86 actual-source emitter (type 3) fixture, native plugin ABI; no game or GPU startup.
rem 2026-09-26 (G1). The smoke run drives a real emitter with a real type-0 child through the manager's
rem own vtable; the `nullchild` run must stop at the emitter's NAMED stop (an unregistered child), which
rem the manager fixture's frontier trap reports as UNEXPECTED with exit 87.
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
   /Fe"!OUT!\particle_instance_type3_runtime.exe" ^
   "%~dp0particle_instance_type3_runtime.cpp" "%TASK_ROOT%\Code\RendDX9Abi\Vc71String.cpp" ^
   "%TASK_ROOT%\Code\BF2\Math\Mat4.cpp" "%TASK_ROOT%\Code\BF2\Math\Frustum.cpp" ^
   "%TASK_ROOT%\Code\BF2\Math\Mat4Inverse.cpp" "%TASK_ROOT%\Code\BF2\Math\Vec3.cpp" ^
   "%TASK_ROOT%\Code\BF2\Math\Plane3.cpp" ^
   /link /OPT:REF /INCREMENTAL:NO /SUBSYSTEM:CONSOLE > "!OUT!\build.log" 2>&1
  if errorlevel 1 (echo abi%%A BUILD FAILED & type "!OUT!\build.log" & goto :fail)
  call :run %%A || goto :fail
)
echo Emitter (type 3) fixture: native ABI passed, and the null-child named stop confirmed
exit /b 0

:run
set "RUNOUT=%BF2_BUILD_OUT%\abi%1"
"%RUNOUT%\particle_instance_type3_runtime.exe" > "%RUNOUT%\smoke.txt" 2>&1
set "RC=%errorlevel%"
type "%RUNOUT%\smoke.txt"
if not "%RC%"=="0" (echo abi%1 smoke exited %RC% & exit /b 1)
"%RUNOUT%\particle_instance_type3_runtime.exe" nullchild > "%RUNOUT%\nullchild.txt" 2>&1
set "RC=%errorlevel%"
if not "%RC%"=="87" (echo abi%1 nullchild exited %RC%, expected 87 & type "%RUNOUT%\nullchild.txt" & exit /b 1)
findstr /C:"ParticleSystemEmitter::advanceParticle (child instance is null" "%RUNOUT%\nullchild.txt" >nul
if errorlevel 1 (echo abi%1 nullchild stopped without its named stop & type "%RUNOUT%\nullchild.txt" & exit /b 1)
echo abi%1: null-child named stop confirmed
exit /b 0

rem !! Under `cmd /c`, an `exit /b 1` INSIDE a parenthesized block (or after `||` in a for body)
rem reaches the caller as exit code 0 (measured 2026-09-26). Every failure jumps here first.
:fail
exit /b 1
