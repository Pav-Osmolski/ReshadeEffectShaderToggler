# Local compositor boundary experiment

Standalone developer research tools for the exact saved Warframe HDR captures in issue #22. The prototype runs the split in its own D3D12 process. Keep captures, game shaders and generated data in the external output directory.

## Dependencies

- Windows x64, a D3D12-capable GPU and Visual C++/Windows SDK tools.
- RenderDoc **1.46**, matching public replay headers and `renderdoc.lib`. Put its native `renderdoc.dll` directory on the process PATH. This uses headless native replay; do not launch `qrenderdoc`.
- Microsoft DXC **v1.9.2609** (`dxc_2026_09_29.zip` from the [official release](https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.9.2609)). Keep the package's `dxil.dll` beside `dxcompiler.dll`: assembly alone can produce an unsigned replacement that is not used successfully. The tool assembles **and validates** every replacement.
- Python 3.9+ and NumPy for pixel comparisons.

Build with `build.ps1 -RenderDocHeaders <headers> -RenderDocLibrary <renderdoc.lib> -OutputDirectory <external-output>`. Use an external working directory for all following output; never write generated game data into the repository.

## Reproduce

1. Create `taa`, `no-taa` and `fsr` subdirectories under the external output root. Run `replay-variants.exe <capture.rdc> <event> <mode-directory>` for each saved capture. Events are **6229** (TAA/no sharpening), **6021** (no TAA) and **5917** (FSR/TAAU). This exports the selected pixel shader, original output, UI texture and constant buffer into that directory. Capture input files are opened read-only.
2. Run `python prepare-capture-variants.py --root <external-output> --dxcompiler <DXC/bin/x64/dxcompiler.dll> --dxil-tool <external-output/dxil-tool.exe>`. Full SHA-256 identities are checked before transformation. Unknown shaders are refused. The script's structural checks are research assertions; run Python normally without `-O`.
3. Repeat each replay command, appending the full variant path stems (without `.dxil`): `identity`, `identity-hook`, `scene-only`, `post-grade-tint`, `main-input-tint`, `secondary-only`, `secondary-only-main-tint` and `secondary-only-post-tint`. For example, a stem is `<external-output/taa/identity>`. Replay restores the original resource between variants.
4. Run `python verify-replays.py --root <external-output>`. A result is accepted only if the unmodified assembly round trip and identity hook exactly match the original pixels, the late tint changes output, the main tint disappears within one 10-bit code value at forced full secondary weight, and opaque UI interiors remain unchanged.

The full-secondary-weight case is a controlled shader experiment, **not** a new recording of in-game motion. A five-by-five opaque-alpha interior mask avoids drawing conclusions from bilinear sampling at UI edges. Transparent UI backgrounds are expected to reveal the changed scene. Raw PNG exports are not colour-managed HDR reference images.

## Run the two-pass D3D12 test

1. Append `--split-inputs` to each capture export command in step 1 above. This additionally exports post-vertex geometry, original sampled textures and mip levels, samplers, viewport and scissor.
2. Generate the shader variants and replay the inline references as above. The generator also creates the two-pass UI shader and a scene-only shader with full secondary weight.
3. Use the pinned `dxc.exe` to build the two test entry points in `split-test.hlsl`.

   ```powershell
   & $dxc -E ReplayVertex -T vs_6_0 split-test.hlsl -Fo "$output/replay-vertex.dxil"
   & $dxc -E TintScene -T ps_6_0 split-test.hlsl -Fo "$output/tint-scene.dxil"
   ```

4. Run `split-test.exe <mode-directory> <replay-vertex.dxil> <tint-scene.dxil>` for each mode. It compares the original draw and the split across three intermediate formats. Enable the Windows Graphics Tools feature to collect D3D12 debug messages.
5. Run `python verify-split.py --root <external-output>`. It writes `split-verification.json` and fails if the reconstructed original, full-precision identity, full-precision tint or forced-secondary comparisons differ from their reference.

The original shader and full-precision split matched the saved frames bit-for-bit. The standalone test uses a dedicated tint pass. Arbitrary ReShade effects, live state restoration, resource lifetime, performance and native DLSS require integration testing. See the [engineering results and implementation requirements](../../docs/compositor-boundary-research.md).

To exercise the add-on's runtime shader generator, run `runtime-split-variants.exe <mode-directory> <DXC/bin/x64>` for each mode. Compile `TintSceneOverwriteAlpha` from `split-test.hlsl` as `ps_6_0`, pass its output as the fourth argument to `split-test.exe`, and run `verify-split.py --runtime-pair --root <external-output>`. This compares both runtime variants and verifies that alpha overwritten by the test effect is recovered from the separate alpha target.
