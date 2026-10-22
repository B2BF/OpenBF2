param([Parameter(Mandatory=$true)][string]$CodeDirectory,
      [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$code=(Resolve-Path -LiteralPath $CodeDirectory).Path.Replace('\','/')
$source=[IO.File]::ReadAllText("$code/BF2/RendDX9/Effect.cpp")
$start='ShaderParameter::ShaderParameter(ParameterOwner* owner)'
$end='// Reference-counted effect wrapper with a four-slot IBase interface.'
if (($source.Split(@($start),[StringSplitOptions]::None).Length -ne 2) -or
    ($source.Split(@($end),[StringSplitOptions]::None).Length -ne 2)) {
    throw 'ShaderParameter source anchors changed or are ambiguous.'
}
$first=$source.IndexOf($start);$last=$source.IndexOf($end)
if ($last -le $first) { throw 'ShaderParameter source anchors are out of order.' }
$body=$source.Substring($first,$last-$first)
if (-not $body.Contains('unsigned int ShaderParameter::getTypeCode() const')) {
    throw 'ShaderParameter extraction is incomplete.'
}
$prefix="// TEMP: verbatim ShaderParameter methods for the movie fixture.`n"+
    "#include `"$code/BF2/RendDX9/ShaderParameter.h`"`n"+
    "#include `"$code/RendDX9Abi/BfMemory.h`"`n"+
    "namespace dice { namespace hfe { namespace dx9rend {`n"
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'ShaderParameter.cpp'),
    $prefix+$body+"`n} } }`n",[Text.UTF8Encoding]::new($false))
