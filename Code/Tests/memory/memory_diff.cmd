@echo off
setlocal
rem ===========================================================================
rem [TEMP] SCAFFOLDING. Differential test of Memory.dll: runs memory_diff.exe
rem against the retail Memory.dll and against the rebuilt MSVC x86 DLL and
rem compares the two outputs.
rem
rem   usage:  memory_diff.cmd <retail dir>
rem   <retail dir> holds the retail Memory.dll with its msvcr71.dll and
rem   msvcp71.dll; they are copied into the work directory, never modified.
rem   BF2_MEMORY_DIFF_OUT overrides the work directory
rem   (default Code\Tests\memory\obj_memory_diff).
rem ===========================================================================
if "%~1"=="" (
    echo usage: memory_diff.cmd ^<retail dir^>
    exit /b 2
)
set "RETAIL=%~1"
set "WORK=%~dp0obj_memory_diff"
if defined BF2_MEMORY_DIFF_OUT set "WORK=%BF2_MEMORY_DIFF_OUT%"
if not exist "%WORK%\retail" mkdir "%WORK%\retail"
if not exist "%WORK%\rebuilt" mkdir "%WORK%\rebuilt"

set "BF2_MEMORY_OUT=%WORK%\obj"
set "BF2_MEMORY_BIN=%WORK%\rebuilt"
call "%~dp0..\..\Build\memory.cmd" native
if errorlevel 1 exit /b 1

rem memory.cmd keeps its developer environment local; load it again for the harness.
call "%~dp0..\..\Build\vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1

copy /y "%RETAIL%\Memory.dll" "%WORK%\retail\Memory.dll" >nul || exit /b 1
copy /y "%RETAIL%\msvcr71.dll" "%WORK%\retail\msvcr71.dll" >nul || exit /b 1
copy /y "%RETAIL%\msvcp71.dll" "%WORK%\retail\msvcp71.dll" >nul || exit /b 1

cl /nologo /O2 /MD /EHsc /W3 /D_CRT_SECURE_NO_WARNINGS "%~dp0memory_diff.cpp" /Fo"%WORK%\memory_diff.obj" /Fe"%WORK%\memory_diff.exe"
if errorlevel 1 exit /b 1

pushd "%WORK%\retail"
"%WORK%\memory_diff.exe" "%WORK%\retail\Memory.dll" > "%WORK%\retail.txt"
set "RC_RETAIL=%errorlevel%"
popd
pushd "%WORK%\rebuilt"
"%WORK%\memory_diff.exe" "%WORK%\rebuilt\Memory.dll" > "%WORK%\rebuilt.txt"
set "RC_REBUILT=%errorlevel%"
popd
echo [memory_diff] exit codes: retail %RC_RETAIL%, rebuilt %RC_REBUILT%
fc /n "%WORK%\retail.txt" "%WORK%\rebuilt.txt"
if errorlevel 1 (
    echo [memory_diff] OUTPUTS DIFFER
    exit /b 1
)
echo [memory_diff] outputs identical
exit /b 0
