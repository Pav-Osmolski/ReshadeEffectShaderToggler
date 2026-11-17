# HDR, colour and alpha

REST decides **where** ReShade effects run. It does not choose the game's HDR mode or convert every effect into an HDR-compatible effect. Correct output depends on both the scene target's encoding and the effect's colour math.

## Start with these settings

- Use effects designed for the colour encoding at the selected target.
- Leave **Apply tone map clamping** off for HDR10/PQ and HLG. This control uses a lossy ACES compression/inverse pair; it is not standards-based HDR conversion.
- Compare a known scene with the REST group off/on. Check brightness, colour, highlight detail and UI separately.
- Treat hunting previews as diagnostic texture views, not a colour-managed HDR reference.

An FP16 texture may contain scene-linear, scRGB or other data. A 10-bit UNORM texture may contain PQ or ordinary normalised colour. The storage format alone does not identify the encoding. A selected early scene target can differ from the swapchain's colour space.

## Tone-map clamping

The optional ACES pair may be useful for specific linear-HDR effects, but it can clip highlights and alter negative wide-gamut values. Both helper techniques must be loaded for the pair to run. Do not use it as a general fix for washed-out or over-bright HDR.

Effects with 8-bit intermediates, saturation or SDR-only assumptions can still clip or distort colour when REST preserves the target format correctly. REST's target selection does not repair those effects.

## Alpha preservation

**Preserve target alpha** uses REST's shader-copy path on D3D9/10/11/12. It is ignored while Auto Scene Colour is active, and that copy shader is unavailable on Vulkan/OpenGL. The preference remains saved for a supported configuration.

Vulkan previews do not apply flip, alpha clearing or tone-map processing. Other previews and bindings have their own topology and format requirements.

## Backend status

| Backend | Relevant handling | Coverage limits |
|---|---|---|
| D3D10 / D3D11 | Typed views, float copy path, compatible preview/buffer recreation | Current BG3 D3D11 reports do not establish fresh HDR-specific coverage |
| D3D12 | Typed copy pipelines, resource transitions and pass guards | Warframe HDR work remains separate; no universal HDR10/scRGB result |
| Vulkan | Concrete views, safe deferred captures/injection and typed staging | Enshrouded SDR/HDR placement passed in the documented setup; this is not measured colour accuracy |
| D3D9 / OpenGL | Existing manual view/cache paths | No modern HDR presentation or Auto Scene Colour claim |

Modern's custom-target effect permutations use an unknown colour-space declaration in the pinned ReShade implementation. Legacy does not gain newer colour-space/descriptor capabilities through REST. Manual copied bindings require separate validation; Auto and hunting results do not certify every binding configuration.

## What has been tested

Enshrouded Modern and Legacy SDR/HDR passes were confirmed in November 2026. Detailed Modern tests included DLSS Quality, render-scale changes, sharp UI with scene blur, toggles and clean exit in the documented launch setup. BG3 passed D3D11 and Vulkan on both release lines; those reports did not separately establish HDR settings. See [validation records](validation.md).

The closed shader-hunting issue #15 and the later [Warframe HDR investigation #22](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/issues/22) are distinct. These releases do not claim to resolve the Warframe HDR work.

## Report an HDR problem

Include the game/build, API, REST/ReShade versions, other add-ons, HDR source (native, Auto HDR or an add-on), target format and scene boundary, enabled effects, tone-map/alpha settings, and expected versus actual appearance. State PQ/scRGB/HLG only if known. Attach fresh logs and a suitable comparison capture; a normal screenshot may not preserve HDR appearance.

Useful comparisons include SDR reference, FP16 values above one/negative channels, 10-bit PQ, same-resolution versus staging, resizing/upscaling, and supported alpha on/off. Mark unavailable tests as unrun.

## References

- [Microsoft: DirectX Advanced Color and HDR](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range)
- [Khronos: Vulkan swapchain colour spaces](https://docs.vulkan.org/refpages/latest/refpages/source/VkColorSpaceKHR.html)
- [Technical reference](reference.md) and [Vulkan allocation safety](vulkan-transfer-safety.md)
