# REST Enhanced HDR diagnostics 6 (pre-release)

This is a focused **scene-boundary placement test** for [issue #22](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/issues/22), based on the supplied GPU frame capture. It includes a simple tint-check technique and a specific shader to test. It does not introduce a new injection path or a general HDR conversion fix.

File versions remain the stable baseline: Modern **1.7.3.680**, Legacy **1.6.7.580**. Identify this test package by **HDR diagnostics 6 (pre-release)** above the preview and its commit in `BUILD-INFO.txt`.

## What the capture established

In the captured frame, pixel shader **`0x4ee80dd6`** writes a full-resolution **R11G11B10_FLOAT** scene texture before the separately rendered UI is combined with it. The scene texture and separate UI texture were replayed, exported and visually inspected. Scene production and final composition use different command lists, explaining why a same-list trace could miss the producer.

REST retains the selected draw's resource when later targets change. The capture also contains a subsequent pipeline bind on the producer's command list while the scene texture is still in render-target state. This supports testing the existing **AFTER DRAW** path. It does not establish live-game REST compatibility by itself.

Shader hashes can vary with game version/settings. The captured final-composition shader differs from the earlier `0x7dcef337` report; do not assume those bytecodes are identical. The new producer hash comes directly from the captured shader bytecode.

## Install

Back up the current REST add-on and `ReshadeEffectShaderToggler.ini`, then close the game. Use **Modern** for ReShade **6.8.0+** (Warframe's reported setup); **Legacy** is for ReShade **5.8.0+**. Install only one matching `.addon64` or `.addon32`; Warframe uses `.addon64`.

Copy the included **`Shaders/REST_SceneBoundaryCheck.fx`** into an existing ReShade effect search folder, commonly `reshade-shaders/Shaders`, and reload effects. Its `ReShade.fxh` include is provided by the normal ReShade shader package; ensure the include search path is configured. No extra third-party effect is needed.

## One focused test

1. Use **D3D12, native HDR**, the same scene as the capture, game menu closed and normal HUD visible. Keep ReShade's **global effects switch on**. Keep other techniques off.
2. Create a temporary REST group, hunt and mark **only pixel shader `0x4ee80dd6`**. Leave the earlier composition hashes out of this test group.
3. Select **AFTER DRAW**, **manual render target 0**, **Auto Scene Colour off**, tone-map clamping/flip off. Leave sampled-input rendering disabled. No producer tracing or additional copied diagnostic report is required.
4. The preview should show a full-resolution floating-point scene without the separately composited HUD/text panels. It may look dark because this is an unmanaged HDR preview; judge scene/UI content rather than brightness alone.
5. In the group's **Effects** selection, choose **REST_SceneBoundaryCheck**. **Enable its ReShade technique checkbox too**—REST-selected effects must remain enabled. This differs from the earlier metadata-only tests, where all techniques stayed unticked.
6. Finish hunting with **Done**, keep the temporary group active, then toggle the **REST_SceneBoundaryCheck technique checkbox** off/on in ReShade to compare. With the technique on, the 3D scene should have a strong red tint while the separate HUD retains its normal colours. If the HUD is tinted too, report that result; it may mean the technique fell through to normal final-frame rendering.

The only feedback needed initially is **whether the hash is available** and **whether the scene changes while the separate HUD stays unchanged**. No repeat metadata matrix, SDR comparison or new GPU capture is needed for a successful result. If the hash is missing or the test fails, describe that outcome rather than changing many settings.

## How the check works

The probe multiplies RGB by `(1.0, 0.25, 0.25)` and preserves alpha. It does not clamp HDR values, apply tone mapping or convert a display transfer function. Warframe's subsequent grading/tone mapping may affect the exact colour. The probe is a temporary placement check, not an HDR processing fix or a guarantee that SDR-oriented techniques work at this boundary.

Runtime tracking/injection is retained from diagnostics 5. Track descriptors still honours the saved setting and requires save/restart when changed. Reports retain **Sampled texture inputs at selected draw**, actual registration mode and bounded metadata. The optional **Trace earlier colour-target draws (diagnostic)** checkbox remains session-only, disabled by default and limited to same-command-list recording hints; it is not needed for this test.

## Restore

Disable the tint technique before removing it from the REST group, then remove the temporary group/shader and restore the INI backup if desired. To return fully to stable, close the game and restore the backed-up add-on. Stable releases and their source branches are unchanged.

## Validation limits

Native builds, CPU regressions and package verification do not run Warframe or certify ReShade effect execution. The capture establishes actual scene/UI textures and a candidate boundary; the included probe still needs the focused live test above. This release does not claim universal HDR support, performance certification, hash stability or RenoDX compatibility.
