@echo off
setlocal EnableDelayedExpansion
rem ===========================================================================
rem SCAFFOLDING - NOT RECOVERED SOURCE. Builds the REPLACEMENT TextureAtlasBuilder.dll.
rem
rem   Code\Build\renddx9.cmd              the replacement RendDX9.dll - it IMPORTS this DLL
rem   Code\Build\textureatlasbuilder.cmd  THIS. The atlas packer RendDX9's editor commands
rem                                        (Overgrowth.generateAtlas, DecalManager.generateTexture,
rem                                        the undergrowth rebuild) call through two exported creates.
rem   Code\Build\textureatlas_diff.cmd    the parity harness (retail vs rebuilt)
rem
rem IT NEVER WRITES INTO THE GAME DIRECTORY. Output goes to
rem   Code\Build\bin_textureatlasbuilder_native\.
rem   The shipped TextureAtlasBuilder.dll in the reference game directory is the REFERENCE BINARY.
rem   Sources: Code\BF2\RendDX9\TextureAtlas\TextureAtlasBuilder\ (every TU is guarded by
rem   BF2_IMAGE_TEXTUREATLASBUILDER and compiles to nothing anywhere else).
rem
rem   usage:  textureatlasbuilder.cmd [native]   CRT heap, no Memory.dll
rem   BF2_TEXTUREATLAS_OUT and BF2_TEXTUREATLAS_BIN override the output paths.
rem [CHOSEN, not recovered] Native-only builds: https://github.com/B2BF/OpenBF2/issues/7
rem
rem This DLL links the CRT and STL statically (/MT), unlike the other rebuilt plugin DLLs
rem and the shipped image (it imports MSVCR71.dll / MSVCP71.dll, the VC7.1
rem runtime). Nothing on this DLL's interface crosses a heap or a CRT (the callbacks' string
rem is read only by a host that loaded this exact image), so sharing the host's /MD runtime buys
rem nothing - and it would cost byte parity: the .tai writer formats its floats with
rem ostream::operator<<(float), and under /MD that formatting runs inside msvcp140.dll with the UCRT's
rem round-half-even, while VC7.1 rounded an exact decimal tie UP ("0.000976563" in the stock
rem MemeAtlas.tai). legacy_stdio_float_rounding.obj restores the old rounding only for stdio linked
rem into THIS image. Allocations stay on this DLL's static CRT heap.
rem
rem VALIDATE THE DLL'S FILES ONLY WITH THIS SCRIPT, RUN VIA THE PowerShell TOOL (Git-Bash `cmd /c`
rem returns 0 without building). Failures leave through :fail - an `exit /b 1` inside a block reaches
rem a `cmd /c` caller as 0.
rem ===========================================================================
if not "%~2"=="" goto :usage
if "%~1"=="" goto :args_ok
if /i "%~1"=="native" goto :args_ok
goto :usage
:args_ok
call "%~dp0vsdevcmd.cmd" x86
if errorlevel 1 goto :fail
rem The exact SDK generation the shipped image was built with (D3DX_SDK_VERSION 25 = April 2005):
rem its d3dx9.lib names d3dx9_25.dll, and its DxErr9.lib is the very DXGetErrorString9A the shipped
rem image linked (third-party code). dxsdk.cmd refuses any other SDK.
call "%~dp0dxsdk.cmd" x86
if errorlevel 1 goto :fail

for %%I in ("%~dp0..") do set CODE=%%~fI
set ABI=%CODE%\RendDX9Abi
set BF2=%CODE%\BF2
set SRC=%CODE%\BF2\RendDX9\TextureAtlas\TextureAtlasBuilder
set OUT=%~dp0obj_textureatlasbuilder_native
set BIN=%~dp0bin_textureatlasbuilder_native
rem Preserve agent/CI output isolation.
if defined BF2_TEXTUREATLAS_OUT set "OUT=%BF2_TEXTUREATLAS_OUT%"
if defined BF2_TEXTUREATLAS_BIN set "BIN=%BF2_TEXTUREATLAS_BIN%"
for %%I in ("%OUT%") do set "OUT=%%~fI"
for %%I in ("%BIN%") do set "BIN=%%~fI"

set "D3DX25_LIB=%BF2_DXSDK_LIB%\d3dx9.lib"
rem DxErr9.lib is a VC7.1 object: its DXTrace references __vsnprintf (legacy_stdio_definitions.lib)
rem and MessageBox (user32.lib). /OPT:REF drops DXTrace, so neither reaches the import table - the
rem ABI check below holds that.
set DXERR_LIBS="%BF2_DXSDK_LIB%\DxErr9.lib" legacy_stdio_definitions.lib user32.lib

