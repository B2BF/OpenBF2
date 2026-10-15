@echo off
setlocal EnableDelayedExpansion
rem ===========================================================================
rem SCAFFOLDING - NOT RECOVERED SOURCE. Builds the REPLACEMENT SwiffPlayer.dll.
rem
rem   Code\Build\build.cmd        the dedicated server, bf2_w32ded.exe
rem   Code\Build\renddx9.cmd      the replacement RendDX9.dll (the sibling front)
rem   Code\Build\swiffplayer.cmd  THIS. The SWF menu player BF2.exe loads.
rem
rem IT NEVER WRITES INTO THE GAME DIRECTORY. Output goes to
rem   Code\Build\bin_swiffplayer_native\.
rem   The shipped SwiffPlayer.dll at
rem   "D:\B2BF Launcher\Battlefield2\SwiffPlayer.dll" is the REFERENCE BINARY.
rem   Testing against the real client is a DELIBERATE, SEPARATE act - see
rem   docs\building.md for host ABI and runtime setup.
rem
rem VALIDATE SWIFFPLAYER FILES ONLY WITH THIS SCRIPT, RUN VIA THE PowerShell TOOL.
rem   syntaxcheck.cmd passes no image define, so a guarded TU compiles to nothing
rem   and its clean exit means nothing. Git-Bash `cmd /c` returns 0 without building.
rem
rem   STAGE 2: the plugin skeleton DLL - the vendored gameswf + base drop, the
rem            shared RendDX9Abi kit, SwiffPlayerAbi (initDll/deinitDll + memory),
rem            and the wrapper (module, factory, the 1120-byte SwiffPlayer with a
rem            shape-complete 37-slot vtable). Links to SwiffPlayer.dll.
rem
rem   usage:  swiffplayer.cmd [native]   rebuilt host ABI (/MD, no Memory.dll), x86
rem           swiffplayer.cmd x64        the native x64 client's SwiffPlayer.dll
rem   BF2_SWIFFPLAYER_OUT and BF2_SWIFFPLAYER_BIN override the output paths.
rem [CHOSEN, not recovered] Native-only builds: https://github.com/B2BF/OpenBF2/issues/7
rem ===========================================================================
if not "%~2"=="" goto :usage
set "ARCH=x86"
if /i "%~1"=="x64" set "ARCH=x64"
if not "%~1"=="" if /i not "%~1"=="native" if /i not "%~1"=="x64" goto :usage
call "%~dp0vsdevcmd.cmd" %ARCH%
if errorlevel 1 exit /b 1

for %%I in ("%~dp0..") do set CODE=%%~fI
set ABI=%CODE%\SwiffPlayerAbi
set RD9ABI=%CODE%\RendDX9Abi
set BF2=%CODE%\BF2
set EXT=%CODE%\BF2\External
set SWF=%CODE%\BF2\External\gameswf\SwiffPlayer
set OUT=%~dp0obj_swiffplayer_native
set BIN=%~dp0bin_swiffplayer_native
if "%ARCH%"=="x64" set OUT=%~dp0obj_swiffplayer_x64
if "%ARCH%"=="x64" set BIN=%~dp0bin_swiffplayer_x64
set CRT=/MD
set PLUGIN_ABI=/DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0
rem Preserve agent/CI output isolation.
if defined BF2_SWIFFPLAYER_OUT set "OUT=%BF2_SWIFFPLAYER_OUT%"
if defined BF2_SWIFFPLAYER_BIN set "BIN=%BF2_SWIFFPLAYER_BIN%"
for %%I in ("%OUT%") do set "OUT=%%~fI"
for %%I in ("%BIN%") do set "BIN=%%~fI"

