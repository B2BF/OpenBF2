@echo off
setlocal
rem [CHOSEN, not recovered] Builds and runs sm1_jump_lowering_win.cpp: our vkd3d bytecode for the
rem SM1/2 jump lowering cases (sm1_jump_lowering_inputs.h, from gen_inputs.py) through the real
rem D3DX validator (disassemble + assemble) and the real runtime (CreateVertexShader/PixelShader),
rem plus D3DX's own compile of the same HLSL. Writes sm1_jump_lowering_result.txt next to this
rem script. Needs MSVC (vsdevcmd x86), d3d9.lib and a d3dx9 DLL at run time: optional first
rem argument or %D3DX9_DLL% (path to e.g. the retail d3dx9_25.dll), else d3dx9_25..43 on the path.
call "%~dp0..\..\Build\vsdevcmd.cmd" x86
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%~dp0..\..\Build\obj_sm1_jump_lowering"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"
cl /nologo /MD /EHsc /O2 "%~dp0sm1_jump_lowering_win.cpp" /Fo"%BF2_BUILD_OUT%\\" /Fe"%BF2_BUILD_OUT%\sm1_jump_lowering.exe" d3d9.lib user32.lib >"%BF2_BUILD_OUT%\compile.log" 2>&1
if errorlevel 1 (
  type "%BF2_BUILD_OUT%\compile.log"
  exit /b 1
)
"%BF2_BUILD_OUT%\sm1_jump_lowering.exe" "%~dp0sm1_jump_lowering_result.txt" %1
exit /b %errorlevel%