rem /MT                       - the static CRT and STL (see the header).
rem /DBF2_IMAGE_TEXTUREATLASBUILDER - admits the TUs (they compile to nothing without it) and makes
rem                             ITextureAtlas.h dllexport.
rem /DDEDICATED=0 + /FI Core\BuildConfig.h - the IMAGE define; this DLL belongs to the client side
rem                            .
rem /DNDEBUG                  - the shipped image is a release build: no asserts.
rem No /Gr: the conventions the ABI needs are spelled out in ITextureAtlas.h.
set FLAGS=/nologo /c /MT /O2 /std:c++14 /permissive /EHsc /W4 /wd4100 /wd4127 /Zi /FS ^
 /DBF2_IMAGE_TEXTUREATLASBUILDER /DDEDICATED=0 /FI"%BF2%\Core\BuildConfig.h" /DWIN32 /D_WINDOWS /DNDEBUG /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_DEPRECATE ^
 /Fd"%OUT%\TextureAtlasBuilder.pdb"

echo   Native atlas heap, CRT=/MT
echo   CODE=%CODE%
echo   OUT=%OUT%
echo   BIN=%BIN%
echo   D3DX: the April 2005 SDK's d3dx9.lib and DxErr9.lib
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OUT%" goto :fail
if not exist "%BIN%" mkdir "%BIN%"
if not exist "%BIN%" goto :fail
del /q "%OUT%\*.obj" >nul 2>&1
del /q "%OUT%\*.log" >nul 2>&1

rem ---------------------------------------------------------------------------
rem STEP 1 - import libraries: the DirectX SDK's, selected above. Nothing to generate.
rem ---------------------------------------------------------------------------
echo [1/4] import libraries
echo   "%D3DX25_LIB%"

rem ---------------------------------------------------------------------------
rem STEP 2 - compile. Explicit manifest, numbered objects (flat output dir), in the shipped image's
rem link order: the format helpers (0x10001000), the packer (0x10001E60), the atlas base (0x10002210),
rem DllMain (0x10007450), the configuration base (0x10007460), the D3D9 leaves (0x10008220).
rem ---------------------------------------------------------------------------
echo [2/4] compiling
set SOURCES=^
 "%SRC%\TextureFormat.cpp" ^
 "%SRC%\Packer.cpp" ^
 "%SRC%\TextureAtlas.cpp" ^
 "%SRC%\DllMain.cpp" ^
 "%SRC%\TextureAtlasConfiguration.cpp" ^
 "%SRC%\TextureAtlasD3D9.cpp"

set OK=0
set BAD=0
set /a OBJN=0
for %%F in (%SOURCES%) do (
    set /a OBJN+=1
    cl %FLAGS% /Fo"%OUT%\!OBJN!_%%~nF.obj" "%%~F" >"%OUT%\cl_%%~nF.log" 2>&1
    if errorlevel 1 (
        set /a BAD+=1
        echo   COMPILE FAIL %%~nxF
        type "%OUT%\cl_%%~nF.log"
    ) else (
        set /a OK+=1
        findstr /c:"warning C" "%OUT%\cl_%%~nF.log" >nul && type "%OUT%\cl_%%~nF.log"
    )
)
echo   %OK% compiled, %BAD% failed
if not "%BAD%"=="0" goto :fail

rem ---------------------------------------------------------------------------
rem STEP 3 - link. /DEF pins the twelve exports to the shipped ordinals and marks the vftables DATA.
rem legacy_stdio_float_rounding.obj - VC7.1's round-half-up for %g ties (see the header).
rem No d3d9.lib: the shipped image has no d3d9 import; the device arrives through create().
rem ---------------------------------------------------------------------------
echo [3/4] linking
link /nologo /DLL /MACHINE:X86 /BASE:0x10000000 /DEBUG /OPT:REF /OPT:ICF ^
     /DEF:"%ABI%\TextureAtlasBuilderExports.def" ^
     /OUT:"%BIN%\TextureAtlasBuilder.dll" ^
     /PDB:"%BIN%\TextureAtlasBuilder.pdb" ^
     /IMPLIB:"%OUT%\TextureAtlasBuilder.lib" ^
     /MAP:"%OUT%\TextureAtlasBuilder.map" ^
     "%OUT%\*.obj" legacy_stdio_float_rounding.obj "%D3DX25_LIB%" %DXERR_LIBS% kernel32.lib > "%OUT%\link.log" 2>&1
if not errorlevel 1 goto :link_ok
echo   LINK FAILED - see "%OUT%\link.log"
type "%OUT%\link.log"
goto :fail
:link_ok
findstr /c:"warning LNK" "%OUT%\link.log" >nul && type "%OUT%\link.log"

