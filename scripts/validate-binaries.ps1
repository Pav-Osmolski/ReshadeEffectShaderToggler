$ErrorActionPreference = 'Stop'
$x86 = "src/Win32/Release/ReshadeEffectShaderToggler.addon32"
$x64 = "src/x64/Release/ReshadeEffectShaderToggler.addon64"

function Get-PeMachine([string]$Path) {
  $stream = [System.IO.File]::OpenRead($Path)
  try {
    $reader = [System.IO.BinaryReader]::new($stream)
    $stream.Position = 0x3c
    $peOffset = $reader.ReadInt32()
    $stream.Position = $peOffset
    if ($reader.ReadUInt32() -ne 0x00004550) {
      throw "$Path does not contain a valid PE signature."
    }
    return $reader.ReadUInt16()
  }
  finally {
    $stream.Dispose()
  }
}

if ((Get-PeMachine $x86) -ne 0x014c) {
  throw "addon32 is not an i386/PE32 binary."
}
if ((Get-PeMachine $x64) -ne 0x8664) {
  throw "addon64 is not an AMD64/PE32+ binary."
}

$match = Select-String -Path "./src/version.h" -Pattern 'REST_VERSION_STRING "([^"]+)"'
$version = $match.Matches[0].Groups[1].Value

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
$dumpbin = Get-ChildItem "$vsPath\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe" |
  Sort-Object FullName -Descending |
  Select-Object -First 1 -ExpandProperty FullName
if (-not $dumpbin) {
  throw "Could not locate dumpbin.exe in the Visual Studio toolchain."
}

foreach ($binary in @($x86, $x64)) {
  $fileVersion = (Get-Item $binary).VersionInfo.FileVersion
  if ($fileVersion -ne $version) {
    throw "$binary reports version $fileVersion; expected $version."
  }

  $exports = (& $dumpbin /nologo /exports $binary | Out-String)
  foreach ($requiredExport in @("NAME", "DESCRIPTION")) {
    if ($exports -notmatch "(?i)\b$requiredExport\b") {
      throw "$binary is missing required export $requiredExport."
    }
  }
}

Write-Host "Validated x86/x64 PE architecture, version metadata and REST exports."

