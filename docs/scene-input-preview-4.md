# REST Enhanced Scene input preview 4

Preview 4 adds an opt-in x64 Warframe D3D12 profile for native HDR10. It renders the complete graded scene into a private 32-bit float texture, applies the group's ReShade techniques, then runs the game's original UI composition. The scene pass saves the game's alpha in a separate R32 float target. The UI pass uses that saved value even if a technique overwrites alpha.

## Install

1. Back up your existing add-on and settings. Replace the add-on with the `.addon64` from **REST-Scene-Input-Preview-4-Modern.zip**. This build requires ReShade 6.8+ with add-on support.
2. Download `dxc_2026_09_29.zip` from the official [Microsoft DXC v1.9.2609 release](https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.9.2609). Create a folder named `REST-DXC` next to the REST add-on and place the archive's **bin/x64/dxcompiler.dll** and **bin/x64/dxil.dll** inside it. REST checks both file hashes before loading them.
3. Open Warframe normally in D3D12 with native HDR enabled and RenoDX off for this test. In REST's settings, enable **Track descriptors** and **Warframe HDR compositor split (experimental)**. Save and restart the game. The settings should show both options active after restart.

## Test

1. Use the same group with **Shader Resource View / Pixel / Slot 2 / Binding 0**. Keep Auto Scene Colour, clamping, flip and Hide marked shaders off. Use `0x418d71b1` for the previously tested TAA setup or `0x7dcef337` for the previously tested DLSS setup. End shader hunting and keep the group and ReShade effects active.
2. Select **REST_SceneBoundaryCheck** in the group and enable the technique in ReShade. Start with that technique alone. Allow ReShade to finish compiling the new target permutation. The processing status should read **Warframe split: graded effects and original UI composition recorded**.
3. Check the tint with motion blur enabled and the HUD visible. Move the camera and check whether tint persists on fast-moving objects. Translucent HUD elements reveal the scene beneath them, while their original UI colour and blend remain part of the game's composition.
4. After the tint test, disable it and test SOLARIS alone. Enable SOLARIS in both the group and ReShade. Compare the strength and colour with the expected HDR result.

Please report the AA/upscaler mode and whether the scene, motion blur and HUD behaved as expected. If the split skips, paste **Copy preview diagnostics** and any `Warframe split` lines in `ReShade.log`. For a crash, include `ReShade.log` and the last action you took.

## Scope and validation

The profile requires an exact match to one of the two captured pixel shaders and the captured vertex shader. It checks the draw, root bindings, target and viewport before replacing the compositor. A mismatch, failed allocation or unready technique leaves the original game draw in place. The setting applies to manual D3D12 scene-input groups in Warframe. Disable it and restart to use Preview 3's scene-input path in the same build.

Native D3D12 replay matched the three saved capture references bit-for-bit using the generated shader pair. The tint deliberately overwrote alpha in that test, and the final alpha still matched. The debug queue reported zero errors. Live ReShade technique execution, performance, motion blur and native DLSS are the purpose of this pre-release.

Each graded colour and alpha pair uses about 117 MiB at 3824×1600. REST uses a bounded pool and skips a frame if no safe lease is available. ReShade's custom-target API gives effects an unknown colour-space tag, so technique behaviour on the graded values needs live testing.

The file version remains **1.7.3.680**. The overlay label **Scene input preview 4 (pre-release)** and the commit in `BUILD-INFO.txt` identify this build. See the [GPU research and results](compositor-boundary-research.md).