rem ---------------------------------------------------------------------------
rem STEP 4 - the ABI checks that matter, run every build. findstr, NOT find. The expected exports are
rem read from the .def the link pinned them with (name and @ordinal on each EXPORTS line) and matched
rem against dumpbin's "<ordinal> <hint> <rva> <name>" lines - both fields, so a drifted ordinal fails.
rem ---------------------------------------------------------------------------
echo [4/4] ABI check
dumpbin /nologo /exports "%BIN%\TextureAtlasBuilder.dll" > "%OUT%\exports.txt"
if errorlevel 1 goto :fail
dumpbin /nologo /dependents "%BIN%\TextureAtlasBuilder.dll" > "%OUT%\dependents.txt"
if errorlevel 1 goto :fail
dumpbin /nologo /imports "%BIN%\TextureAtlasBuilder.dll" > "%OUT%\imports.txt"
if errorlevel 1 goto :fail

set ABI_BAD=0
set EXPECTED=0
findstr /c:"12 number of functions" "%OUT%\exports.txt" >nul
if errorlevel 1 (
    echo   exports:      not exactly twelve functions - the shipped image has twelve
    set /a ABI_BAD+=1
)
for /f "usebackq eol=; tokens=1,2" %%A in ("%ABI%\TextureAtlasBuilderExports.def") do (
    set "ORD=%%B"
    if "!ORD:~0,1!"=="@" (
        set /a EXPECTED+=1
        set "ORD=!ORD:~1!"
        set FOUND=0
        for /f "tokens=1,4" %%O in ('findstr /l /c:"%%A" "%OUT%\exports.txt"') do (
            if "%%O"=="!ORD!" if "%%P"=="%%A" set FOUND=1
        )
        if "!FOUND!"=="0" (
            echo   exports:      ordinal !ORD! %%A MISSING
            set /a ABI_BAD+=1
        )
    )
)
if not "!EXPECTED!"=="12" (
    echo   exports:      read !EXPECTED! names from TextureAtlasBuilderExports.def, expected 12
    set /a ABI_BAD+=1
)
findstr /i /c:"d3dx9_25.dll" "%OUT%\dependents.txt" >nul
if errorlevel 1 (
    echo   imports:      d3dx9_25.dll MISSING
    set /a ABI_BAD+=1
)
findstr /i /c:"d3d9.dll" "%OUT%\dependents.txt" >nul
if not errorlevel 1 (
    echo   imports:      d3d9.dll UNEXPECTED - the shipped image takes the device through create^(^)
    set /a ABI_BAD+=1
)
findstr /i /c:"USER32.dll" "%OUT%\dependents.txt" >nul
if not errorlevel 1 (
    echo   imports:      USER32.dll UNEXPECTED - DxErr9's DXTrace should have been discarded by /OPT:REF
    set /a ABI_BAD+=1
)
findstr /i /c:"VCRUNTIME" /c:"api-ms-win-crt-" /c:"ucrtbase.dll" /c:"MSVCP" /c:"msvcr71.dll" "%OUT%\dependents.txt" >nul
if not errorlevel 1 (
    echo   imports:      a shared CRT UNEXPECTED - this DLL links the CRT statically
    set /a ABI_BAD+=1
)
findstr /i /c:"Memory.dll" "%OUT%\dependents.txt" >nul
if not errorlevel 1 (
    echo   imports:      Memory.dll UNEXPECTED in the native atlas
    set /a ABI_BAD+=1
)
for %%D in (D3DXCreateTexture D3DXCreateTextureFromFileExA D3DXGetImageInfoFromFileA D3DXLoadSurfaceFromSurface D3DXSaveTextureToFileA) do (
    findstr /l /e /c:" %%D" "%OUT%\imports.txt" >nul
    if errorlevel 1 (
        echo   imports:      d3dx9_25 %%D MISSING
        set /a ABI_BAD+=1
    )
)
if not "!ABI_BAD!"=="0" goto :abi_fail
echo   twelve exports at the shipped ordinals; d3dx9_25 by name; heap/CRT imports OK

echo.
echo   built: %BIN%\TextureAtlasBuilder.dll
echo   NOTE: this is NOT installed anywhere. Stage it beside the rebuilt RendDX9.dll.
exit /b 0

:abi_fail
echo   ABI CHECK FAILED - !ABI_BAD! invariant(s) violated
:fail
echo   textureatlasbuilder.cmd FAILED
exit /b 1

:usage
echo Usage: textureatlasbuilder.cmd [native]
exit /b 2
