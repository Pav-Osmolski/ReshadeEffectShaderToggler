# REST Enhanced — Scene input preview 3

This Modern pre-release addresses the processing path behind [Warframe HDR issue #22](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/issues/22#issuecomment-6646729243). Preview 2 now captures the selected scene texture, but the reporter still sees no tint. Capturing a preview and processing the compositor's input are separate operations.

## Changes

- Allocate private descriptor tables through a REST-created pipeline layout. ReShade's D3D12 allocator relies on private metadata attached by its add-on API; a native game root signature does not necessarily contain it. Binding still uses the original game layout and parameter indices. Allocation failure releases partial allocations.
- Count a private-input technique only after ReShade emits its completion event for that exact technique, command list and target. The void `render_technique` call can instead queue an effect variant without drawing it. Until a completion is observed, REST keeps the original compositor input and suppresses final-frame fallback for the selected effects.
- Show processing status beside the preview. Copied diagnostics include technique selection, global effects/hunting state, processing attempts, completion events and redirected draw counts. Descriptor coverage also includes retained null resource descriptors, which the previous non-null resource list omitted.

These counters describe commands recorded on the CPU; they do not prove the final GPU pixels. The raw input preview may precede the game's grading/tone mapping. Texture format alone does not establish colour space, so this build does not add an assumed conversion or switch to the final output containing the HUD.

## Repeat the existing test

1. Replace the add-on with **REST-Scene-Input-Preview-3-Modern.zip**, using `.addon64` for Warframe, and restart normally without RenderDoc. Requires ReShade 6.8+ with add-on support. Confirm **Scene input preview 3 (pre-release)** appears in REST. File version remains 1.7.3.680; `BUILD-INFO.txt` identifies the source commit.
2. Keep descriptor tracking enabled and **Shader Resource View / Pixel / Slot 2 / Binding 0**. Keep Auto Scene Colour, clamping, flip and Hide marked shaders off. Use the existing compositor `0x418d71b1` for TAA, or `0x7dcef337` for the reported DLSS setup. Finish hunting and keep the group and ReShade effects active.
3. Select **REST_SceneBoundaryCheck** in the group's Effects and enable its ReShade technique checkbox. Test it alone in native HDR with the HUD visible. Allow effect loading/reloading to finish. Toggle the tint: the scene should change while HUD colours stay unchanged.
4. After a positive tint test, disable the tint, select **SOLARIS** for the group and enable its ReShade checkbox. Test SOLARIS alone with the same settings.

No new frame capture is requested. If successful, report the mode and scene/HUD result. If tint is still absent, use **Copy preview diagnostics** from this build; the processing section is now included without enabling separate debug options. Include `ReShade.log` for a crash or rendering error.

## Validation and scope

Regression tests cover allocation through an API-owned layout, partial failure cleanup and matching completion events, including nested calls and thread isolation. Existing tests cover the real pinned SDK's non-array view conversion and supported/rejected shader containers. Full x86/x64 builds and packaging checks are required before publication. Live HDR/DLSS behaviour still requires confirmation.

The existing restrictions and resource lifetime protections remain in place. See [initial setup](scene-input-preview-1.md), [Preview 2 fixes](scene-input-preview-2.md) and [implementation limits](experimental-backend-support.md).
