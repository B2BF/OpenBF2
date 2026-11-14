@echo off
rem [TEMP] [CHOSEN, not recovered] Builds the dinput8.dll proxy test tool (dinput8proxy.cpp).
rem usage: build.cmd x86|x64 OUTDIR    (run from any directory; needs Visual Studio 2022 C++)
setlocal
set "ARCH=%~1"
if "%ARCH%"=="" set "ARCH=x86"
set "OUT=%~2"
if "%OUT%"=="" set "OUT=%~dp0out-%ARCH%"
if not exist "%OUT%" mkdir "%OUT%"
call "%~dp0..\..\..\Code\Build\vsdevcmd.cmd" %ARCH% >nul || exit /b 1
cl /nologo /LD /O2 /EHsc /MT /W3 /std:c++14 "%~dp0dinput8proxy.cpp" /Fo"%OUT%\\" /Fe"%OUT%\dinput8.dll" /link /DEF:"%~dp0dinput8.def" user32.lib gdi32.lib dxguid.lib || exit /b 2
echo built %OUT%\dinput8.dll
