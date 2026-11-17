param(
    [Parameter(Mandatory=$true)][string]$CaptureRoot,
    [Parameter(Mandatory=$true)][string]$ShaderRoot,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference='Stop'
$inputRoot=(Resolve-Path -LiteralPath $CaptureRoot).Path.TrimEnd('\','/')
$shaderPath=(Resolve-Path -LiteralPath $ShaderRoot).Path
$out=[IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\','/')
if ($out.Equals($inputRoot,[StringComparison]::OrdinalIgnoreCase) -or
    $out.StartsWith($inputRoot + '\',[StringComparison]::OrdinalIgnoreCase) -or
    $inputRoot.StartsWith($out + '\',[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Use an independent output directory outside the captured inputs.'
}
New-Item -ItemType Directory -Force $out | Out-Null
$vswhere="${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw 'Visual C++ tools are required.' }
Import-Module "$vsPath/Common7/Tools/Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
& cl /nologo /EHsc /std:c++17 /W4 /WX "$PSScriptRoot/split-test.cpp" d3d12.lib "/Fo$out/split-test.obj" "/Fe$out/split-test.exe"
if ($LASTEXITCODE -ne 0) { throw 'Captured compositor replay failed to compile.' }
foreach ($mode in @('taa','no-taa','fsr')) {
    & "$out/split-test.exe" "$inputRoot/$mode" "$shaderPath/replay-vertex.dxil" "$shaderPath/tint-scene.dxil" "$shaderPath/overwrite-alpha.dxil" --submission-replay "$out/$mode"
    if ($LASTEXITCODE -ne 0) { throw "$mode captured compositor replay failed." }
    $identity=(Get-FileHash -LiteralPath "$out/$mode/runtime-identity.raw" -Algorithm SHA256).Hash
    $original=(Get-FileHash -LiteralPath "$inputRoot/$mode/original.raw" -Algorithm SHA256).Hash
    $tint=(Get-FileHash -LiteralPath "$out/$mode/runtime-tint-overwrite-alpha.raw" -Algorithm SHA256).Hash
    $priorTint=(Get-FileHash -LiteralPath "$inputRoot/$mode/runtime-tint-overwrite-alpha.raw" -Algorithm SHA256).Hash
    if ($identity -ne $original -or $tint -ne $priorTint) { throw "$mode differs from saved capture or prior validated tint." }
    Write-Output "${mode}: 32 live recordings / three reordered submissions, bit-exact saved reference checks passed."
}
