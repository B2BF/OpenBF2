@echo off
setlocal EnableDelayedExpansion
rem ===========================================================================
rem SCAFFOLDING - NOT RECOVERED SOURCE. Builds the rebuilt AIDLL_w32ded.dll.
rem
rem   Code\Build\build.cmd        the dedicated server, bf2_w32ded.exe
rem   Code\Build\aidll.cmd        THIS. The bot-AI side-DLL the server loads.
rem   Code\Build\renddx9.cmd      the replacement RendDX9.dll   (CLIENT image)
rem   Code\Build\swiffplayer.cmd  the replacement SwiffPlayer.dll (CLIENT image)
rem
rem IT NEVER WRITES INTO THE GAME DIRECTORY. Output goes to Code\Build\bin_aidll\.
rem   "D:\B2BF Launcher\Battlefield2\AIDLL_w32ded.dll", its .i64, AIDLL.dll and libaidll.so are
rem   the REFERENCE BINARIES and must never be overwritten. Pointing the server at the rebuilt DLL
rem   is done with a RELATIVE AIDLL setting back into this directory - never by copying.
rem
rem VALIDATE AIDLL FILES ONLY WITH THIS SCRIPT, RUN VIA THE PowerShell TOOL.
rem   Code\Tests\syntaxcheck.cmd parses the same TUs, but it cannot see a missing definition and it
rem   does not link a DLL. Git-Bash `cmd /c` returns 0 without building.
rem
rem   A diagnostic link may load the DLL and stop at a named frontier.
rem
rem   usage:  aidll.cmd            compile the manifest, strict link (the gap report), /FORCE link
rem           aidll.cmd <file>     one translation unit, compile check only
rem
rem   env:    BF2_AIDLL_OUT        per-agent object directory  (default Code\Build\obj_aidll)
rem           BF2_AIDLL_BIN        per-agent output directory  (default Code\Build\bin_aidll)
rem ===========================================================================
call "%~dp0vsdevcmd.cmd" x86

for %%I in ("%~dp0..") do set CODE=%%~fI
set ROOT=%CODE%\BF2
set ABI=%CODE%\AidllAbi
set OUT=%~dp0obj_aidll
set BIN=%~dp0bin_aidll
if defined BF2_AIDLL_OUT set OUT=%BF2_AIDLL_OUT%
if defined BF2_AIDLL_BIN set BIN=%BF2_AIDLL_BIN%

rem ---------------------------------------------------------------------------
rem THE HEAP DECISION - /MD, THE SHARED CRT.  [CHOSEN, not recovered]
rem [RETIRE-WHEN: aidll-00-heap]
rem
rem The retail pair shares TWO heaps: the CRT's, through MSVCR71.dll, which both images import; and
rem the engine's, through Memory_w32ded.dll, whose bf_malloc/bf_free/addMemoryName are three of the
rem AI DLL's imports. This rebuild has NEITHER of those DLLs: bf2_w32ded.exe is /MD against the
rem toolchain CRT (Code\Build\build.cmd, corrected 2026-08-22 for exactly this reason - one CRT per
rem image, shared with dice_py.dll) and has no bf_malloc at all.
rem
rem So the faithful analogue is /MD here too. Exe and DLL then bind the SAME ucrtbase/vcruntime, one
rem `operator new` and one heap, and a std::string built in the DLL can be destroyed in the exe.
rem
rem (!) /MT WOULD BE A REAL BUG, NOT A STYLE CHOICE. It gives this DLL a PRIVATE CRT - its own heap and
rem its own FILE table - and every object that crosses the boundary would be freed on the wrong heap.
rem SwiffPlayer.dll IS built /MT (Code\Build\swiffplayer.cmd) and that is not a precedent: that image
rem reroutes its allocations to the shipped Memory.dll and talks to a RETAIL exe it does not build.
rem This DLL is a native rebuild talking to a native rebuild.
rem
rem (!) AND THE STL LAYOUT HAS TO MATCH TOO, which /MD alone does not guarantee. `initDll` hands over
rem ClassManager and the AI then calls getSimpleSingleton(std::string) across the boundary, so both
rem images must agree on sizeof(std::string) and on iterator debugging. Same toolchain, same
rem /std:c++14, same _ITERATOR_DEBUG_LEVEL - see the flag block below.
rem
rem Retire when the engine recovers memory::bf_malloc and the Memory DLL: at that point both images
rem allocate through the engine heap and this comment becomes the record of an interim.
rem ---------------------------------------------------------------------------

rem The CPython 2.3.4 and zlib include roots, identical to build.cmd's. Harmless for the TUs that
rem include neither, and required by whatever engine TU the DLL manifest grows into.
set PYINC=/I"%ROOT%\External\python-embedded\py-src-2.3.4\Include" /I"%ROOT%\External\python-embedded\py-src-2.3.4\PC"
set ZIPINC=/I"%ROOT%\External\zlib-1.2.3" /I"%ROOT%\External\zlib-1.2.3\contrib\minizip"

