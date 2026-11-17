# Compatibility and limits

## ReShade and release lines

REST requires ReShade's **full add-on support** build on Windows.

| Line | ReShade baseline | Branch | Architectures |
|---|---|---|---|
| Modern | ReShade 6.8+ / API 20 | `main` | x86 and x64 |
| Legacy / ReShade 5.x | ReShade 5.8–5.9.2 / API 7 | `legacy/reshade-5.x` | x86 and x64 |

ReShade 5.9.2 is recommended for Legacy. The final version component (`.680` or `.580`) identifies REST's SDK baseline. It is not a game version or a requirement to use precisely one ReShade patch.

## Graphics backends

| Backend | Hunting / manual workflows | Auto Scene Colour | Key limits |
|---|---|---|---|
| D3D9 | Supported, including standalone surface render targets | No | A surface is not necessarily sampleable or previewable; no modern HDR presentation claim |
| D3D10 / D3D11 | Supported | Yes | Needs an appropriate colour target and shader boundary |
| D3D12 | Supported with pass guards | Yes, subject to pass guards | Preview/effect work is skipped while a native D3D12 render pass is active |
| Vulkan | Supported with safe-boundary restrictions | Yes | Requires a compatible transfer-capable image and a safe later boundary; cannot split an active game pass |
| OpenGL | Existing manual paths | No | No REST shader-copy alpha preservation; no fresh game-test coverage claimed |

Source support is broader than the set of configurations tested in games. [Validation records](validation.md) distinguish automated checks, direct observations and maintainer reports.

## What REST does and does not control

REST can choose an effect boundary, select techniques, suppress matching graphics draws and expose bindings for advanced configurations. It cannot guarantee a usable injection boundary in every game, repair arbitrary effects, infer HDR colour encoding from a texture format or make SDR shader code HDR-aware.

Automatic matching accepts compatible aspect ratios and output/input scale factors from **0.5 to 4.0** in each dimension. Manual matching modes keep their own policy. The scale allowance does not make a depth/history/unrelated target suitable for scene effects.

Previews require a single-sample, single-layer, single-mip 2D target. Vulkan colour previews additionally need a safe copy boundary. An available preview does not guarantee the same candidate supports effect injection.

## Legacy differences

The older SDK does not expose all Modern colour-space, descriptor or render-pass information. Legacy keeps conservative Vulkan descriptor fallbacks instead of assuming offsets can be restored. The FFXIV constant-copy hook is x64-only. Successful compilation of both architectures does not establish identical coverage for every game-specific hook.

## HDR and upscalers

Auto Scene Colour supports internal/output size differences on its supported backends. Native staging has a GPU cost. HDR correctness also depends on the scene target and selected effect. Read [Auto Scene Colour](automatic-scene-colour.md) and [HDR](HDR.md).

Optional Vulkan scene transfers default off. Enable them only for a setup that needs them, then save and restart. They retain eligibility checks and do not establish universal Vulkan allocation compatibility.
