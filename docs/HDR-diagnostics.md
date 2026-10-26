# REST Enhanced HDR diagnostics 4 (pre-release)

This test build follows the [build 2 results in issue #22](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/issues/22#issuecomment-6376383242). It corrects a descriptor-tracking gap and explains missing input coverage. It is **not a confirmed HDR fix**. The four-part file version remains the stable baseline (Modern 1.7.3.680 / Legacy 1.6.7.580); identify this build by **HDR diagnostics 4 (pre-release)** above the preview and the source commit in `BUILD-INFO.txt`.

## New in diagnostics 3

### Activation correction in diagnostics 4

Builds 1–3 had an older hard-coded override that forced descriptor-table tracking off at startup, even when the checkbox was on. Their reports used the configured value rather than the active registration mode. The build-3 flagged-table helper correction was present but its capture callback was not activated. This oversight was ours; the reporter supplied the requested sections correctly.

Build 4 removes that override, honours the saved setting, and reports **configured**, **registered active**, **restart required**, plus observed update/copy/tracked-bind event counts. The Options panel also shows the active mode. Changes to Track descriptors require saving and restarting; the report uses the registered mode to decide whether input inspection is available.

Before activating the dormant path, descriptor metadata copying was guarded: storage is sparse and mutex-protected, capped at 65,536 known entries per heap and 65,536 entries of work per update/copy. Unknown copies invalidate old destination metadata, partially observed source ranges remain unknown, and invalid spans do not index outside storage. High heap offsets do not allocate huge prefix vectors. Metadata omitted at limits appears as missing/partial coverage; GPU descriptors are not modified by this cache.

Unlike build 3 in practice, this build activates the table tracking/copy callbacks when enabled. It can change CPU/memory overhead and descriptor-state capture/restoration. It remains a runtime test candidate. To return to handle-only tracking, disable Track descriptors, save and restart; to roll back fully, restore the backed-up add-on.

The new integration test links the actual StateTracking.cpp and DescriptorTracking.cpp registration functions against a mock ReShade host and checks on/off callbacks, the public active-mode query and unregistration. This verifies production registration, not just snapshot helper behavior. It does not exercise a real GPU or guarantee Warframe HDR correctness.

For the next test, first check the report says **registered active: true**, **restart required: false**. Copy the complete report for `0x7dcef337` / AFTER DRAW, native HDR, menu closed, manual target 0, Auto scene colour off. Keep global effects and tracking on, individual techniques unticked, tone-map clamping/flip off and Clear alpha on. The report already includes the coverage and input sections; no shader-count information or repeated multi-scene tests are needed initially.

- Modern now captures both plain and flagged descriptor tables. ReShade exposes flagged tables for newer D3D12 root signatures; REST previously skipped them. Original GPU table handles are retained for restoration even when metadata is empty or partial. This intentionally improves state-tracking/restoration coverage, rather than only changing diagnostic text.
- The input report lists root/table coverage: plain/flagged tables, cached entries, skipped unbounded/sampler ranges, missing/partial data and clipped ranges. Root constants are labelled separately and no longer count as resource-descriptor entries.
- Finite metadata snapshots are limited to 65,536 entries per table; unbounded ranges are not allocated. These limits affect metadata, not the original GPU table handle used for replay. Unknown heaps and partial tracked ranges are reported without inventing resource views.
- Legacy/API 7 retains its plain descriptor-set representation, with the shared bounded snapshots, coverage reporting and counting correction. It does not acquire an API-20 flagged-table type.

For the first retest, use native HDR and copy **one** report for `0x7dcef337` / **AFTER DRAW**, manual target 0, Auto scene colour off, with the menu closed. Include both **Root/table coverage** and the sampled-texture section. Confirm Track descriptors and ReShade's global effects switch are on; individual effects stay unticked. There is no need to repeat all five build-2 comparisons initially.

## Install

Back up your current REST add-on and `ReshadeEffectShaderToggler.ini`. Close the game, then replace the existing add-on with the matching diagnostic build: `.addon64` for a 64-bit game such as Warframe, `.addon32` for a 32-bit game. Use the Modern ZIP for ReShade 6.8.0+; use the explicitly named Legacy ZIP for ReShade 5.8.0+. Install only one REST add-on. Keep your existing configuration and helper shaders.

## What changed

- The preview reports the actual matched shader hash/stage on Direct3D, including vertex/compute hunting, rather than displaying an unpopulated zero hash.
- Storage format and selected typed view format are shown separately.
- **Copy preview diagnostics** works in both automatic and manual target modes. It copies the selected shader, target/view formats, all colour targets tracked at that draw, capture timing/path, status and relevant settings.
- Missing targets, resource/view failures and unknown resource states have explicit status messages. A recorded capture is labelled as recorded, rather than implying that GPU pixels have been verified.
- The target-selection policy and HDR conversion are unchanged. Automatic selection still follows primary RTV 0; it does not discover compute UAV outputs. Build 3 expands table state tracking/restoration as described above.

### New in diagnostics 2

**Copy preview diagnostics** now includes **Sampled texture inputs at selected draw** for the matched shader stage. Tracked textures are identified by layout/descriptor coordinates, dimensionality, dimensions, mips, sample count, storage/view formats, usage flags and resource address. Coordinates are **not HLSL register numbers**. Inputs are recorded at the selected draw even when the output preview is captured at a later boundary.

The input inspection reads tracked descriptors and resource descriptions only. It does not sample/copy input pixels, create input views, change input resource states, modify inputs or apply effects to them. D3D12 input-texture writing remains disabled. Metadata does not automatically establish scene/UI roles or colour space. The underlying table tracking/restoration is corrected in build 3.

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
