$ErrorActionPreference = 'Stop'
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
Import-Module "$vsPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
New-Item -ItemType Directory -Force tests/out/sdk | Out-Null
# Compile the pinned SDK implementation itself. Hand-built API view fixtures
# missed its zeroed, unused array fields on ordinary non-array D3D12 SRVs.
cl /nologo /EHsc /std:c++20 /W3 /DNOMINMAX /DWIN32_LEAN_AND_MEAN /Isrc /Ideps/reshade/include /Ideps/reshade/deps/d3d12/include/directx /Ideps/reshade/source/d3d12 tests/SceneInputSdkTests.cpp deps/reshade/source/d3d12/d3d12_impl_type_convert.cpp /Fotests/out/sdk/ /Fetests/out/scene-input-sdk.exe
if ($LASTEXITCODE -ne 0) { throw 'Scene-input SDK integration test failed to compile.' }
& ./tests/out/scene-input-sdk.exe
if ($LASTEXITCODE -ne 0) { throw 'Scene-input SDK integration regression.' }

cl /nologo /EHsc /std:c++20 /W4 /WX /Isrc tests/SceneInputShaderTests.cpp /Fotests/out/sdk/ /Fetests/out/scene-input-shaders.exe
if ($LASTEXITCODE -ne 0) { throw 'Scene-input shader tests failed to compile.' }
& ./tests/out/scene-input-shaders.exe
if ($LASTEXITCODE -ne 0) { throw 'Scene-input shader eligibility regression.' }

cl /nologo /EHsc /std:c++20 /W4 /WX /Isrc /Ideps/reshade/include tests/SceneInputProcessingTests.cpp /Fotests/out/sdk/ /Fetests/out/scene-input-processing.exe
if ($LASTEXITCODE -ne 0) { throw 'Scene-input processing contract tests failed to compile.' }
& ./tests/out/scene-input-processing.exe
if ($LASTEXITCODE -ne 0) { throw 'Scene-input processing contract regression.' }

cl /nologo /EHsc /std:c++20 /W4 /WX /DNOMINMAX /Isrc tests/SceneInputReuseTests.cpp /Fotests/out/sdk/ /Fetests/out/scene-input-reuse.exe
if ($LASTEXITCODE -ne 0) { throw 'Scene-input GPU reuse tests failed to compile.' }
& ./tests/out/scene-input-reuse.exe
if ($LASTEXITCODE -ne 0) { throw 'Scene-input GPU reuse regression.' }
