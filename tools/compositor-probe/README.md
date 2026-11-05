# Local compositor boundary experiment

Developer research tools for the exact saved Warframe HDR captures in issue #22. They are **not loaded by REST**, not included in release packages, and do not implement a live compositor split or arbitrary ReShade technique execution. No game shaders, captures or generated image data are included.

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

## Meaning of the result

The experiment locates a post-blend/post-grading, pre-UI hook in two exact compositor shaders. The identity hook is bit-exact in the saved frames; the late tint survives when the secondary colour fully replaces the main scene. See [the engineering results and implementation requirements](../../docs/compositor-boundary-research.md).

It does not validate a two-pass texture round trip, a live add-on's state restoration, GPU lifetimes, resize/reload behaviour, performance, arbitrary effects or live DLSS. The saved DLSS capture did not contain valid DLSS execution. A production implementation must preserve those distinctions and retain the original composition code rather than guessing a generic UI formula.