rem /DDEDICATED=1 + /FI Core\BuildConfig.h - THE IMAGE DEFINE. AIDLL_w32ded.dll is a SERVER-side
rem image: it is loaded by bf2_w32ded.exe and never by BF2.exe (the client's bot DLL is the separate
rem AIDLL.dll, out of scope).
rem
rem /DBF2_IMAGE_AIDLL - the image tag, matching /DBF2_IMAGE_SWIFFPLAYER and /DBF2_IMAGE_RENDDX9.
rem Math\Mat4Inverse.cpp reads it to expose the PE-identical client/AIDLL x86 body while keeping the
rem dedicated engine and platform-distinct ELF implementations separate. It also makes every compile
rem line say which of the three server-side images it is building.
rem
rem The rest of the flag set is build.cmd's, verbatim, because these objects must be
rem interchangeable with the engine's: same /MD, same /std:c++14, same /permissive, same /EHsc.
set FLAGS=/nologo /c /MD /std:c++14 /permissive /EHsc /W3 /wd4996 /Zi /FS /Fd"%OUT%\AIDLL_w32ded.pdb" %PYINC% %ZIPINC% /DDEDICATED=1 /DBF2_IMAGE_AIDLL /FI"%ROOT%\Core\BuildConfig.h"

if not exist "%OUT%" mkdir "%OUT%"
if not exist "%BIN%" mkdir "%BIN%"

rem Single-file compile check. Writes one throwaway object and deletes it, exactly like build.cmd's
rem single-file path - a leftover unnumbered .obj would become a SECOND definition at link time.
if not "%~1"=="" (
    cl %FLAGS% /Fo"%OUT%\_check.obj" "%~1"
    set RC=!errorlevel!
    del /q "%OUT%\_check.obj" >nul 2>&1
    exit /b !RC!
)

del /q "%OUT%\*.obj" >nul 2>&1
del /q "%OUT%\*.log" >nul 2>&1

rem ===========================================================================
rem THE DLL MANIFEST. An EXPLICIT LIST, never a glob - the source tree is broader than any one
rem image, and this list IS the image boundary for AIDLL_w32ded.dll exactly as ENGINE_OBJECT is for
rem bf2_w32ded.exe in build.cmd.
rem
rem   group 1  the module entry point - engine code COMPILED INTO EVERY SIDE-DLL, never into the exe
rem            (Core\DllModule.cpp; build.cmd line 111 and CMakeLists.txt both exclude it on purpose)
rem   group 2  the AIDLL-only AI translation units
rem   group 3  translation units shared with the exe - present in BOTH shipped images
rem   group 4  engine translation units the retail DLL STATICALLY LINKS
rem            (census\00_statically_linked_engine_code.tsv: 573 functions, io::File, Frustum,
rem            ConsoleObjects, the Vec2/Vec3 geometry helpers, Mat4/Mat3, Debug, ...). This group is
rem            LINK-DRIVEN: a TU joins it when the strict link below names a symbol it defines, and
rem            the reference census says whether the retail DLL really carried it.
rem            ★ `IO\File\File.cpp` JOINED THIS GROUP ON 2026-09-11, by exactly that rule:
rem            AerialHeightMap's two file paths named twelve io::File symbols in the strict
rem            link, and the census already listed io::File as statically linked. Confirmed
rem            against the reference DLL, which imports NOTHING from bf2_w32ded.exe yet
rem            carries the "/AIPathFinding/AerialHeighMap.ahm" literal. [PLATFORM]:
rem            libaidll.so resolves the same symbols against the executable instead.
rem            ★ `IO\Input\PlayerInput.cpp` JOINED ON 2026-09-28 by the same rule: the
rem            plan executor and BotMain::updatePlayerAction named exactly one symbol,
rem            Input<PlayerInputMap,64>::setInput(PlayerInputMap,float), which the census lists
rem            at 0x18F020 (with setInputIsMapped and the bool overload). [PLATFORM]-free, but
rem            NOT identical: the shared TU also instantiates Input<GameInputMap,74>, which
rem            the retail DLL does not carry - dead code here, recorded rather than split.
rem ===========================================================================
rem The source list is Code\Build\aidll_sources.txt (shared with CMake; one path per line, link order).
rem It is read line by line: one 440-file "set SOURCES=" line exceeds cmd.exe's 8191-character limit.
set MANIFEST=%~dp0aidll_sources.txt

echo [1/4] compiling
set OK=0
set BAD=0
set /a OBJN=0
set OBJ_RSP=%OUT%\aidll_objects.rsp
type nul > "%OBJ_RSP%"
for /f "usebackq eol=# tokens=* delims=" %%L in ("%MANIFEST%") do (
    set "REL=%%L"
    set "SRC=%ROOT%\!REL:/=\!"
    for %%F in ("!SRC!") do (
        set /a OBJN+=1
        cl %FLAGS% /Fo"%OUT%\!OBJN!_%%~nF.obj" "%%~fF" >"%OUT%\cl_%%~nF.log" 2>&1
        if errorlevel 1 (
            set /a BAD+=1
            echo   COMPILE FAIL %%~nxF
            type "%OUT%\cl_%%~nF.log"
        ) else (
            set /a OK+=1
            echo "%OUT%\!OBJN!_%%~nF.obj">>"%OBJ_RSP%"
        )
    )
)
echo   %OK% compiled, %BAD% failed
if not "%BAD%"=="0" exit /b 1

