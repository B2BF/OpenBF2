@echo off
setlocal EnableDelayedExpansion
rem ===========================================================================
rem SCAFFOLDING - NOT RECOVERED SOURCE. Builds the REPLACEMENT Memory.dll from
rem Code\BF2\Memory.
rem
rem No game image links or loads it: the game allocates through the shared CRT
rem (https://github.com/B2BF/OpenBF2/issues/7). The DLL exists for the
rem differential test against the retail Memory.dll
rem (Code\Tests\memory\memory_diff.cmd) and for a later decision on using it.
rem IT NEVER WRITES INTO THE GAME DIRECTORY.
rem
rem   usage:  memory.cmd [native]   x86, the retail-fidelity build
rem           memory.cmd x64        native x64 ([CHOSEN, not recovered] 16-byte header)
rem   Output: Code\Build\bin_memory_native\Memory.dll (bin_memory_x64 for x64).
rem   BF2_MEMORY_OUT and BF2_MEMORY_BIN override the object and DLL directories.
rem   Set BF2_MEMORY_PASSTHROUGH=1 to build the pass-through variant.
rem ===========================================================================
set "ARCH=x86"
if /i "%~1"=="x64" set "ARCH=x64"
if not "%~1"=="" if /i not "%~1"=="native" if /i not "%~1"=="x64" (
    echo usage: memory.cmd [native^|x64]
    exit /b 2
)
call "%~dp0vsdevcmd.cmd" %ARCH%
if errorlevel 1 exit /b 1

for %%I in ("%~dp0..") do set "CODE=%%~fI"
set "SRC=%CODE%\BF2\Memory"
set "OUT=%~dp0obj_memory_native"
set "BIN=%~dp0bin_memory_native"
if "%ARCH%"=="x64" set "OUT=%~dp0obj_memory_x64"
if "%ARCH%"=="x64" set "BIN=%~dp0bin_memory_x64"
if defined BF2_MEMORY_OUT set "OUT=%BF2_MEMORY_OUT%"
if defined BF2_MEMORY_BIN set "BIN=%BF2_MEMORY_BIN%"
set "PT="
if defined BF2_MEMORY_PASSTHROUGH set "PT=/DBF2_MEMORY_PASSTHROUGH=%BF2_MEMORY_PASSTHROUGH%"
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%BIN%" mkdir "%BIN%"

rem /MD - the shipped DLL uses the dynamic CRT (MSVCR71/MSVCP71).
set "CFLAGS=/nologo /c /O2 /MD /EHsc /W3 /GS- /DBF2_MEMORY_BUILD /D_CRT_SECURE_NO_WARNINGS %PT% /I"%SRC%""
set "OBJS="
for %%F in (AllocInfo Callstack ChunkPools Memory Histogram Threading MemoryTrace) do (
    cl %CFLAGS% "%SRC%\%%F.cpp" /Fo"%OUT%\%%F.obj"
    if errorlevel 1 exit /b 1
    set "OBJS=!OBJS! "%OUT%\%%F.obj""
)
rem /BASE - the shipped image's preferred base; cosmetic under ASLR.
link /nologo /DLL /OUT:"%BIN%\Memory.dll" /IMPLIB:"%OUT%\Memory.lib" /BASE:0x10000000 %OBJS% kernel32.lib user32.lib
if errorlevel 1 exit /b 1
echo [memory] built %BIN%\Memory.dll
exit /b 0
