# HDR handling and validation

REST selects where ReShade effects run. It does not select a game's HDR presentation mode, install HDR upgrades, or infer a render target's colour encoding from its pixel format. A floating-point texture can contain linear scene colour, scRGB or other data; a 10-bit UNORM texture can contain HDR10/PQ or ordinary normalized colour. Storage format alone is not a colour-space declaration.

## Safe defaults

- Leave **Apply tone map clamping** off for HDR10/PQ and HLG content. It is an optional, lossy ACES compression/inverse pair for linear HDR effects, not a standards-based HDR colour conversion. It can clip highlights and change negative wide-gamut values. Both techniques must be loaded before the pair runs.
- Use effects designed for the selected target's encoding and precision. An effect with an 8-bit intermediate, saturation, or SDR-only colour math can clip HDR even when REST preserves the input format correctly.
- Hunting previews are diagnostic texture views. They are not a colour-managed HDR reference display: raw PQ values and linear values above one need suitable display mapping. Vulkan preview capture is a raw copy at a safe render-pass boundary; flip, alpha-clearing and tone-map preview controls are not applied there.
- Preserve target alpha is available through REST's copy shader on D3D9/10/11/12. On Vulkan/OpenGL the copy shader is unavailable, so the option is ignored and effects render directly rather than disappearing into a scratch buffer. The saved preference is preserved for supported backends. Auto Scene Colour also ignores this option as documented in the UI.

## What the format fixes cover

- Preview and copied-buffer compatibility distinguishes FP16 FLOAT from 16-bit UNORM/UINT, and 10-bit UNORM from UINT. Sharing a typeless storage family does not make those interpretations interchangeable.
- Cached game-resource views are keyed by resource handle **and** selected typed format. Changing a texture's bound interpretation no longer reuses the first requested view indefinitely.
- Formats such as FP16 and 10-bit UNORM have no separate sRGB variant: their valid view is reused and destroyed once. Vulkan uses the selected concrete view rather than assuming a mutable linear/sRGB pair. D3D/OpenGL retain supported linear/sRGB alternatives, with fallback to a valid view when one alternative cannot be created.
- The DX12 copy pipeline remains keyed by actual destination format. D3D10/11 do not bake render-target format into the same pipeline object; their copy shader writes sampled RGB without saturation. The copy shader changes alpha only where explicitly requested.

## Backend coverage

| Backend | HDR-related source coverage | Remaining validation |
|---|---|---|
| D3D10 | Typed views, preview/copy-buffer recreation, float copy shader and alpha preservation | Fresh in-game FP16/10-bit tests; HDR presentation availability is game/platform-dependent |
| D3D11 | Same paths as D3D10; no REST requirement that HDR be limited to DX12 | Fresh HDR tests; the reported BG3 test did not specify HDR mode |
| D3D12 | Typed destination pipelines, copy/resource transitions, float buffers and alpha preservation | Warframe HDR symptoms, HDR10/scRGB comparisons, validation-layer capture |
| Vulkan | Concrete image views, safe deferred Auto Scene Colour/preview capture and typed staging buffers | HDR swapchain/effect colour-space handling, HDR title testing and manual copied-binding paths |
| D3D9/OpenGL | Shared view/cache changes reviewed; copy shader is D3D-only | No claim of modern HDR presentation or Auto Scene Colour support; title/backend-specific testing required |

Modern ReShade's pinned runtime creates custom-target effect permutations with an **unknown colour space**. REST cannot assume a swapchain's HDR encoding also describes an earlier scene target. Effects must handle this explicitly rather than relying on automatic HDR detection. Legacy/API 7 does not gain API 20 colour-space or descriptor capabilities through these fixes; its documented Vulkan buffer-descriptor fallback remains in force.

Manual copied bindings on Vulkan need separate safe-boundary/resource-state validation; the generic binding path is not certified by the Auto Scene Colour and hunting tests. Do not extend their test results to every manual binding configuration.

## Recorded results and open questions

On 19 October 2026 the reporter of [issue #15](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/issues/15#issuecomment-6259749953) confirmed that shader-hunting crashes were fixed in both official and unofficial ReShade, then mentioned HDR problems without describing their appearance or settings. The issue was subsequently closed. No separate HDR issue was available when this audit was performed. The fixes here address concrete format/cache problems; they are not a reproduction or confirmed fix of those unspecified HDR symptoms.

The maintainer previously reported successful BG3 D3D11/Vulkan functionality and shader hunting. Exact HDR mode, ReShade versions, architectures and per-branch coverage were not recorded. No fresh HDR game session was available for this audit.

For HDR feedback, record: game and renderer; REST and ReShade versions; native HDR/Windows Auto HDR/HDR add-on and its settings; selected target format and injection point; HDR10/PQ versus scRGB if known; enabled effects; tone-map and alpha options; expected/actual brightness, colours and alpha; comparison with REST effects disabled. Attach ReShade logs and screenshots/captures suitable for the selected HDR mode.

Test matrix: FP16 scRGB (negative channels and values above one), 10-bit HDR10/PQ, SDR reference, alpha on/off where supported, hunting while switching FP16/UNORM interpretations, resize/upscaler changes, same-resolution and native-staging paths. Run modern and legacy separately and record actual coverage.

## References

- [Microsoft: DirectX Advanced Color and HDR](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range): scRGB uses values outside [0,1]; storage and presentation colour space are separate.
- [Khronos: swapchain colour spaces](https://docs.vulkan.org/refpages/latest/refpages/source/VkColorSpaceKHR.html): HDR10/ST2084 and extended-linear colour spaces are distinct.
- Pinned ReShade sources: `source/runtime_api.cpp` (custom-target rendering), `source/runtime.cpp` (effect permutations), and the D3D10/D3D11 device implementations (render-target format pipeline subobjects are ignored).
