# Bounded scene-scratch submission prototype

This is a standalone research executable, not an add-on path, new preview or Warframe fix. Neither the bridge header nor test executable is linked into REST. Modern and Legacy carry the same prototype so this investigation stays on the two feature branches.

## Result: restricted proof passed

`pwsh -NoProfile -File scripts/test-scene-submission-prototype.ps1` runs native D3D12 WARP with the debug layer required. On the local machine it passed:

| Case | Evidence |
| --- | --- |
| 32 closed recordings retained simultaneously | All bind one shared `R16G16B16A16_FLOAT` scratch texture, with immutable per-recording descriptor heaps. One 64 KiB physical texture allocation in this 1×1 test; no count-based scratch acquisition or skipped draw. Inputs/readbacks/descriptors still scale with recordings. This is not a 4K memory or performance measurement. |
| Reverse order, rotated batches and four resubmissions | Each recording executes a compute transform with a unique red value, a channel above 1, a negative channel and fractional alpha; readbacks match exact FP16 bits. |
| Canonical state | Every native recording starts and ends with scratch in unordered-access state. Copy transitions are unconditional, rather than depending on CPU state from a previous recording. |
| Whole-batch completion | A GPU gate holds execution; the post-batch completion ticket remains pending. |
| Two simultaneous CPU submitters on the same queue | A bridge mutex covers validation, all per-list native calls and the completion signal. Tickets are distinct and outputs remain exact. |
| Pending execution plus recording replacement/reset model | A fresh immutable recording version/allocator replaces the CPU identity. The bridge retains the old version and all its native objects until GPU completion. This models version replacement; it does not call the production ReShade reset callback. |
| Abandoned recording | Unsubmitted version is destroyed without changing scratch GPU state or leaving a flight. |
| Invalid batch, repeated/in-flight native list, second queue | Rejected before any native submission. The test is not evidence of successful cross-queue processing. |
| Injected signal failure after real native execution | No completion ticket is published; in-flight references are quarantined and further submissions/collection fail. An explicit external test-owned drain precedes teardown. Recovery and device-loss teardown are not implemented. |
| Native validation | Zero D3D12 debug errors/corruption. Fence waits in the test time out after 15 seconds rather than hanging indefinitely. |

The restricted bridge separates the native ExecuteCommandLists call for every list. Microsoft documents that separate calls guarantee completion ordering between calls, unlike lists in one call: [ExecuteCommandLists](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-executecommandlists). This is stronger ordering and may cost performance. The harness owns the whole queue submission; the current ReShade per-list event cannot substitute for it.

## What this proves

Closed-recording lifetime does not inherently require a separate physical scratch image for each recording when all those recordings use canonical states and a real submission owner orders their execution. The old eight-image rejection is avoided in this controlled single-queue case while recordings remain valid for resubmission.

## What remains unproven

1. The existing ReShade API still does not let REST own/split a native batch. No native hook or dependency fork is installed by this work.
2. The queue must be known before sharing scratch during recording. A command list can be recorded before its eventual queue is known. Rejecting a second queue is a harness boundary, not a usable game fallback: an add-on cannot simply suppress the game's submission or safely pass through injected shared-scratch commands without the required ordering.
3. A universal global fence chain across queues can introduce cycles with existing game waits. The prototype therefore inserts no cross-queue waits. General concurrent/cross-queue support remains a blocker, not a passed test.
4. The small workload uses a real FP16 compute shader and texture readbacks. The follow-up below additionally replays the captured split shader pair, but neither test runs the complete REST/ReShade technique renderer, production descriptor cloning/state restoration or live-game integration. HDR colour correctness and those renderer-level checks remain required.
5. Signal-failure quarantine has no automatic recovery. The bridge requires an external proven drain before destruction; it is not suitable for uncontrolled device/queue teardown.
6. Mutable/volatile descriptor behaviour, multiple device queues, unaffected game lists, secondary lists, device removal and queue/command-list interception must be assessed before any integration.

## Next boundary

Establish a submission integration contract that can identify or safely bind the queue before recording shared scratch, observe whole batches, and preserve the game's queue dependencies. Then replay the actual compositor with canonical entry/exit states and immutable recording versions. Keep the existing runtime image pool untouched until that contract is implemented and tested. No new user capture is required for these local design questions.

## Captured compositor replay follow-up

The standalone native replay tool now runs the captured production shader-pair outputs rather than only the small compute workload. The local TAA, no-TAA and FSR exports (3824×1600) each passed 32 simultaneously retained recordings sharing one FP32 scene/processed/R32-alpha scratch set, three reordered submissions, and bit-exact identity/tint/alpha output with no D3D12 debug errors. Identity outputs matched the original saved captures and tint outputs matched the prior validated split reference.

The tool uses a dedicated tint/alpha-overwrite pass rather than arbitrary ReShade effects. Per-recording readbacks/descriptors still scale, and its scratch set has three intermediate images plus the output. This advances captured compositor replay evidence but does not cover live queue integration, actual ReShade rendering/state restoration or cross-queue execution. Run instructions are in [the replay tool guide](../tools/compositor-probe/README.md#retained-recordings-with-one-shared-scratch-set).
