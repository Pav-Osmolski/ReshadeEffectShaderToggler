$ErrorActionPreference = 'Stop'
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
Import-Module "$vsPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
New-Item -ItemType Directory -Force tests/out | Out-Null
cl /nologo /EHsc /std:c++20 /W4 /WX /Isrc /Ideps/reshade/include tests/RenderingSafetyTests.cpp /Fotests/out/ /Fetests/out/rendering-safety.exe
if ($LASTEXITCODE -ne 0) { throw 'Rendering safety tests failed to compile.' }
& ./tests/out/rendering-safety.exe
if ($LASTEXITCODE -ne 0) { throw 'Rendering safety tests failed.' }
Write-Output 'Rendering safety regression tests passed.'
