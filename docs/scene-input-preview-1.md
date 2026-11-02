# REST Enhanced — Scene input preview 1

This Modern pre-release tests the new D3D12 scene-input path for [Warframe HDR issue #22](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/issues/22). It replaces the sharpening-specific diagnostics-6 test with processing of the scene texture supplied to the final compositor. The original game texture is left unchanged; REST processes a private copy and restores bindings immediately after the selected draw.

Use **ReShade 6.8+ with add-on support**, and the add-on matching the game's architecture (Warframe uses `.addon64`). File version remains **1.7.3.680**; identify this build by **Scene input preview 1 (pre-release)** in REST and the full commit in `BUILD-INFO.txt`.

## Warframe: exact first test

1. Back up the existing add-on and REST INI. Install this build, enable **Track descriptors**, save, and restart Warframe normally. Do not launch through RenderDoc: the previous capture disabled DLSS. Start with **native HDR**, with RenoDX disabled for this first comparison.
2. Disable the old temporary test groups, including the `0x4ee80dd6` sharpening group. Create a temporary active group marking the compositor **pixel shader `0x7dcef337`**. This was present in the earlier live report and the no-TAA/FSR captures. The TAA captures used **`0x418d71b1`** instead; use that compositor if it is the one present in the current mode. Do not hunt for another producer shader.
3. In the group's Effects settings, turn **Auto Scene Colour off**, choose **Shader Resource View** as the render destination, and set **Pixel / Slot 2 / Binding 0**. These coordinates come from the reporter's earlier input report: Layout 2 / descriptor 0 was the full-resolution R11G11B10 FLOAT scene, while descriptor 8 was the UI. Confirm the selected scene format/size in the preview diagnostics if game settings or a game update change the layout. Keep tone-map clamping and flip off; leave Hide marked shaders off. Finish hunting so the marked draw is no longer suppressed.
4. Copy the included `Shaders/REST_SceneBoundaryCheck.fx` into a configured ReShade shader folder and reload effects. Select **REST_SceneBoundaryCheck** for this REST group and **enable its technique checkbox in ReShade too**. Keep global effects on and other techniques off initially.
5. With the menu closed and normal HUD visible, toggle the tint technique off/on in the usual **DLSS** configuration. The target result is a red-tinted scene with unchanged HUD colours. Then try FSR/TAAU or TAA as convenient; keep results labelled by mode. The captured modes support this test design, but do not prove the live DLSS result.

If the tint isolates the scene correctly, turn it off and try SOLARIS alone with the same group. Its appearance may differ from final-frame application because game grading/tone mapping still follows the selected scene input.

## What to report

A successful result only needs the upscaler mode and whether the scene changes while the HUD stays unchanged. No new RenderDoc capture or full diagnostic report is needed for success.

If the status says the input was skipped, or the result is wrong, paste **Copy diagnostics** and **Copy preview diagnostics** for this group, with the upscaler mode and one screenshot. Attach a fresh `ReShade.log` if there is a crash or rendering error. Do not keep cycling random slots: unsupported or incomplete inputs are intentionally skipped rather than redirected to the UI/final frame.

## Scope

This is a release candidate for **runtime testing**, not a confirmed resolution or universal HDR conversion. The new path is opt-in and Modern-only. Both x86 and x64 must pass builds, regression/integration tests, binary checks and packaging verification before publication. Actual Warframe/DLSS colour, UI isolation and stability still need live confirmation. Start with REST as the only additional add-on; other add-ons that also replace draws need separate compatibility checks.

See [supported formats, descriptor limits and implementation details](experimental-backend-support.md).