rem ===========================================================================
rem STEP 2 - THE STRICT LINK. This is the DIAGNOSTIC, and it is EXPECTED TO FAIL while the DLL
rem manifest is incomplete: its unresolved-external list is the work list, exactly as build.cmd's
rem is for the exe. Compare symbol SETS between runs, not totals (AGENTS.md).
rem ===========================================================================
echo [2/4] strict link - the gap report
link /nologo /DLL /MACHINE:X86 /NODEFAULTLIB:python23.lib ^
     /OUT:"%OUT%\AIDLL_w32ded_strict.dll" ^
     /DEF:"%ABI%\AIDLL_w32ded.def" ^
     /IMPLIB:"%OUT%\AIDLL_w32ded_strict.lib" ^
     @"%OBJ_RSP%" kernel32.lib user32.lib advapi32.lib Ws2_32.lib > "%OUT%\link_strict.log" 2>&1
find /c "unresolved external symbol" < "%OUT%\link_strict.log" > "%OUT%\count.txt"
set /p NRAW=<"%OUT%\count.txt"
set "NFINAL=0"
set "NSUMMARY="
for /f "delims=" %%L in ('findstr /c:"fatal error LNK1120:" "%OUT%\link_strict.log"') do set "NSUMMARY=%%L"
if defined NSUMMARY for /f "tokens=1" %%N in ("!NSUMMARY:*LNK1120:=!") do set "NFINAL=%%N"
echo   unresolved diagnostics: !NRAW!
echo   unique unresolved externals: !NFINAL!
echo   full list: %OUT%\link_strict.log

rem ===========================================================================
rem STEP 3 - THE /FORCE LINK. This is the image the harness LOADS. Every unwritten function links at
rem RVA 0 and faults BY NAME when the DLL first calls it - the project's own measurement, the same
rem instrument bf2_w32ded.exe is built with. /DEBUG + /MAP so a fault resolves to file:line.
rem
rem (!) /FORCE PROVES NOTHING ABOUT LINK COMPLETENESS. A force-linked DLL proves only the path it
rem actually executed (AGENTS.md). Report the strict count above, never this step's success.
rem
rem /BASE:0x10000000 matches the shipped image's preferred base. Cosmetic under ASLR; it keeps an
rem address diff against AIDLL_w32ded.dll.i64 readable, which is chunk 12's whole job.
rem ===========================================================================
echo [3/4] /FORCE link - the loadable image
link /nologo /DLL /MACHINE:X86 /FORCE /DEBUG /INCREMENTAL:NO /BASE:0x10000000 /NODEFAULTLIB:python23.lib ^
     /OUT:"%BIN%\AIDLL_w32ded.dll" ^
     /DEF:"%ABI%\AIDLL_w32ded.def" ^
     /PDB:"%BIN%\AIDLL_w32ded.pdb" ^
     /IMPLIB:"%OUT%\AIDLL_w32ded.lib" ^
     /MAP:"%BIN%\AIDLL_w32ded.map" ^
     /IGNORE:4006,4088,4098 ^
     @"%OBJ_RSP%" kernel32.lib user32.lib advapi32.lib Ws2_32.lib > "%OUT%\link_force.log" 2>&1
if not exist "%BIN%\AIDLL_w32ded.dll" (
    echo   FORCE LINK FAILED - see "%OUT%\link_force.log"
    type "%OUT%\link_force.log"
    exit /b 1
)

rem ===========================================================================
rem STEP 4 - the ABI checks that matter, run every build. findstr, NOT find: find is shadowed by the
rem POSIX toolchain when this script is launched from Git Bash and misreports a correct DLL.
rem ===========================================================================
echo [4/4] ABI check
dumpbin /nologo /exports "%BIN%\AIDLL_w32ded.dll" > "%OUT%\exports.txt"
dumpbin /nologo /dependents "%BIN%\AIDLL_w32ded.dll" > "%OUT%\dependents.txt"

findstr /c:"initDll" "%OUT%\exports.txt" >nul && findstr /c:"deinitDll" "%OUT%\exports.txt" >nul
if not errorlevel 1 (echo   exports:      initDll + deinitDll  OK) else (echo   exports:      MISSING - LoadLibraryBf resolves initDll by name)
findstr /c:"Memory_w32ded.dll" "%OUT%\dependents.txt" >nul
if errorlevel 1 (echo   heap:         no Memory_w32ded.dll dependency  OK - shared CRT, see the heap block) else (echo   heap:         Memory_w32ded.dll present - NOT the chosen heap for this rebuild)

echo.
echo   built: %BIN%\AIDLL_w32ded.dll
echo   NOTE: not installed anywhere. Point the AIDLL setting at it with a RELATIVE path;
echo         never copy it into the game folder. Use a separate test directory.
if "%NRAW%"=="0" (echo   STRICT LINK CLEAN & exit /b 0)
exit /b 0
