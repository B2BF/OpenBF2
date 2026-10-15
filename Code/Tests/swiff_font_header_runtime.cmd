@echo off
setlocal
rem [TEMP] Native x86 regression for the production Swiff .dif Tokenizer/DifFont::load path.
call "%~dp0..\Build\vsdevcmd.cmd" x86
call "%~dp0..\Build\dxsdk.cmd" x86
if errorlevel 1 exit /b 1
for %%I in ("%~dp0..") do set "TASK_CODE=%%~fI"
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%TASK_CODE%\Build\swiff_font_header_runtime_20260917"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"

set "TASK_FLAGS=/nologo /c /MD /std:c++14 /permissive /EHsc /Od /Z7 /Gy /Gw /Zc:inline /DDEDICATED=0 /DBF2_IMAGE_RENDDX9 /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0 /FI%TASK_CODE%\BF2\Core\BuildConfig.h"

cl %TASK_FLAGS% "%~dp0swiff_font_header_runtime.cpp" /Fo"%BF2_BUILD_OUT%\fixture.obj" /Fd"%BF2_BUILD_OUT%\compile.pdb" >"%BF2_BUILD_OUT%\compile_fixture.log" 2>&1
if errorlevel 1 (type "%BF2_BUILD_OUT%\compile_fixture.log" & exit /b 1)

cl %TASK_FLAGS% "%TASK_CODE%\RendDX9Abi\BfMemory.cpp" /Fo"%BF2_BUILD_OUT%\BfMemory.obj" /Fd"%BF2_BUILD_OUT%\compile.pdb" >"%BF2_BUILD_OUT%\compile_memory.log" 2>&1
if errorlevel 1 (type "%BF2_BUILD_OUT%\compile_memory.log" & exit /b 1)

link /nologo /MACHINE:X86 /DEBUG /OPT:REF /INCREMENTAL:NO /SUBSYSTEM:CONSOLE ^
  /OUT:"%BF2_BUILD_OUT%\swiff_font_header_runtime.exe" ^
  /PDB:"%BF2_BUILD_OUT%\swiff_font_header_runtime.pdb" ^
  "%BF2_BUILD_OUT%\fixture.obj" "%BF2_BUILD_OUT%\BfMemory.obj" ^
  >"%BF2_BUILD_OUT%\link.log" 2>&1
set "TASK_LINK_RESULT=%errorlevel%"
if not "%TASK_LINK_RESULT%"=="0" (type "%BF2_BUILD_OUT%\link.log" & exit /b %TASK_LINK_RESULT%)

"%BF2_BUILD_OUT%\swiff_font_header_runtime.exe" >"%BF2_BUILD_OUT%\runtime.log" 2>&1
set "TASK_RUNTIME_RESULT=%errorlevel%"
type "%BF2_BUILD_OUT%\runtime.log"
exit /b %TASK_RUNTIME_RESULT%
