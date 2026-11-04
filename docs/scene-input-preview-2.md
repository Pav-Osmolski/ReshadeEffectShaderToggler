# REST Enhanced — Scene input preview 2

This Modern pre-release corrects a validation bug found through the TAA and DLSS reports in [Warframe HDR issue #22](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/issues/22#issuecomment-6627780722).

## What was wrong

The reported **Pixel / Slot 2 / Binding 0** settings identify the correct full-resolution R11G11B10 FLOAT scene texture. Preview 1 could still reject it because ReShade's D3D12 conversion leaves the unused array-layer fields at zero for an ordinary non-array 2D SRV. REST mistakenly required a layer count of one, so the preview never resolved a source and the tint did not run.

Preview 2 checks the resource's actual topology and the view type, and ignores array-only fields for a plain 2D view. Array textures/views, MSAA, multiple mips and partial-mip views remain unsupported. A regression test now compiles the pinned ReShade SDK conversion implementation, reproduces its zero-layer representation, and checks both accepted and rejected cases. Preview/status reports also identify the exact failed eligibility check and whether a retained CPU descriptor is available.

## Repeat the existing test

1. Back up and replace the add-on with **REST-Scene-Input-Preview-2-Modern.zip**; use `.addon64` for Warframe and restart normally without RenderDoc. Requires ReShade 6.8+ with add-on support. Confirm **Scene input preview 2 (pre-release)** appears in REST. File version remains 1.7.3.680; `BUILD-INFO.txt` identifies the exact source commit.
2. Keep descriptor tracking enabled and the existing **Shader Resource View / Pixel / Slot 2 / Binding 0** configuration. Use the already-found compositor `0x7dcef337` for the reported DLSS setup, or `0x418d71b1` for TAA. Keep Auto Scene Colour, tone-map clamping, flip and Hide marked shaders off; finish hunting and keep the group active.
3. Select **REST_SceneBoundaryCheck** for that group's Effects and enable its ReShade technique checkbox, with other techniques off initially. In native HDR, toggle the tint with the normal HUD visible. Test the same TAA and DLSS settings as the previous report.
4. If the scene tints while HUD colours stay unchanged, disable the tint, select **SOLARIS in the group's Effects**, and enable its ReShade checkbox. Test SOLARIS alone with the same scene-input settings.

No new frame capture is requested. A successful result only needs the mode and whether the scene/HUD behaved correctly. If it is still skipped, paste the **exact status** and **Copy preview diagnostics** from this build; the new view fields and rejection reason should identify the next gate directly. Include `ReShade.log` for a crash or rendering error.

The rejection bug is reproduced and fixed locally using the real SDK conversion; successful live HDR/DLSS rendering still requires confirmation. See the [full setup](scene-input-preview-1.md) and [implementation limits](experimental-backend-support.md) for background.
