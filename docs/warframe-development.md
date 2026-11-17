# Warframe HDR development

This feature branch contains experimental issue #22 work. It is not a stable release or a confirmed fix for the remaining flicker.

## Branches

- Modern: `feature/scene-input-and-backend-support`, synchronized with `main`.
- Legacy: `feature/legacy-scene-input-and-backend-support`, synchronized with `main` through the matching maintained `legacy/reshade-5.x` implementation. Keep its ReShade 5.8 SDK, compatibility adapters and Legacy release routing.
- The full histories of `diagnose/hdr-preview-22` and `diagnose/legacy-hdr-preview-22` are already ancestors of the corresponding feature branches. Those diagnostic branches are retired; continue development only on these two feature branches.

The November 2026 consolidation includes the stable D3D9 lifecycle fixes, Vulkan safety and scene-transfer preference, documentation site, and explicit release-archive naming. It preserves descriptor tracking and producer tracing on both lines, plus the Modern-only private scene-input images, compositor split and native scene-input regressions. Legacy remains the diagnostic counterpart; it does not gain Modern scene-input processing through this synchronization. Stable Vulkan behaviour and experimental D3D12 resource ownership remain separate paths.

## Remaining blocker

The reporter's captured failure is `Scene input: active image capacity reached`. A local native D3D12 test reproduces exhaustion even after GPU completion: valid closed recordings still own their image pairs and may legally be submitted again. Red entries in a shader-hunting collection do not establish the cause of this captured capacity failure.

Do not increase the pool again or release recording-owned resources merely because a fence completed. Neither resolves safe resubmission. The [lifetime investigation](scene-input-lifetime-investigation.md) records the reproduction and replacement acceptance criteria.

Next work is a bounded-scratch submission prototype with explicit ordering, canonical resource states, whole-batch/concurrent queue handling, and retained recording lifetime. It must pass legal reordering, resubmission, abandonment/reset, failure and actual compositor tests before another public preview. The current consolidation does not implement that redesign or claim to fix flicker.

No additional reporter capture is needed for the established capacity failure. Do not publish a new testing build solely for this branch synchronization.

## Validation

Run `scripts/test-rendering-safety.ps1` (including scene-input SDK/native tests and maintained-backend regressions), the Python packaging/routing tests, workflow validation and complete x64/x86 CI builds on both feature branches. Use `workflow_dispatch` on each feature branch; do not tag or merge this experimental work into a stable branch.
