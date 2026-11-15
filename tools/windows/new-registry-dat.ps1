<#
.SYNOPSIS
    Writes a test Registry.dat for the OpenBF2 Windows client (docs/registry-file.md).

.DESCRIPTION
    Development tool. The Windows installer is the production writer. The text is the
    Registry.con syntax (one `HIVE\path\name = value` line per value), UTF-8 without a
    byte-order mark, protected with DPAPI for the current user (CryptProtectData,
    CurrentUser scope, no entropy, no UI) and written as the raw blob.

    Give either -CdKey (and optionally -XpackCdKey) or -InputFile with ready-made text.
    Use fake keys for tests. The keys are never printed.

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\windows\new-registry-dat.ps1 -CdKey FAKE-0000-1111-2222-3333

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\windows\new-registry-dat.ps1 -InputFile Registry.con -Path "$env:TEMP\Registry.dat"
#>
[CmdletBinding(DefaultParameterSetName = 'Keys')]
param(
    [Parameter(Mandatory = $true, ParameterSetName = 'Keys')]
    [string] $CdKey,
    [Parameter(ParameterSetName = 'Keys')]
    [string] $XpackCdKey,
    [Parameter(Mandatory = $true, ParameterSetName = 'Text')]
    [string] $InputFile,
    # Defaults to the client's location, %LOCALAPPDATA%\OpenBF2\Registry.dat.
    [string] $Path = (Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'OpenBF2\Registry.dat')
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Security

if ($PSCmdlet.ParameterSetName -eq 'Text') {
    $text = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $InputFile).Path)
} else {
    $base = 'HKEY_LOCAL_MACHINE\SOFTWARE\Electronic Arts\EA Games\'
    $text = "${base}Battlefield 2\ergc = $CdKey`r`n"
    if ($XpackCdKey) {
        $text += "${base}Battlefield 2 Special Forces\ergc = $XpackCdKey`r`n"
    }
}

$plain = (New-Object System.Text.UTF8Encoding $false).GetBytes($text)
$blob = [Security.Cryptography.ProtectedData]::Protect(
    $plain, $null, [Security.Cryptography.DataProtectionScope]::CurrentUser)
[Array]::Clear($plain, 0, $plain.Length)

$directory = Split-Path -Parent $Path
if ($directory) { New-Item -ItemType Directory -Force -Path $directory | Out-Null }
[IO.File]::WriteAllBytes($Path, $blob)
Write-Host "Wrote $Path ($($blob.Length) bytes)"