rem /MD              - shares the rebuilt host's current CRT/STL; BfMemory and SwiffMemory route
rem                    allocations into that shared CRT without importing Memory.dll.
rem /I "%EXT%"       - THE include root. Both gameswf and base include as "base/xxx.h", so External
rem                    itself is the -I. Load-bearing; see Code/BF2/External/README.md.
rem /DBF2_IMAGE_SWIFFPLAYER - admits the guarded wrapper TUs AND selects the engine-heap allocator
rem                    routing in compatibility_include.h.
rem /DNDEBUG         - matches the shipped RELEASE build: gameswf's <assert.h> sanity checks are OFF,
rem                    exactly as they were in retail's SwiffPlayer.dll. With them ON, a gameswf
rem                    debug-assert fires (int3) where retail never had one - e.g. array::resize's
rem                    assert(new_size>=0) during the AS VM's stack churn. NDEBUG is the faithful
rem                    config; it does NOT mask the real advance()/AS dependency (that still faults in
rem                    as_value::drop_refs until the Stage-5 dice.* namespaces exist).
rem /I "%EXT%\gameswf" - the wrapper TUs live in gameswf\SwiffPlayer\ and include the parent's
rem   "gameswf.h" (as DICE's SwiffPlayer subdir did); the gameswf TUs already resolve it same-dir, so
rem   this is additive and harmless. "%EXT%" (the root, for "base/xxx.h") stays first.
rem /DDEDICATED=0 + /FI Core\BuildConfig.h, 2026-09-02: the IMAGE define. This DLL is part of the CLIENT
rem configuration of the shared source; see Code\Build\renddx9.cmd.
set FLAGS=/nologo /c %CRT% /std:c++14 /permissive /EHsc /W3 /FS /Zi ^
 /I "%EXT%" /I "%EXT%\gameswf" ^
 /DBF2_IMAGE_SWIFFPLAYER %PLUGIN_ABI% /DDEDICATED=0 /FI"%BF2%\Core\BuildConfig.h" /DWIN32 /D_WINDOWS /DNDEBUG /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_DEPRECATE ^
 /wd4100 /wd4127 /wd4244 /wd4267 /wd4291 /wd4305 /wd4996 /Fd"%OUT%\SwiffPlayer.pdb"
rem x64: a pointer truncated into a 32-bit integer is an error, as for the x64 dedicated server.
if "%ARCH%"=="x64" set FLAGS=%FLAGS% /we4311 /we4312

echo   Native host ABI, CRT=%CRT%
echo   CODE=%CODE%
echo   OUT=%OUT%
echo   BIN=%BIN%
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OUT%" exit /b 1
if not exist "%BIN%" mkdir "%BIN%"
if not exist "%BIN%" exit /b 1
del /q "%OUT%\*.obj" >nul 2>&1
del /q "%OUT%\*.log" >nul 2>&1

rem ---------------------------------------------------------------------------
rem STEP 1 - native host heap, no Memory.dll import library.
rem ---------------------------------------------------------------------------
echo [1/4] native ABI - no Memory import library

rem ---------------------------------------------------------------------------
rem STEP 2 - compile. Explicit manifest, numbered objects (flat output dir).
rem
rem   the vendored gameswf + base drop (35 TUs, per the two upstream Makefiles - see below)
rem   the shared RendDX9Abi kit (BfMemory/DllMain; Frontier.cpp compiled
rem     separately with the SwiffPlayer log defines)
rem   SwiffPlayerAbi (PluginEntry initDll/deinitDll, SwiffMemory tu_* shim)
rem   the wrapper (SwiffPlayer.cpp, SwiffPlayerModule.cpp)
rem
rem Excluded from the gameswf/base drop:
rem   base: ogl, tu_file_SDL, demo, test_ogl, component_hackery, postscript, png_helper,
rem         triangulate_* (no core caller), logger (2006-vintage), dlmalloc.c (tu_malloc rerouted).
rem   gameswf: morph (superseded by morph2), the 3 render handlers, both sound handlers, the tools.
rem   gameswf_xmlsocket.cpp: EXCLUDED pending link demand - its whole body is #ifdef HAVE_LIBXML
rem         (off), so it emits no code; the only compile blocker is an unconditional #include <SDL.h>.
rem         Re-admit with a [BUILD-FIX] fence only if the link references an XMLSocket symbol.
rem ---------------------------------------------------------------------------
echo [2/4] compiling
set SOURCES=^
 "%EXT%\base\config.cpp" ^
 "%EXT%\base\container.cpp" ^
 "%EXT%\base\file_util.cpp" ^
 "%EXT%\base\image.cpp" ^
 "%EXT%\base\image_filters.cpp" ^
 "%EXT%\base\jpeg.cpp" ^
 "%EXT%\base\membuf.cpp" ^
 "%EXT%\base\tu_file.cpp" ^
 "%EXT%\base\tu_random.cpp" ^
 "%EXT%\base\tu_timer.cpp" ^
 "%EXT%\base\tu_types.cpp" ^
 "%EXT%\base\utf8.cpp" ^
 "%EXT%\base\utility.cpp" ^
 "%EXT%\base\zlib_adapter.cpp" ^
 "%EXT%\gameswf\gameswf_action.cpp" ^
 "%EXT%\gameswf\gameswf_button.cpp" ^
 "%EXT%\gameswf\gameswf_dlist.cpp" ^
 "%EXT%\gameswf\gameswf_font.cpp" ^
 "%EXT%\gameswf\gameswf_fontlib.cpp" ^
 "%EXT%\gameswf\gameswf_impl.cpp" ^
 "%EXT%\gameswf\gameswf_log.cpp" ^
 "%EXT%\gameswf\gameswf_morph2.cpp" ^
 "%EXT%\gameswf\gameswf_movie.cpp" ^
 "%EXT%\gameswf\gameswf_render.cpp" ^
 "%EXT%\gameswf\gameswf_shape.cpp" ^
 "%EXT%\gameswf\gameswf_sound.cpp" ^
 "%EXT%\gameswf\gameswf_stream.cpp" ^
 "%EXT%\gameswf\gameswf_string.cpp" ^
 "%EXT%\gameswf\gameswf_styles.cpp" ^
 "%EXT%\gameswf\gameswf_tesselate.cpp" ^
 "%EXT%\gameswf\gameswf_text.cpp" ^
 "%EXT%\gameswf\gameswf_textformat.cpp" ^
 "%EXT%\gameswf\gameswf_timers.cpp" ^
 "%EXT%\gameswf\gameswf_types.cpp" ^
 "%EXT%\gameswf\gameswf_xml.cpp" ^
 "%RD9ABI%\BfMemory.cpp" ^
 "%RD9ABI%\DllMain.cpp" ^
 "%ABI%\PluginEntry.cpp" ^
 "%ABI%\SwiffMemory.cpp" ^
 "%SWF%\SwiffPlayer.cpp" ^
 "%SWF%\SwiffCrashLog.cpp" ^
 "%SWF%\SwiffPlayerModule.cpp"

set OK=0
set BAD=0
set /a OBJN=0
for %%F in (%SOURCES%) do (
    set /a OBJN+=1
    cl %FLAGS% /Fo"%OUT%\!OBJN!_%%~nF.obj" "%%F" >"%OUT%\cl_%%~nF.log" 2>&1
    if errorlevel 1 (
        set /a BAD+=1
        echo   COMPILE FAIL %%~nxF
        type "%OUT%\cl_%%~nF.log"
    ) else (
        set /a OK+=1
    )
)

rem Frontier.cpp - the shared instrument, compiled here with the SwiffPlayer log file, env var and
rem banner (defaults in the file are RendDX9's, so renddx9.cmd stays byte-for-byte unchanged).
set /a OBJN+=1
cl %FLAGS% /DRD9ABI_LOG_FILENAME=\"SwiffPlayerRebuilt.log\" /DRD9ABI_FRONTIER_ENV=\"BF2_SWIFFPLAYER_FRONTIER\" /DRD9ABI_LOG_BANNER=\"SwiffPlayer_replacement\" /Fo"%OUT%\!OBJN!_Frontier.obj" "%RD9ABI%\Frontier.cpp" >"%OUT%\cl_Frontier.log" 2>&1
if errorlevel 1 ( set /a BAD+=1 & echo   COMPILE FAIL Frontier.cpp & type "%OUT%\cl_Frontier.log" ) else ( set /a OK+=1 )

echo   %OK% compiled, %BAD% failed
if not "%BAD%"=="0" exit /b 1

rem ---------------------------------------------------------------------------
rem STEP 3 - link the DLL. /DEF gives the two undecorated exports and pins their ordinals to the
rem shipped image's. No d3d/bink - SwiffPlayer.dll has zero graphics code (rendering is all
rem out-of-process through ISwiffRender).
rem ---------------------------------------------------------------------------
rem /OPT:REF /OPT:ICF - dead-strip and COMDAT-fold, which /DEBUG otherwise disables. Two reasons this
rem is the faithful choice, not a convenience:
rem   - gameswf_shape.cpp carries OpenGL DEBUG helpers (debug_display_shape_paths, show_fill_number)
rem     that nothing on a live path calls; the shipped SwiffPlayer.dll imports NO opengl32, so DICE's
rem     release link dead-stripped them exactly as this does. Linking opengl32 instead would add a
rem     dependent the shipped image does not have.
rem   - ICF folds the identical IBase bodies to one copy per body, the same folding RendDX9's
rem     ModuleCommon.h documents in the shipped images.
rem winmm.lib - timeGetTime, used by tu_timer::get_ticks on a LIVE path; the shipped image imports
rem   WINMM for it. dsound/wsock32/advapi32/ole32 are the VoIP/settings surface (Stage 6), not linked
rem   yet - link demand will name them when that lands.
echo [3/4] linking
rem /BASE:0x10000000 is the shipped x86 image base; the x64 image takes the linker's default.
set "LINK_BASE=/BASE:0x10000000"
if "%ARCH%"=="x64" set "LINK_BASE="
link /nologo /DLL /MACHINE:%ARCH% %LINK_BASE% /DEBUG /OPT:REF /OPT:ICF ^
     /DEF:"%ABI%\SwiffPlayer.def" ^
     /OUT:"%BIN%\SwiffPlayer.dll" ^
     /PDB:"%BIN%\SwiffPlayer.pdb" ^
     /IMPLIB:"%OUT%\SwiffPlayer.lib" ^
     /MAP:"%OUT%\SwiffPlayer.map" ^
     "%OUT%\*.obj" kernel32.lib user32.lib winmm.lib > "%OUT%\link.log" 2>&1
if errorlevel 1 (
    echo   LINK FAILED - see "%OUT%\link.log"
    type "%OUT%\link.log"
    exit /b 1
)

rem ---------------------------------------------------------------------------
rem STEP 4 - the ABI checks that matter, run every build. findstr, NOT find (find is shadowed by the
rem POSIX toolchain when launched from Git Bash and reports a correct DLL's imports as MISSING).
rem ---------------------------------------------------------------------------
echo [4/4] ABI check
dumpbin /nologo /exports "%BIN%\SwiffPlayer.dll" > "%OUT%\exports.txt"
if errorlevel 1 exit /b 1
dumpbin /nologo /dependents "%BIN%\SwiffPlayer.dll" > "%OUT%\dependents.txt"
if errorlevel 1 exit /b 1
dumpbin /nologo /imports "%BIN%\SwiffPlayer.dll" > "%OUT%\imports.txt"
if errorlevel 1 exit /b 1

set ABI_BAD=0
rem Word boundaries prevent deinitDll or _initDll from satisfying the initDll check.
for %%E in (initDll deinitDll) do (
    findstr /r /c:"\<%%E\>" "%OUT%\exports.txt" >nul
    if errorlevel 1 (
        echo   exports:      %%E MISSING
        set /a ABI_BAD+=1
    )
)
findstr /i /c:"Memory.dll" /c:"msvcr71.dll" /c:"msvcp71.dll" "%OUT%\dependents.txt" >nul
if not errorlevel 1 (
    echo   imports:      retail heap/CRT UNEXPECTED in native ABI
    set /a ABI_BAD+=1
)
findstr /l /c:"?bf_malloc@memory@hfe@dice@@YAPAXIH@Z" /c:"?bf_free@memory@hfe@dice@@YAXPAX@Z" /c:"?addMemoryName@memory@hfe@dice@@YAHPBD@Z" "%OUT%\imports.txt" >nul
if not errorlevel 1 (
    echo   imports:      engine heap functions UNEXPECTED in native ABI
    set /a ABI_BAD+=1
)
findstr /i /c:"VCRUNTIME140.dll" "%OUT%\dependents.txt" >nul
if errorlevel 1 (
    echo   imports:      native shared VCRUNTIME140.dll MISSING
    set /a ABI_BAD+=1
)
findstr /i /c:"api-ms-win-crt-heap-" /c:"ucrtbase.dll" "%OUT%\dependents.txt" >nul
if errorlevel 1 (
    echo   imports:      native shared CRT heap MISSING
    set /a ABI_BAD+=1
)
if not "!ABI_BAD!"=="0" (
    echo   ABI CHECK FAILED - !ABI_BAD! invariant^(s^) violated
    exit /b 1
)
echo   exports and heap/CRT imports OK

echo.
echo   built: %BIN%\SwiffPlayer.dll
echo   NOTE: this is NOT installed anywhere. See docs\building.md before using this DLL with a client.
exit /b 0

:usage
echo Usage: swiffplayer.cmd [native^|x64]
exit /b 2
