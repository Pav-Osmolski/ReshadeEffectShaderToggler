# REST Enhanced HDR diagnostics 2 (pre-release)

This test build follows the [new results in issue #22](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/issues/22#issuecomment-6327955448). It adds **read-only metadata for sampled texture inputs at the selected draw**, to help find the scene before Warframe combines it with the UI. It is **not a confirmed HDR fix**. The four-part file version remains the stable baseline (Modern 1.7.3.680 / Legacy 1.6.7.580); identify this build by **HDR diagnostics 2 (pre-release)** above the preview and the source commit in `BUILD-INFO.txt`.

## Install

Back up your current REST add-on and `ReshadeEffectShaderToggler.ini`. Close the game, then replace the existing add-on with the matching diagnostic build: `.addon64` for a 64-bit game such as Warframe, `.addon32` for a 32-bit game. Use the Modern ZIP for ReShade 6.8.0+; use the explicitly named Legacy ZIP for ReShade 5.8.0+. Install only one REST add-on. Keep your existing configuration and helper shaders.

## What changed

- The preview reports the actual matched shader hash/stage on Direct3D, including vertex/compute hunting, rather than displaying an unpopulated zero hash.
- Storage format and selected typed view format are shown separately.
- **Copy preview diagnostics** works in both automatic and manual target modes. It copies the selected shader, target/view formats, all colour targets tracked at that draw, capture timing/path, status and relevant settings.
- Missing targets, resource/view failures and unknown resource states have explicit status messages. A recorded capture is labelled as recorded, rather than implying that GPU pixels have been verified.
- The target-selection policy, HDR conversion and rendering/injection behavior are unchanged. Automatic selection still follows primary RTV 0; it does not discover compute UAV outputs.

### New in diagnostics 2

**Copy preview diagnostics** now includes **Sampled texture inputs at selected draw** for the matched shader stage. Tracked textures are identified by layout/descriptor coordinates, dimensionality, dimensions, mips, sample count, storage/view formats, usage flags and resource address. Coordinates are **not HLSL register numbers**. Inputs are recorded at the selected draw even when the output preview is captured at a later boundary.

The new inspection reads existing tracked descriptors and resource descriptions only. It does not sample/copy input pixels, create input views, change resource states, modify inputs or apply effects to them. Existing output preview/injection behavior is unchanged; D3D12 input-texture writing remains disabled. Metadata does not automatically establish scene/UI roles or colour space.

The scan checks at most 64 layout parameters, 256 entries per parameter, 64 non-null sampled views per snapshot and 1,024 descriptor entries across all snapshots in a frame. It reports truncation. Only sampled-resource and combined sampler/resource descriptors are considered; UAVs, standalone samplers, constant buffers and unobserved entries are excluded. Buffer-backed SRVs are not listed as textures. A missing or truncated list is not proof that the shader has no inputs.

## Warframe test

1. Use **native HDR** without an HDR add-on and untick individual ReShade techniques, including SOLARIS. Keep REST and ReShade's **global effects switch on**, because it gates REST preview updates. Keep tone-map clamping/flip off and Clear alpha on.
2. Ensure **Track descriptors** is enabled in REST's Options. If it was off, enable it, save the configuration and restart before testing. A disabled-tracking message means the input list is unavailable.
3. Select `0x7dcef337`, **Auto scene colour off**, manual target index 0, **AFTER DRAW**, matching the stable 10-bit result. Copy preview diagnostics with the game menu closed, then open it and copy again. Attach a screenshot per state and note whether the main game UI already appears in the preview. Compare **BEFORE DRAW** once if possible.
4. Select `0x83cc42e0`, manual target index 0, **BEFORE DRAW**. Copy reports with the menu open and closed, preferably when the output formats differ. This helps distinguish shader reuse across different targets.

Start with step 3 if time is limited. No need to repeat the original two-hash SDR/alpha tests. The goal is to identify the scene input, not force an HDR conversion.

No GPU frame capture is required for this initial metadata report. If the inputs remain ambiguous, a D3D12 frame capture may be needed. Existing logs do not need to be resent unless a new failure occurs. **AFTER DRAW** defers capture to a matching pipeline-bind boundary; it is not guaranteed to mean immediately after exactly one draw. Tracked bindings do not prove which textures the shader actually samples or what their pixels contain.

## Restore stable

Close the game and restore the backed-up stable add-on. Restore the INI too if you saved experimental target/invocation settings. This diagnostic build does not change the configuration format.

## Validation limits

Build/CPU checks do not establish Warframe HDR correctness. Native HDR target selection, preview pixels, effect placement and RenoDX combinations still require runtime testing. Neither a universal HDR fix nor automatic compute-output injection is claimed.
