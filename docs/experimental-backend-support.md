# Experimental scene input and backend improvements

[Documentation index](README.md) · [HDR guide](HDR.md) · [Troubleshooting](troubleshooting.md)

This work lives on `feature/scene-input-and-backend-support` (Modern) and `feature/legacy-scene-input-and-backend-support` (Legacy). The first D3D12 scene-input implementation is Modern-only; its API 7 adaptation and runtime coverage need a separate review. Legacy receives the D3D9 and general lifecycle fixes. This work is not part of the stable releases. A successful build establishes compilation and automated checks, not compatibility with every game or HDR/upscaler configuration.

## D3D12 scene input

Some games combine scene colour and UI in one final draw. Injecting into that draw's output also affects UI; selecting the scene **input** can avoid that. Warframe's captures in [issue #22](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/issues/22) show different producers for no TAA, TAA and FSR/TAAU. The optional temporal sharpening shader is not a universal boundary. The supplied DLSS capture fell back to TAA after NVAPI initialization failed, so it does not establish DLSS coverage.

REST's experimental path samples the selected input into a private texture, runs the group's effects on that copy, then supplies the processed texture to the selected compositor draw. REST issues the selected direct draw once with the private input, then restores bindings immediately before returning control to the game. It leaves the original scene texture unchanged, including history or output owned by an upscaler. This also permits a graphics compositor to consume an input previously produced by compute on another command list. It does not inject inside the upscaler itself.

### Warframe Preview 3 result and remaining limits

On 5 November 2026, the [reporter confirmed](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/issues/22#issuecomment-6660706352) that Preview 3 tints the scene behind the UI using `0x418d71b1` with TAA and `0x7dcef337` with DLSS. This is live placement evidence for those reported configurations. It does not establish general effect colour accuracy or complete motion-blur coverage.

The reporter also found weaker tint and loss of tint on moving objects with motion blur enabled. The saved TAA compositor reads the selected full-resolution scene from `t0`, then blends it with another half-resolution colour input at `t2` using that input's alpha. In simplified form, this stage is `lerp(processed_main_scene, secondary_colour, secondary_alpha)`. Only the main input is redirected by the current implementation. As the secondary weight approaches one, the main input's effect contribution approaches zero. This is consistent with the reported motion-blur behaviour; the existing capture does not measure the live moving pixels or independently label every secondary contribution.

The same shader subsequently applies grading through a 3D lookup texture and composites UI. Effects applied to the earlier scene input therefore need not match their final-frame appearance. Changing R11G11B10 FLOAT storage to RGB10A2 UNORM, or enabling the optional tone-map clamping pair, does not reproduce those game-specific operations. The companion `t2` texture also has multiple mips and must not be selected by simply relaxing the current single-mip checks.

Remaining work concerns complete scene-contribution coverage and effect colour/placement compatibility. Processing a second input alone would not provide a general post-grading, pre-UI boundary for arbitrary techniques. No further capture was requested for this analysis; the supplied captures already expose the blend and grading stages.

The subsequent [compositor-boundary investigation](compositor-boundary-research.md) GPU-tested a later hook inside the captured shaders. Its identity hook is bit-exact and its tint survives full secondary-colour replacement. This remains a research experiment, not a live add-on implementation or another pre-release.

### Setup

1. Use the Modern experimental build matching your game architecture. Back up your REST configuration first.
2. Enable descriptor tracking, save your configuration, and restart the game. Tracking must be active when the game creates and copies its descriptors.
3. Hunt the **pixel shader** that combines scene colour and UI. Use the bound-input diagnostics to identify the scene texture; shader hashes and table positions can change with game settings or updates.
4. In the group's effect settings, turn **Automatic Scene Colour** off and choose **Shader Resource View** as the render destination. D3D12 labels this experimental.
5. Select the exact **Slot** (root-table parameter) and **Binding** (linear descriptor offset). These are not necessarily the HLSL `t` register. Start with a simple, enabled test technique selected for this group. Leave tone-map clamping off unless the effect specifically needs it.
6. Check the group's status and preview, then verify that the scene changes while UI remains unchanged. Compare TAA/sharpening, no TAA, FSR/TAAU and live DLSS separately.

### Deliberate limits

- Direct graphics draws on direct command lists only; compute, indirect draws, bundles and active D3D12 render passes are skipped. Scene input remains disabled for a recording after a secondary command list executes.
- Classic DXBC and validated Shader Model 6.0–6.5 DXIL graphics pipelines. DXIL requires a shader-feature record without resource/sampler heap-indexing requirements; SM 6.6+ and malformed/unknown containers are skipped. See the Preview 2 guide for the captured Warframe shader checks.
- The selected SRV must be visible only to the pixel shader. Shared-stage bindings are skipped.
- One scene-input group per draw, applied with the existing once-per-frame technique scheduling.
- Single-sample, single-mip, single-layer 2D textures with a complete view. Supported typed formats are R11G11B10 FLOAT, RGBA16 FLOAT, RGBA8 UNORM, BGRA8 UNORM and RGB10A2 UNORM. Integer, depth, sRGB, array, partial and multisampled views are skipped.
- Every resource descriptor table must be finite, fully tracked and at most 255 entries. Unbounded, mixed sampler/resource or incomplete tables are skipped. Sampler tables keep the game's original heap. Observed graphics and compute tables must share a restorable original heap pair; incompatible historical bindings cause a skip. Static samplers do not need a bound table.
- Texture/buffer views require retained CPU descriptor snapshots. Descriptors created directly in shader-visible heaps, unsupported descriptors and exhausted snapshot storage cause a skip. REST does not read descriptors back from shader-visible heaps.
- Private texture storage is capped at 256 MiB and 64 recordings. Resources are reused only after the previous recording has been reset or destroyed and completion fences on all known queues have passed. Allocation or fence failures skip work or retain resources rather than reusing them prematurely.
- Failed scene-input attempts suppress the selected techniques' final-frame fallback for that frame. They must not unexpectedly run over the UI.

The copy preserves sampled RGBA and floating-point range. It does not infer colour space or convert HDR10/PQ, HLG or scRGB. Effects may deliberately change alpha or colour; later game grading, bloom and tone mapping can change the apparent strength of the result. The separate **Preserve target alpha** scratch-buffer option is not applied to this private-copy path.

The preview samples the selected input without transitioning or modifying it. It is not an HDR display calibration view. If a layout is skipped, report the status and bound-input diagnostics rather than repeatedly changing descriptor indices at random.

## D3D9 and resource lifecycle

The fork review adds support for D3D9 standalone surface render targets, releases REST-owned group/view/preview/shader resources before the last runtime on that device is destroyed for Reset, and makes view disposal idempotent. Views are released before their backing textures. Cached views are scoped by device, resource handle and typed format.

This does not add Automatic Scene Colour or modern HDR presentation to D3D9. Standalone surfaces need not support shader sampling or texture-binding extraction. Validate effect placement, shader hunting, copied bindings and alpha preservation in an actual D3D9 game, then repeat after resizing, changing resolution, Alt-Tab and reset/fullscreen transitions. Test both x86 and x64 where the title permits.

Removing a group now retains its CPU object until add-on teardown so outstanding command-list queues cannot dereference a deleted group. Its GPU resources are released through the existing synchronized removal path; retired groups are excluded from matching, rendering, bindings, constants and configuration saves.

## Fork audit and attribution

Reviewed [BadassBaboon's fork](https://github.com/BadassBaboon/ReshadeEffectShaderToggler) at `372c4a83725df11f62adfc3ce41822774fb40855`, against common ancestor `6d63c9d1dfd3ab47d7ab1512ecb8111d2aa461a6`. Changes are adapted to REST Enhanced's current state tracking, format handling and Modern/Legacy SDKs; the fork is not merged wholesale.

| Change | Decision |
|---|---|
| D3D9 surface/reset and destruction ordering, [f403b477](https://github.com/BadassBaboon/ReshadeEffectShaderToggler/commit/f403b4778969808ed369a057d060c1e858d5e96a) | Adopted and narrowed to D3D9 where appropriate. The fork credits CrazyDaifu for the D3D9 contribution. |
| Removed-group lifetime, [584e36b](https://github.com/BadassBaboon/ReshadeEffectShaderToggler/commit/584e36b) | Adapted with retained map nodes and explicit retired-group guards. |
| Shift-key migration, [4cd08e0](https://github.com/BadassBaboon/ReshadeEffectShaderToggler/commit/4cd08e0) | Not applicable to Enhanced's unassigned defaults; no unrelated key-binding migration imported. |
| Broad 8-bit D3D12/Vulkan restrictions and disabled D3D12 previews | Not imported: they would remove newer typed HDR and preview support. |
| Controller/XInput UI and geometry-filter features | Deferred as separate product features requiring their own configuration, interaction and compatibility review. |
| Enhanced features already copied into the fork | Kept the maintained Enhanced implementations instead of importing older duplicates. |

Existing copyright and license notices remain in their original files. The fork's [MIT license notice](fork-license.txt) is retained alongside the repository's [license](../LICENSE).

## Before a stable release

Build Modern and Legacy for x86/x64 with the pinned SDKs and run the regression checks. Start runtime testing with REST as the only additional add-on: draw interception may interact with other add-ons that also cancel or replace draws. A public test build should be a clearly marked pre-release with this guide. Validate the compositor path in-game across the Warframe modes above, including live DLSS outside RenderDoc; check UI isolation, effect intensity, alpha, HDR range, long sessions and repeated resolution changes. Capture debug-layer errors/device removal if any occur. D3D9 reset tests and D3D10/D3D11/Vulkan regression tests remain separate requirements. Do not describe untested modes as confirmed support.
