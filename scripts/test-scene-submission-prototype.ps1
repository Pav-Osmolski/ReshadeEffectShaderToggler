$ErrorActionPreference = 'Stop'
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
Import-Module "$vsPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
New-Item -ItemType Directory -Force tests/out/submission | Out-Null
cl /nologo /EHsc /std:c++20 /W4 /WX /DNOMINMAX tests/SceneSubmissionPrototypeTests.cpp /Fotests/out/submission/ /Fetests/out/submission/prototype.exe
if ($LASTEXITCODE -ne 0) { throw 'Submission research prototype failed to compile.' }
& ./tests/out/submission/prototype.exe
if ($LASTEXITCODE -ne 0) { throw 'Submission research prototype regression.' }
# Capture assets stay external; CI compiles the replay tool but cannot replay them.
cl /nologo /EHsc /std:c++17 /W4 /WX tools/compositor-probe/split-test.cpp d3d12.lib /Fotests/out/submission/captured-replay.obj /Fetests/out/submission/captured-replay.exe
if ($LASTEXITCODE -ne 0) { throw 'Captured compositor replay tool failed to compile.' }
