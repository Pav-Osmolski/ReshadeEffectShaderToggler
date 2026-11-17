# Troubleshooting

[Documentation index](README.md) · [Getting started](getting-started.md)

Change one setting at a time and keep a known-good INI backup. A REST group chooses an injection boundary; the game, selected ReShade effects and other add-ons still determine the final image.

| Symptom | First checks |
|---|---|
| REST does not appear under Add-ons | Correct executable, full add-on-support ReShade build, matching Modern/Legacy release and game architecture, binary beside the injected executable |
| Helper techniques are missing or fail to compile | Supplied `REST_*.fx` files are in an effect search folder; `ReShade.fxh` is available through the include search path; reload effects and inspect errors |
| A selected effect does not run | Technique globally enabled; group active; correct hash/renderer; correct technique filter; first matching group may have already rendered that technique |
| The effect appears over the HUD | The marked boundary or selected target is too late, or injection was skipped and the technique used ordinary end-of-frame placement |
| Shader list is empty or a saved hash says not seen | Show the relevant scene, collect more frames and rescan; hashes vary by renderer, game version and settings |
| A preview is unavailable | Only single-sample, single-layer, single-mip 2D targets are supported; inspect the displayed reason |
| Vulkan waits for a safe boundary | Inspect Auto diagnostics and the transfer-usage/pass-boundary requirements; do not force a copy inside an active pass |
| HDR colours, brightness or alpha look wrong | Compare with REST effects disabled; identify the target encoding and effects' HDR support; leave ACES clamping off for PQ/HLG; see the HDR guide |
| Alpha preservation is disabled/ignored | Auto ignores it, and REST's shader-based alpha preservation is unavailable on Vulkan/OpenGL |
| Settings disappear after restarting | Use Save changes, check the INI location and write access, and ensure another REST binary/config is not being loaded |

## Upscalers and target matching

Auto Scene Colour targets the primary live colour RTV at a marked shader. Its matching range accepts output/input scale factors between 0.5 and 4.0 with compatible aspect ratio. This allows scene targets from one quarter to twice the output dimensions, including DLSS combined with a reduced game render scale. The earlier 1.85 limit rejected half-resolution targets and some combinations of upscaling and render scale. Manual target-matching modes retain their existing policy. Targets beyond the automatic range still require a different marked pass; widening the size filter does not make an unsuitable image a scene-colour target.

A first matching frame can be skipped while a new staging buffer or ReShade effect permutation is prepared. Persistent skipped work needs the diagnostic reason, rather than assuming every skipped frame is a crash. Full details are in [Automatic Scene Colour](automatic-scene-colour.md).

## Crashes and issue reports

Record the game/build, graphics API, REST version and architecture, ReShade version/fork, other add-ons, upscaler and HDR mode. Describe the exact action that failed and whether it also happens with the previous known-good configuration. For shader hunting, include the selected hash and target format/dimensions if available.

Attach a fresh `ReShade.log`, the game's own crash/log output when relevant, and REST's **Copy diagnostics** output. Review configuration files for local paths or preferences before sharing. For image problems, include expected versus actual behaviour and screenshots/captures appropriate to the HDR mode. See the [HDR test/report checklist](HDR.md).

The Warframe hunting crash in issue #15 was confirmed fixed by its reporter in official and unofficial ReShade. That confirmation does not describe or resolve the later, unspecified HDR symptoms. Separate HDR reports with reproduction details are useful.

## Testing scope

Successful x86/x64 builds verify compilation and package metadata. CPU tests cover selected format/descriptor bookkeeping and packaging. They do not run the game or certify HDR colour output, validation-layer cleanliness or performance. Read each release's recorded coverage rather than extending a BG3 result to every title and renderer.


## Crashes during shader hunting

Keep `ReShade.log` from the failed session before restarting the game, since a new launch may replace it. REST logs hunting stage/hash before selection becomes active; search for `REST hunting selection`. Record the selected destination mode and the action that triggered the crash if known. A screenshot of the shader list is not required when the log retains that context.

