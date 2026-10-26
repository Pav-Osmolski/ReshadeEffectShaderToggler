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

# Link the real production registration functions to a mock ReShade host. This
# catches forced-off registration even when isolated snapshot helper tests pass.
$compilerVersion = [version](Get-Item (Get-Command cl.exe).Source).VersionInfo.ProductVersion
if ($compilerVersion.Major -eq 14 -and $compilerVersion.Minor -lt 30) {
  Write-Output 'Registration integration test requires VS2022; hosted CI runs it.'
} else {
  cl /nologo /EHsc /std:c++20 /W3 /DNOMINMAX /DWIN32_LEAN_AND_MEAN /DIMGUI_DISABLE_INCLUDE_IMCONFIG_H '/DImTextureID=unsigned long long' /Isrc /Ideps/reshade/include /Ideps/reshade/deps/imgui tests/TrackingRegistrationTests.cpp src/StateTracking.cpp src/DescriptorTracking.cpp /Fotests/out/ /Fdtests/out/registration.pdb /Fetests/out/tracking-registration.exe /link user32.lib
  if ($LASTEXITCODE -ne 0) { throw 'Tracking registration integration test failed to compile.' }
  & ./tests/out/tracking-registration.exe
  if ($LASTEXITCODE -ne 0) { throw 'Tracking registration integration test failed.' }
  Write-Output 'Production tracking registration integration test passed.'
}
