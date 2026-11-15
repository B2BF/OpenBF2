@echo off
setlocal
rem [TEMP] MSVC check (game-frontier batch 16 N1): the instruction right after the imported
rem NovintHFX::ControlBoxEffectParams constructor call must store FalconControlBoxParams' local
rem vtable, as BF2.exe does at 0x6A58A1 (mov dword ptr [esi], 0x9116E0). Compiles FalconDevice.cpp for x86 /O2,
rem disassembles FalconDevice::updateCurrentState and prints PASS or FAIL with the lines it saw.
rem N2 also checks all Constant/Envelope constructor calls for the immediate local-vtable store:
rem at least three Constant sites are required; Envelope sites are optional until used.
rem Writes only under BF2_BUILD_OUT; no DLL is loaded and nothing runs.
call "%~dp0..\Build\vsdevcmd.cmd" x86
set "TASK_ROOT=%~dp0..\.."
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%~dp0..\Build\obj_falcon_vtable_restore"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"
set "TASK_FLAGS=/nologo /O2 /EHsc /std:c++14 /permissive /MD /Gy /DDEDICATED=0 /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0"
cl %TASK_FLAGS% /FI"%TASK_ROOT%\Code\BF2\Core\BuildConfig.h" /c "%TASK_ROOT%\Code\BF2\IO\Input\FalconDevice.cpp" /Fo"%BF2_BUILD_OUT%\FalconDevice.obj" >"%BF2_BUILD_OUT%\compile.log" 2>&1
if errorlevel 1 (type "%BF2_BUILD_OUT%\compile.log" & echo FAIL: compile & exit /b 1)
dumpbin /nologo /disasm "%BF2_BUILD_OUT%\FalconDevice.obj" >"%BF2_BUILD_OUT%\disasm.txt"
if errorlevel 1 (echo FAIL: dumpbin & exit /b 1)
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$l = Get-Content -LiteralPath '%BF2_BUILD_OUT%\disasm.txt';" ^
  "$allPass = $true; $s = -1; for ($i = 0; $i -lt $l.Count; $i++) { if ($l[$i] -match '^\?updateCurrentState@FalconDevice@io@hfe@dice@@') { $s = $i; break } }" ^
  "if ($s -lt 0) { Write-Output 'FAIL: ControlBoxEffectParams: updateCurrentState not found in the disassembly'; $allPass = $false } else {" ^
  "$e = $l.Count; for ($i = $s + 1; $i -lt $l.Count; $i++) { if ($l[$i] -match '^\?\S+:$' -or $l[$i] -match '^_\S+:$') { $e = $i; break } }" ^
  "$c = -1; for ($i = $s; $i -lt $e; $i++) { if ($l[$i] -match '__imp_\?\?0ControlBoxEffectParams@NovintHFX@@QAE@XZ') { $c = $i; break } }" ^
  "if ($c -lt 0) { Write-Output 'FAIL: no call of the imported ControlBoxEffectParams constructor'; $allPass = $false } else {" ^
  "$hit = $null; if ($l[$c + 1] -match 'mov\s+dword ptr \[\w+\],(offset )?\?\?_7FalconControlBoxParams@') { $hit = $l[$c + 1] }" ^
  "Write-Output '--- after the constructor call:'; $l[$c..([Math]::Min($c + 8, $e - 1))] | Write-Output;" ^
  "if ($hit) { Write-Output ('PASS: FalconControlBoxParams vtable stored right after the imported constructor: ' + $hit.Trim()) }" ^
  "else { Write-Output 'FAIL: the instruction after the ControlBoxEffectParams constructor call is not the store of ??_7FalconControlBoxParams'; $allPass = $false } } }" ^
  "foreach ($rule in @(@{ Base = 'ConstantEffectParams'; Derived = 'FalconConstantParams'; Minimum = 3 }, @{ Base = 'EnvelopeEffectParams'; Derived = 'FalconEnvelopeParams'; Minimum = 0 })) {" ^
  "$callPattern = '\bcall\s+.*' + [regex]::Escape('__imp_??0' + $rule.Base + '@NovintHFX@@QAE@XZ');" ^
  "$storePattern = 'mov\s+dword ptr \[\w+\],(offset )?' + [regex]::Escape('??_7' + $rule.Derived + '@');" ^
  "$count = 0; $bad = 0; $function = '<unknown function>';" ^
  "for ($i = 0; $i -lt $l.Count; $i++) {" ^
  "if ($l[$i] -match '^(\?\S+):$') { $function = $Matches[1] }" ^
  "if ($l[$i] -notmatch $callPattern) { continue }; $count++;" ^
  "$next = '<end of disassembly>'; if ($i + 1 -lt $l.Count) { $next = $l[$i + 1] }" ^
  "Write-Output ($rule.Base + ' site in ' + $function + ':'); Write-Output $next;" ^
  "if ($next -notmatch $storePattern) { $bad++ } }" ^
  "if ($count -eq 0 -and $rule.Base -eq 'EnvelopeEffectParams') { Write-Output 'no EnvelopeEffectParams sites yet' }" ^
  "if ($count -ge $rule.Minimum -and $bad -eq 0) { Write-Output ('PASS: ' + $rule.Base + ': ' + $count + ' sites, all immediate vtable stores valid') }" ^
  "else { Write-Output ('FAIL: ' + $rule.Base + ': ' + $count + ' sites (minimum ' + $rule.Minimum + '), ' + $bad + ' invalid immediate vtable stores'); $allPass = $false } }" ^
  "if ($allPass) { Write-Output 'PASS: all Falcon vtable restore rules'; exit 0 }" ^
  "Write-Output 'FAIL: Falcon vtable restore rules'; exit 1"
exit /b %errorlevel%
