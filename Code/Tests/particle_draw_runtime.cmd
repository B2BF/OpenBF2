@echo off
setlocal
rem [TEMP] Strict x86 actual-source constructor/preload/update/draw fixture, no real GPU or level.
if not defined BF2_BUILD_OUT exit /b 2
call "%~dp0..\Build\vsdevcmd.cmd" x86
call "%~dp0..\Build\dxsdk.cmd" x86
if errorlevel 1 exit /b 1
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"
set "TASK_ROOT=%~dp0..\.."
if not defined BF2_TEST_OPT set "BF2_TEST_OPT=/O2"
cl /nologo %BF2_TEST_OPT% /EHsc /std:c++14 /permissive /MD /Gy /Gw /Z7 ^
 /DDEDICATED=0 /DBF2_IMAGE_RENDDX9 /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0 ^
 /FI"%TASK_ROOT%\Code\BF2\Core\BuildConfig.h" /Fo"%BF2_BUILD_OUT%\\" ^
 /Fe"%BF2_BUILD_OUT%\particle_draw_runtime.exe" ^
 "%~dp0particle_draw_runtime.cpp" ^
 "%TASK_ROOT%\Code\BF2\Math\Mat4.cpp" "%TASK_ROOT%\Code\BF2\Math\Frustum.cpp" ^
 "%TASK_ROOT%\Code\BF2\Math\Mat4Inverse.cpp" "%TASK_ROOT%\Code\BF2\Math\Vec3.cpp" ^
 "%TASK_ROOT%\Code\BF2\Math\Plane3.cpp" ^
 /link /OPT:REF /INCREMENTAL:NO /SUBSYSTEM:CONSOLE
if errorlevel 1 exit /b 1
"%BF2_BUILD_OUT%\particle_draw_runtime.exe"
if errorlevel 1 exit /b %errorlevel%
rem 2026-09-26 (G1): cases 2, 6 and 8 targeted the mesh renderer's named stops, which are recovered
rem bodies now; they are gone. The loop also silently PASSED a crashing case (an access violation's
rem negative errorlevel slipped through the old `if not errorlevel` pair inside the block), so each
rem case now runs through a subroutine that accepts exit 86 and nothing else.
for %%i in (3 7 9) do call :negative %%i || goto :fail
echo Remaining optional Trail debug and constructor frontiers: 3/3 expected stops
exit /b 0

:negative
"%BF2_BUILD_OUT%\particle_draw_runtime.exe" active %1
if "%errorlevel%"=="86" exit /b 0
echo negative case %1 exited %errorlevel%, expected 86
exit /b 1

rem !! Under `cmd /c`, an `exit /b 1` INSIDE a parenthesized block (or after `||` in a for body)
rem reaches the caller as exit code 0 (measured 2026-09-26). Every failure jumps here first.
:fail
exit /b 1
