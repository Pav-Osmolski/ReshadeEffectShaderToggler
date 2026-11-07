param(
    [Parameter(Mandatory=$true)][string]$RenderDocHeaders,
    [Parameter(Mandatory=$true)][string]$RenderDocLibrary,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference='Stop'
$out=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force $out | Out-Null
$vswhere="${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw 'Visual C++ tools are required.' }
Import-Module "$vsPath/Common7/Tools/Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
& cl /nologo /EHsc /std:c++17 /W4 "$PSScriptRoot/dxil-tool.cpp" "/Fo$out/dxil-tool.obj" "/Fe$out/dxil-tool.exe"
if ($LASTEXITCODE -ne 0) { throw 'DXIL tool compile failed.' }
& cl /nologo /EHsc /std:c++17 /DRENDERDOC_PLATFORM_WIN32 /DNOMINMAX "/I$RenderDocHeaders" "$PSScriptRoot/replay-variants.cpp" $RenderDocLibrary "/Fo$out/replay-variants.obj" "/Fe$out/replay-variants.exe"
if ($LASTEXITCODE -ne 0) { throw 'Replay tool compile failed.' }
& cl /nologo /EHsc /std:c++17 /W4 "$PSScriptRoot/split-test.cpp" d3d12.lib "/Fo$out/split-test.obj" "/Fe$out/split-test.exe"
if ($LASTEXITCODE -ne 0) { throw 'D3D12 split test compile failed.' }
