param([switch]$RequireNative)
$ErrorActionPreference = 'Stop'
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
Import-Module "$vsPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
foreach ($arch in @('x64', 'x86')) {
    Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments "-arch=$arch -host_arch=x64"
    $out = "tests/out/maintenance-$arch"
    New-Item -ItemType Directory -Force $out | Out-Null
    cl /nologo /EHsc /std:c++20 /W4 /WX /DNOMINMAX /DWIN32_LEAN_AND_MEAN /Isrc /Itests /Ideps/reshade/include tests/D3D9ResetTests.cpp src/GlobalResourceView.cpp /Fo"$out/" /Fe"$out/d3d9-reset.exe"
    if ($LASTEXITCODE -ne 0) { throw 'D3D9 reset tests failed to compile.' }
    & "$out/d3d9-reset.exe"
    if ($LASTEXITCODE -eq 77 -and !$RequireNative) { Write-Output "Native $arch D3D9 test skipped; unsupported host." }
    elseif ($LASTEXITCODE -ne 0) { throw 'Native D3D9 reset regression or required coverage unavailable.' }
    $compilerVersion = [version](Get-Item (Get-Command cl.exe).Source).VersionInfo.ProductVersion
    if ($compilerVersion.Major -eq 14 -and $compilerVersion.Minor -lt 30) {
        Write-Output 'Group lifecycle and hunting log tests require VS2022; hosted CI runs it without compatibility substitutions.'
    } else {
        cl /nologo /EHsc /std:c++20 /W3 /DWIN32 /DNOMINMAX /DWIN32_LEAN_AND_MEAN /DIMGUI_DISABLE_INCLUDE_IMCONFIG_H '/DImTextureID=unsigned long long' /Isrc /Ideps/reshade/include /Ideps/reshade/deps/imgui /Ideps/robin-map/include tests/HuntingLogTests.cpp src/ShaderManager.cpp /Fo"$out/" /Fe"$out/hunting-log.exe" /link user32.lib
        if ($LASTEXITCODE -ne 0) { throw 'Hunting log regression failed to compile.' }
        & "$out/hunting-log.exe"
        if ($LASTEXITCODE -ne 0) { throw 'Hunting log regression failed.' }
        cl /nologo /EHsc /std:c++20 /W3 /Gy /DWIN32 /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX /DWIN32_LEAN_AND_MEAN /DIMGUI_DISABLE_INCLUDE_IMCONFIG_H '/DImTextureID=unsigned long long' /Isrc /Itests /Ideps/reshade/include /Ideps/reshade/deps/imgui tests/MaintenanceLifecycleTests.cpp src/GlobalResourceView.cpp src/ToggleGroup.cpp src/CDataFile.cpp /Fo"$out/" /Fe"$out/lifecycle.exe" /link /OPT:REF
        if ($LASTEXITCODE -ne 0) { throw 'Production lifecycle tests failed to compile.' }
        & "$out/lifecycle.exe"
        if ($LASTEXITCODE -ne 0) { throw 'Production lifecycle regression.' }
        cl /nologo /EHsc /std:c++20 /W3 /DWIN32 /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX /DWIN32_LEAN_AND_MEAN /Isrc tests/VulkanSceneTransferSettingsTests.cpp src/CDataFile.cpp /Fo"$out/" /Fe"$out/vulkan-settings.exe"
        if ($LASTEXITCODE -ne 0) { throw 'Vulkan settings tests failed to compile.' }
        & "$out/vulkan-settings.exe"
        if ($LASTEXITCODE -ne 0) { throw 'Vulkan settings regression.' }
    }

}


