@echo off
rem ===========================================================================
rem SCAFFOLDING - NOT RECOVERED SOURCE. Locates the DirectX SDK the renderer builds against.
rem
rem   usage:  call "%~dp0dxsdk.cmd" x86^|x64      (after vsdevcmd.cmd, which rebuilds INCLUDE/LIB)
rem
rem RendDX9.dll and TextureAtlasBuilder.dll compile against the installed April 2005 DirectX SDK
rem (D3DX_SDK_VERSION 25) and link its import libraries. That SDK's d3dx9.lib binds d3dx9_25.dll, the
rem D3DX both shipped DLLs import, and it ships Lib\x86 and Lib\x64, so a 64-bit client build links
rem the same D3DX generation. A later SDK binds a later d3dx9_NN.dll with a different effect compiler
rem and is refused rather than taken silently.
rem
rem Resolution order: DXSDK_DIR (the SDK installer sets it), then the SDK's default install folder.
rem
rem On success, in the CALLER's environment (no setlocal here, on purpose):
rem   BF2_DXSDK_DIR   the SDK root
rem   BF2_DXSDK_LIB   <root>\Lib\<arch>, holding d3dx9.lib and DxErr9.lib for that architecture
rem   INCLUDE         <root>\Include APPENDED after the Windows SDK. <d3dx9.h> resolves from the
rem                   DirectX SDK while <d3d9.h>, <dinput.h>, <dsound.h> and <strsafe.h> keep
rem                   resolving from the Windows SDK.
rem   LIB             <root>\Lib\<arch> appended after the Windows SDK, so d3d9.lib, dinput8.lib and
rem                   dxguid.lib still come from the Windows SDK.
rem On failure: errorlevel 1, a message naming what is missing, and INCLUDE/LIB unchanged.
rem
rem Everything runs at top level or in :subroutines: the SDK's path holds parentheses, and expanding
rem it inside a parenthesised block would close the block early.
rem ===========================================================================
if /i "%~1"=="x86" goto :arch_ok
if /i "%~1"=="x64" goto :arch_ok
echo dxsdk.cmd: usage: call dxsdk.cmd x86^|x64
exit /b 2
:arch_ok
set "BF2_DXSDK_ARCH=%~1"
set "BF2_DXSDK_DIR="
set "BF2_DXSDK_LIB="
set "BF2_DXSDK_WHY="

set "BF2_DXSDK_TRY="
if defined DXSDK_DIR for %%I in ("%DXSDK_DIR%\.") do set "BF2_DXSDK_TRY=%%~fI"
if defined BF2_DXSDK_TRY call :probe
if defined BF2_DXSDK_DIR goto :found
set "BF2_DXSDK_TRY=%ProgramFiles(x86)%\Microsoft DirectX 9.0 SDK (April 2005)"
call :probe
if defined BF2_DXSDK_DIR goto :found
set "BF2_DXSDK_TRY=%ProgramFiles%\Microsoft DirectX 9.0 SDK (April 2005)"
call :probe
if defined BF2_DXSDK_DIR goto :found

echo.
echo   DirectX SDK NOT FOUND - RendDX9.dll and TextureAtlasBuilder.dll need the April 2005 DirectX SDK
echo   ^(D3DX_SDK_VERSION 25, the D3DX generation of the shipped d3dx9_25.dll^).
if defined BF2_DXSDK_WHY echo   %BF2_DXSDK_WHY%
echo   Install "Microsoft DirectX 9.0 SDK (April 2005)" and point DXSDK_DIR at it.
echo   See docs\building.md, "DirectX SDK".
set "BF2_DXSDK_TRY="
set "BF2_DXSDK_ARCH="
set "BF2_DXSDK_WHY="
exit /b 1

:found
set "BF2_DXSDK_LIB=%BF2_DXSDK_DIR%\Lib\%BF2_DXSDK_ARCH%"
set "INCLUDE=%INCLUDE%;%BF2_DXSDK_DIR%\Include"
set "LIB=%LIB%;%BF2_DXSDK_LIB%"
echo   DirectX SDK: %BF2_DXSDK_DIR% ^(D3DX 25, %BF2_DXSDK_ARCH%^)
set "BF2_DXSDK_TRY="
set "BF2_DXSDK_ARCH="
set "BF2_DXSDK_WHY="
exit /b 0

rem Accepts BF2_DXSDK_TRY as the SDK root when it holds the version-25 D3DX headers and this
rem architecture's d3dx9.lib; otherwise records why in BF2_DXSDK_WHY.
:probe
if not exist "%BF2_DXSDK_TRY%\Include\d3dx9core.h" goto :eof
findstr /c:"#define D3DX_SDK_VERSION 25" "%BF2_DXSDK_TRY%\Include\d3dx9core.h" >nul 2>&1
if errorlevel 1 goto :probe_version
if not exist "%BF2_DXSDK_TRY%\Lib\%BF2_DXSDK_ARCH%\d3dx9.lib" goto :probe_lib
if not exist "%BF2_DXSDK_TRY%\Lib\%BF2_DXSDK_ARCH%\DxErr9.lib" goto :probe_lib
set "BF2_DXSDK_DIR=%BF2_DXSDK_TRY%"
goto :eof
:probe_version
set "BF2_DXSDK_WHY=Rejected "%BF2_DXSDK_TRY%": its d3dx9core.h is not D3DX_SDK_VERSION 25."
goto :eof
:probe_lib
set "BF2_DXSDK_WHY=Rejected "%BF2_DXSDK_TRY%": Lib\%BF2_DXSDK_ARCH% lacks d3dx9.lib or DxErr9.lib."
goto :eof
