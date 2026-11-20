# [TEMP] [CHOSEN, not recovered] Extract the unchanged method text into the bounded fixture.
param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
function Get-FunctionText([string]$source, [string]$signature) {
    $start = $source.IndexOf($signature)
    if ($start -lt 0) { throw "Missing $signature" }
    $brace = $source.IndexOf('{', $start)
    $depth = 1
    $end = $brace + 1
    while ($depth -gt 0 -and $end -lt $source.Length) {
        if ($source[$end] -eq '{') { ++$depth }
        if ($source[$end] -eq '}') { --$depth }
        ++$end
    }
    if ($depth) { throw "Incomplete $signature body" }
    return $source.Substring($start, $end - $start)
}
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$log = Get-Content (Join-Path $PSScriptRoot '../BF2/Game/Main/BF2Log.cpp') -Raw
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'crash_log_body.inc'),
    (Get-FunctionText $log 'void BF2Log::addLogEntry('))
# The Win32 entry's dump-name builder and SEH frame, for the end-to-end access-violation case.
$main = Get-Content (Join-Path $PSScriptRoot '../BF2/Game/Main/BF2.cpp') -Raw
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'crash_startup_body.inc'),
    (Get-FunctionText $main 'static const wchar_t* crashDumpFileName(') + "`n" +
    (Get-FunctionText $main 'static int bf2WinMain(') + "`n")
