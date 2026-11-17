# Troubleshooting

Start with one known-good preset and change one setting at a time. Preserve the current log before restarting after a crash.

## REST is missing from Add-ons

Check the executable ReShade actually loads into, full add-on support, the correct Modern/Legacy line, game architecture and binary location. Keep one REST binary. `ReShade.log` should identify the loaded add-on path and version. [Installation checklist](getting-started.md).

## Helpers are missing or fail to compile

Install the supplied `Shaders/REST_*.fx` files in a configured effect folder. Make `ReShade.fxh` available through the include path, reload effects and read the compilation error. Installing the binary alone is not the complete helper setup.

## A selected effect does not run

Check the group is active, the marked hash is encountered, the effect is globally enabled in ReShade and the group mode includes it. **Ticked techniques are EXCLUDED** means a tick excludes that technique from injection. Required companion techniques must be enabled and ordered correctly. Disable competing groups while comparing.

Read **Successful renders** and **Last successful techniques** after Done hunting. A selected checkbox without increasing renders does not prove injection. [Effect-selection modes](effect-placement.md#understand-the-technique-checkboxes).

## Effects still cover the HUD

The selected boundary may be too late, a different group may render the effect first, or a skipped/excluded technique may still use ordinary end-of-frame ReShade placement. Compare the correct group on/off and choose an earlier scene boundary. HUD hiding and pre-UI effect placement are separate workflows.

## Vulkan waits for a safe boundary

Inspect current/last-success diagnostics and target format. The matched image needs copy support and a compatible later LOAD continuation or post-pass transition. A final UI pass may be too late. A preview alone does not prove injection is possible.

For eligible FP16 targets in setups that need it, enable [Vulkan scene transfers](vulkan-scene-transfer-setting.md), save and restart. Check Active this launch and remove any old launcher override. The option does not bypass boundary guards or fix a poor shader/technique selection. Manual Auto-off Vulkan rendering also retains pass and transfer guards.

## Upscalers and target matching

Auto accepts output/input ratios from **0.5 to 4.0** in both dimensions with compatible aspect ratio. It admits quarter-size to double-size targets, including DLSS combined with reduced render scale. Manual matching uses its separate policy.

After a resize, one matching frame may be skipped while staging/permutations are prepared. Subsequent eligible frames should update. Persistent failure needs the diagnostic reason; widening a size range does not turn an unrelated texture into scene colour.

## A shader is missing or not seen

Show the relevant element, increase collection frames and Rescan. Saved marks can be absent from one collection while still useful elsewhere. Renderer, game version and graphics settings can change hashes. [Hunting guide](shader-hunting.md).

## A preview is unavailable

Previews need supported single-sample, single-layer, single-mip 2D targets; Vulkan also needs a legal copy boundary. Read the displayed reason. A D3D9 surface is not necessarily sampleable. Preview colour is not an HDR reference.

## HDR colours or alpha look wrong

Compare with the REST group disabled, check target encoding and effects' HDR support, and leave ACES clamping off for PQ/HLG. Auto ignores alpha preservation; shader-based alpha preservation is unavailable on Vulkan/OpenGL. See [HDR](HDR.md).

## Settings disappear after restarting

Use **Save changes**, then check the loaded add-on/config path and folder write access. The isolated diagnostic launchers used during development may temporarily replace and restore configs; use your normal launcher for persistence testing. General settings such as scene transfers can require a full restart after saving.

REST maintains the previous INI as `.ini.bak`. Close the game before manually restoring it. [Saving and recovery](groups-and-configuration.md).

## Enshrouded Vulkan testing

Follow the [Enshrouded guide](games/enshrouded.md) for the normal launcher, saved transfer option, scene hash and technique inclusion. Keep the graphics-hook workaround and Vulkan-layer argument in the launcher. A final HUD shader can hide UI but still be too late for effect placement.

Local testing separated an unsafe Auto-off rendering crash from a shutdown problem consistent with a ReShade DirectX helper proxy outliving module unload. REST guards the first path; the process-only graphics-hook workaround addressed the latter in that setup. No blanket cause or fix is claimed for every Enshrouded crash.

## Crashes and issue reports

Preserve fresh `ReShade.log` before another launch replaces it. REST logs hunting stage/hash before selection becomes render-visible; search for `REST hunting selection`. Keep the game's crash report/minidump when available, without uploading it automatically.

Include:

- Game/build, renderer, REST/ReShade versions and game architecture.
- Other add-ons, HDR/upscaler settings and the exact failing action.
- Whether it reproduces with the previous known-good build/config.
- REST **Copy diagnostics**, the shader hash/destination mode if known, and the fresh log.
- Expected versus actual appearance and suitable screenshots for image issues.

Review logs/configs for personal paths before sharing. [Open an issue](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/issues/new/choose). For HDR-specific comparisons use the [HDR report checklist](HDR.md#report-an-hdr-problem).
