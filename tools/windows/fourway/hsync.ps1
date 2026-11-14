# [TEMP] fourway comparison: mirror a Mac worktree into the guest build source by content, not by
# timestamp. Only files whose bytes differ are copied (and stamped now, so ninja rebuilds them);
# source files (.cpp .h .c .inc .mm) missing from the worktree are deleted; build outputs that tools
# write inside the tree (Code\Build\obj_*, ...) are kept. out\ and .git stay out.
# usage: hsync.ps1 -src \\Mac\Home\<worktree path> [-dst C:\rendbuild\src]
param([Parameter(Mandatory=$true)][string]$src, [string]$dst = 'C:\rendbuild\src')
$md5 = [Security.Cryptography.MD5]::Create()
function HashOf($p) { [BitConverter]::ToString($md5.ComputeHash([IO.File]::ReadAllBytes($p))) }
function Skip($rel) { $r = $rel.ToLower(); return $r.StartsWith('out\') -or $r.StartsWith('.git') }
$seen = @{}; $copied = 0; $now = Get-Date
Get-ChildItem -LiteralPath $src -Recurse -File -Force | ForEach-Object {
  $rel = $_.FullName.Substring($src.Length + 1)
  if (Skip $rel) { return }
  $seen[$rel.ToLower()] = 1
  $d = Join-Path $dst $rel
  $copy = $true
  if (Test-Path -LiteralPath $d) {
    if ((Get-Item -LiteralPath $d).Length -eq $_.Length) { $copy = (HashOf $_.FullName) -ne (HashOf $d) }
  }
  if ($copy) {
    New-Item -ItemType Directory -Force -Path (Split-Path $d) | Out-Null
    Copy-Item -LiteralPath $_.FullName -Destination $d -Force
    (Get-Item -LiteralPath $d).LastWriteTime = $now
    $copied++
    "copied $rel"
  }
}
Get-ChildItem -LiteralPath $dst -Recurse -File -Force | ForEach-Object {
  $rel = $_.FullName.Substring($dst.Length + 1)
  if (-not (Skip $rel) -and $rel -match '\.(cpp|h|c|inc|mm)$' -and -not $seen.ContainsKey($rel.ToLower())) { Remove-Item -LiteralPath $_.FullName -Force; "deleted $rel" }
}
"copied $copied"