Windows may also retain a crash report in the Application event log and a dump under `%LOCALAPPDATA%/CrashDumps`. Dumps are not guaranteed to exist on every installation. These files can still provide useful evidence when the final log line does not show an exception. The current maintenance candidate includes a native D3D9 copy/preview state-restoration regression; see [maintenance validation](maintenance-release-validation.md#re4-crash-regression).

## Enshrouded Vulkan testing

Modern v1.7.6.680 / Legacy v1.6.10.580 and later provide a [saved Vulkan scene-transfer option](vulkan-scene-transfer-setting.md): enable it under Options > Advanced Vulkan options to use a per-game REST preference instead of the scene-transfer launcher variable. Save and restart are required; the graphics-hook workaround and Vulkan-layer argument still belong in the launcher.

Enshrouded requires `--keep-vulkan-layers` to retain ReShade's Vulkan layer. In a local SDR test on build 1287369 with ReShade 6.8.0.2155, Vulkan rendering also created DirectX helper objects. An exit crash was consistent with a ReShade D3D12 command-list proxy outliving the unloaded module. A process-only `RESHADE_DISABLE_GRAPHICS_HOOK=1` launch retained Vulkan/REST, allowed normal add-on unloading and exited successfully. This is a tested local workaround, not a global environment recommendation or proof that every exit crash has that cause.

Disabling Auto Scene Colour exposed a separate rendering crash during local testing. Manual Vulkan effect injection now skips active render passes and targets without required transfer-source usage/format support, before creating views or calling ReShade effect rendering. It does not attempt to split the game's pass. This safety guard does not establish a working pre-HUD injection point: Auto still requires a compatible image and safe continuation/transition. UI hiding and hunting are separate from effect placement.

With the opt-in transfer experiment below and a suitable earlier scene shader, local loaded-world testing confirmed CinematicDoF placement before the HUD in both SDR and HDR, at 3840×2160 output with DLSS Quality and 100% game render scale. Group off/on comparisons blurred/restored the HUD while scene blur remained, and REST reported a 2560×1440 to 3840×2160 Vulkan image blit. One Auto Scene Colour off/on check, a windowed/fullscreen round-trip and normal shutdown completed without a crash. These checks establish placement in the tested scene, not HDR colour accuracy, broad gameplay coverage or a fix for Warframe. A lower render scale exposed the automatic size-filter limit described above. After correcting that filter, a fresh loaded-world test retained sharp Display menu text at 74% and 67% scale in SDR. At 67% with HDR enabled, REST reported a 1718×967 to 3840×2160 image blit and increasing successful renders; disabling/re-enabling the group blurred/restored the menu text while scene blur remained. Normal exit was clean. This verifies placement in that tested scene/menu configuration, not all gameplay areas or HDR colour accuracy.

For targeted Vulkan boundary diagnosis, a test launch can set `REST_VULKAN_BOUNDARY_TRACE=1` for that process. The first queued Auto Scene Colour effect starts one capture per device, limited to 512 event lines and three subsequent ReShade presents. `ReShade.log` records queued target dimensions/usage, command-list identity, begin/end callbacks, barriers, eligible deferred targets/effects and present-time discarded work. These callbacks do not by themselves prove where the native render pass ended. The trace does not alter target usage, injection eligibility or GPU commands. Leave the variable unset for normal use; restart to capture again.

The optional Vulkan scene-transfer setting requests transfer-source/destination usage when eligible FP16 scene images are created. It defaults to off. Enable it in REST, save and restart; `REST_VULKAN_SCENE_TRANSFERS=1/0` remains a diagnostic launcher override. It requires an originally sampled, single-sample, single-layer, single-mip 2D colour render target, undefined initial state, no ReShade resource flags and no active resource shim. Eligibility is checked before REST changes the descriptor, then checked again afterward; combined usage must also pass the format-support query. At Vulkan image creation the heap is not yet known because memory has not been bound; unknown/default heaps are accepted and explicit CPU heaps are excluded. REST no longer automatically adds sampling usage to Vulkan attachments, whose native restrictions are not fully exposed by this callback.

The experiment does not change formats, dimensions or colour conversion. Shared/sparse images represented by ReShade flags are excluded. Native tiling, alias/protected flags, extension chains and allocation plans are not fully exposed: an empty ReShade flags field does not certify an ordinary native image. This remains opt-in; the [safety review](vulkan-transfer-safety.md) explains why it must not be enabled globally. Restart is required. The preceding scene shader must still be selected: marking a shader inside the final UI pass remains too late for pre-UI injection. The stricter guards were subsequently tested in the Modern Enshrouded candidate: SDR/HDR placement, reduced scale, toggles, hunting and clean exit passed in the recorded local setup. The maintainer also confirmed a Legacy Enshrouded SDR/HDR pass; individual Legacy operations and host version were not separately recorded.
