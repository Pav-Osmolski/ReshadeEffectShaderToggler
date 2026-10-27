# REST Enhanced HDR diagnostics 5 (pre-release)

This build follows the [diagnostics 4 report in issue #22](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/issues/22#issuecomment-6427642806). Descriptor tracking is now confirmed active for that Warframe D3D12 draw. Its full-resolution floating-point input is a scene candidate; a separate full-resolution 8-bit input is a UI candidate. Their contents and colour spaces remain unverified.

**This is an investigation build, not a confirmed HDR fix.** File versions remain the stable baseline (Modern 1.7.3.680 / Legacy 1.6.7.580). Identify the test build by **HDR diagnostics 5 (pre-release)** and the commit in `BUILD-INFO.txt`.

## One focused Warframe test

1. Back up the installed REST add-on and INI. Close the game, install the **Modern** `.addon64` from this release, and keep only one REST add-on installed. Legacy is for ReShade 5.8.0+; Modern is for ReShade 6.8.0+.
2. Use native HDR, game menu closed, ReShade global effects on and individual techniques unticked. Keep Track descriptors enabled, Auto scene colour off, manual target 0, Clear alpha on, tone-map clamping/flip off.
3. Hunt `0x7dcef337` with **AFTER DRAW**. In its preview, enable **Trace earlier colour-target draws (diagnostic)**. This is a session-only option; no save/restart is needed for the trace. Wait a few frames, then press **Copy preview diagnostics** and paste the complete report into issue #22.
4. Check that the report says **HDR diagnostics 5**, **registered active: true**, **restart required: false**, and **Earlier colour-target trace enabled: true**. If descriptor tracking itself needs changing, save and restart first; that setting has separate restart semantics.

No new screenshots, SDR comparisons or shader-count reports are needed initially. A missing earlier draw is a useful result; do not change many other settings to force a match.

## What the trace means

The existing **Sampled texture inputs at selected draw** section now optionally adds an **Earlier bound-target draw** beneath each texture, with PS/VS hashes, RTV index and a recording ordinal. It matches the exact resource handle within the current command-list recording. Resources are discovered at runtime; the reporter's addresses are not hardcoded.

These are **candidate draws that bound the resource as a colour target**, not verified pixel writes, proven scene/UI roles or guaranteed GPU execution order. A shader may leave pixels untouched, use multiple subresources, or have its work suppressed by another add-on. The trace matches whole resources, not individual mips/slices. It does not inspect input pixels, compile/disassemble game shaders, change resource states or modify textures. D3D12 rendering into sampled input textures remains disabled.

Histories stay separate per command list and reset when the list resets. Any resource creation/destruction or trace toggle changes the device generation and discards old history at the next observation, preventing stale handles from matching across lifetimes. Copies/resolves and explicit colour/UAV clears discard the destination's draw history. Compute/unknown indirect operations and secondary-list execution discard the list's history because provenance is not established. Unobserved operations remain a limitation; the trace is deliberately a hint, not a complete write ledger.

No attempt is made to merge independently recorded command lists or infer queue ordering. A missing result can mean another command list, compute production, a transfer/clear, a lifetime reset or a limit. It does not mean the texture has no producer. Reused command lists do not acquire a new recording history merely because they execute again.

The cache holds at most 512 resource candidates and observes at most 32,768 graphics callbacks per retained segment, with at most eight colour targets per callback. Hitting either limit clears candidates and reports the limit until reset. An indirect graphics batch counts as one callback; its GPU draw count is not reconstructed. Tracing starts disabled and is D3D12-only in both packages. Other backends retain their existing diagnostics; no new backend certification is claimed.

If a credible scene candidate appears, the next test will hunt that producer hash and inspect its output before attempting effects. If the producer is outside the retained recording, a GPU frame capture may be needed rather than more metadata builds.

## Existing diagnostics

Storage/view formats, draw targets, matched shader/stage, capture timing/path and bounded sampled-input metadata remain available. Layout/descriptor coordinates are not HLSL register numbers. Buffer views, UAVs and non-resource descriptors are not printed as sampled textures. The sampled-view count precedes texture filtering, so it can exceed the number of texture lines.

The input scan is limited to 64 layout parameters, 256 entries per parameter, 64 sampled views per snapshot and 1,024 inspected descriptors per frame. It reports truncation. Finite descriptor snapshots and sparse heap metadata are bounded at 65,536 entries; unknown source copies invalidate stale destination metadata. Build 3 added flagged descriptor-table handling; build 4 removed the old override that had forced its capture path off. The build-4 activation correction changes tracking/restoration coverage and may add CPU overhead when Track descriptors is enabled.

Format alone does not establish linear, PQ or HLG colour space. **AFTER DRAW** captures at the next matching pipeline-bind boundary, not necessarily immediately after one draw. The preview's **Capture recorded (GPU pixels not verified)** status does not validate image contents. Vulkan previews retain their existing raw-copy limitations.

## Restore stable

Close the game and restore the backed-up add-on. Restore the INI if you saved experimental settings. The trace option is not saved. Turning Track descriptors off and saving/restarting restores handle-only tracking; restoring the stable add-on is the full rollback.

## Validation limits

CPU tests cover candidate replacement, isolation between histories, lifetime generations, destination invalidation, reset and capacity/work limits. Native x86/x64 builds and the existing registration tests check compilation and descriptor activation. They cannot establish Warframe HDR correctness, actual pixel writes, effect placement, performance or RenoDX compatibility.
