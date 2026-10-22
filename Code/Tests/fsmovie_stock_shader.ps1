param([Parameter(Mandatory=$true)][string]$GameDirectory,
      [Parameter(Mandatory=$true)][string]$OutputFile)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=[IO.Compression.ZipFile]::OpenRead((Join-Path $GameDirectory 'mods/xpack/Shaders_client.zip'))
try {
    $entry=$archive.GetEntry('LoadingScreen.fx')
    if (-not $entry) { throw 'Stock LoadingScreen.fx is missing.' }
    $source=$entry.Open()
    try {
        $output=[IO.File]::Open($OutputFile,[IO.FileMode]::CreateNew)
        try { $source.CopyTo($output) } finally { $output.Dispose() }
    } finally { $source.Dispose() }
} finally { $archive.Dispose() }
