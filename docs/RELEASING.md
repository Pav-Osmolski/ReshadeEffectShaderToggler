# Release Process

This fork uses GitHub Actions for validation and tagged releases. For local commands see [Building](BUILDING.md); user instructions start at the [documentation index](README.md).

| Release line | Maintained branch | Version suffix | Latest designation |
|---|---|---|---|
| Modern | `main` | `.680` | Yes |
| Legacy / ReShade 5.x | `legacy/reshade-5.x` | `.580` | No |

## Version format

Release tags must use:

`vMAJOR.MINOR.PATCH.RESHADE`

Example:

`v1.7.3.680`

The final component preserves the upstream convention used for the ReShade baseline.

The source tree contains a human-readable default version in `src/version.h`. During a tagged release, the workflow validates the tag and runs `src/set_release_version.ps1` so the packaged binaries are stamped from the tag itself.

## Pre-release checklist

Before tagging a release:

1. Confirm the intended changes are merged into `main`.
2. Confirm **MSBuild** is green for the final PR merge result, then for the maintained-branch merge commit. Keep revisions in draft and mark the PR ready for review when final. If it changes afterward, return it to draft and mark ready again (or reopen it) to refresh required PR checks. No synchronize trigger is configured.
3. Confirm both **x64** and **x86** Release configurations build successfully.
4. Check that `README.md` and files under `docs/` describe any user-visible changes.
5. Confirm the default version in `src/version.h` matches the release you intend to tag.
6. Runtime-test the add-on in at least one representative configuration for the release's main feature. For architecture/API changes, smoke-test both x64 and x86 in representative titles where available.
7. For Automatic Scene Colour changes, use the BG3 DX11 and Vulkan matrices below and record the actual coverage. Do not describe unrun configurations as passed.
8. Add the version to `CHANGELOG.md` and write `docs/releases/<tag>.md`; the release workflow publishes that file as the release notes.

### Automatic Scene Colour regression matrix

The D3D regression reference is **Baldur's Gate 3 launched through `bg3_dx11.exe`**. Vulkan testing is separate and does not establish D3D12 or DX11 runtime coverage.

1. **DX11 + DLSS enabled**
   - Auto scene colour is clickable.
   - The editor reports `D3D11`.
   - The matched scene resolution follows the DLSS internal resolution.
   - The effect resolution follows the ReShade/output resolution and reports native staging when the dimensions differ.
   - A multi-pass chain such as `Lumenite_Kernel -> Lumenite_LSAO` updates every frame while the camera moves.
   - AO/effects remain beneath later BG3 UI and fog composition.
2. **DX11 without an internal-resolution mismatch**
   - Auto remains functional.
   - Scene and effect resolutions can match without native staging.
   - The effect still updates every frame and remains at the selected shader boundary.
3. **Configuration preservation**
   - Record manual render destination, render-target index, swapchain-match mode and Preserve Alpha settings.
   - Toggle Auto on, close/reopen the editor, then toggle Auto off.
   - Confirm all manual settings are unchanged.
4. **Persistence and ReShade rebuilds**
   - Save the toggle groups and restart the game.
   - Confirm the Auto flag and selected techniques persist.
   - Reload effects and reorder techniques in ReShade.
   - Confirm selected technique names remain selected and the required multi-pass order still renders.
5. **Resolution changes**
   - Change DLSS/output resolution while Auto is active.
   - One matching frame may be skipped while native staging is recreated; subsequent frames must update normally.
6. **Unsupported API safety**
   - A saved Auto preference must not suppress or overwrite the group's manual render-target configuration on an unsupported API.

D3D10, D3D11, D3D12 and Vulkan are supported on x86 and x64. BG3 DX11 remains the primary D3D regression reference because it exercises the full dynamic-resolution/native-staging path.

### Vulkan Auto Scene Colour regression matrix

For Vulkan changes, validate a representative Vulkan title with Auto Scene Colour enabled and record which cases were exercised:

1. **Safe continuation boundary**
   - Auto Scene Colour is clickable and reports `Vulkan`.
   - Mark a shader in a pass whose colour target is reused by a later render pass with **LOAD** semantics.
   - Before a safe boundary is found, the editor may report **Waiting for safe Vulkan continuation or target transition**.
   - Confirm the effect is injected before the first compatible same-target LOAD pass, not inside the matched game render pass.
2. **Same-resolution path**
   - Scene and effect resolutions match.
   - Confirm **Native staging: Not required**.
   - Selected effects update every frame once the safe continuation boundary is reached.
3. **Native-staging/upscaling path**
   - Use a Vulkan title/configuration where the live scene resolution differs from the ReShade/output resolution.
   - Confirm **Native staging: Active (Vulkan blit)**.
   - Copy diagnostics and confirm **Vulkan image blit** staging and the actual successful **Vulkan boundary**: `same-target LOAD pass` or `post-pass RT transition`.
   - Confirm the image remains live while moving the camera and that the effect is not frozen or one frame behind.
   - Confirm later game UI/post-processing remains above the injected effect when the compatible continuation pass precedes those passes.
4. **Same-pass limitation**
   - Test a boundary where later composition remains inside the same Vulkan render pass, if one is readily identifiable.
   - REST must not issue the Auto injection inside that pass or destabilise the game; pending work should be skipped/reset safely.
5. **Resolution changes**
   - Change the internal/output resolution while Auto is active.
   - Allow one frame for staging recreation, then confirm continuous updates resume.
6. **Fallback safety**
   - Confirm CLEAR/DISCARD continuation passes, missing transfer-source/transfer-destination usage, unsupported transfer formats and multisampled targets do not attempt unsafe Auto staging.
   - Verify the editor reports the relevant eligibility reason and the globally enabled technique can fall back to ordinary end-of-frame ReShade placement.
7. **D3D regression**
   - Re-run the BG3 DX11 + DLSS matrix after Vulkan changes; document any unavailable runtime coverage explicitly.
8. **Post-pass transition and subpass safety**
   - Use a candidate without a later same-target LOAD pass, such as `0x782733c1` in the tested BG3 configuration.
   - Confirm `post-pass RT transition`, increasing successful renders, no flicker and effects below subsequent fog/UI.
   - End/begin callbacks for subpass transitions must not trigger effects or preview copies. A real later barrier must establish pass completion.
9. **Rapid hunting navigation**
   - Alternate Prev/Next rapidly for a sustained period across targets with different dimensions/formats.
   - Exercise marked navigation, Rescan, Done and reopening Settings.
   - Confirm no crash/device loss, valid preview status and stable committed Auto injection afterward.

For v1.6.1.633 the user confirmed BG3 Vulkan native staging and post-pass Before Fog injection remained correct while exercising the updated hunting UI, deterministic Prev/Next ordering, repeated held navigation across many shaders and preview targets, with no observed crash/device loss after the Vulkan preview-lifetime fix. This release task does not claim a fresh DX11, same-resolution Vulkan or x86 in-game test. Automated architecture checks cover both x86 and x64 binaries.

For API-specific changes, verify in a representative title that Auto Scene Colour is clickable, the live scene/effect resolutions are reported correctly, native staging activates only when needed, and the effect remains at the intended shader boundary.

### Architecture parity

CI must validate both release binaries after every x86/x64 build:

- x86 output is a PE32/i386 DLL with the `.addon32` extension;
- x64 output is a PE32+/x86-64 DLL with the `.addon64` extension;
- both carry the committed REST version;
- both export the required REST add-on metadata;
- ReShade resource/resource-view handles remain 64-bit in both builds.

The D3D10/D3D11/D3D12 Auto Scene Colour path retains the existing shader-based staging copy on both architectures. Vulkan uses ReShade's generic image-blit path for native staging. REST Release builds treat compiler warnings as errors on both architectures.

Legacy game-specific hooks may be architecture-specific. The FFXIV constant-copy hook is x64-only and is excluded from Win32 builds because its signatures target 64-bit game code.

## Creating a release

For a release PR, commit the matching `src/version.h`, changelog entry and `docs/releases/vMAJOR.MINOR.PATCH.RESHADE.md`, then merge it into `main` with the merge commit title:

`Release vMAJOR.MINOR.PATCH.RESHADE`

The guarded `tag` job runs inside **MSBuild**, after its `build` job succeeds for a push to the maintained branch. It checks out that validated commit, reads its source version, verifies the release title and notes, and checks any existing tag points to that exact commit. It then creates the tag and explicitly dispatches **Release** at the tag. Ordinary commits skip this job and do not create tags or additional workflow runs. Release titles must match exactly on their first line. Both maintained branches contain their own release gate; no default-branch `workflow_run` relay is needed.

The publisher validates its maintained line's version suffix and branch ancestry, rebuilds both architectures, runs regression tests and shared binary validation, then publishes. Per-tag concurrency prevents overlapping publishers. Existing releases/assets are immutable; a manual publisher retry against an existing release fails instead of replacing files. Never move or reuse a tag.

A tag may still be created manually from the desired `main` commit when needed.

The tag-driven **Release** workflow will then:

1. validate the tag format;
2. stamp `src/version.h` from the tag inside the build runner;
3. build x64 and x86 Release configurations;
4. package:
   - `ReshadeEffectShaderToggler.addon64`;
   - `ReshadeEffectShaderToggler.addon32`;
   - `LICENSE`;
   - `README.md`;
   - `CHANGELOG.md`;
   - `BUILD-INFO.txt`;
   - `Shaders/`;
   - `docs/`;
5. create `release.zip` and the BG3 configuration ZIP from the shared manifest;
6. generate checksums for both archives and a BG3 source/provenance file;
7. publish all assets using the committed `docs/releases/<tag>.md` notes.

No release is published if either architecture fails to build.

## CI artifacts

One **MSBuild** workflow handles validation. Build-affecting pushes to `main` validate the merged result. Pull requests targeting that branch validate when opened, reopened or marked ready for review. Draft PRs skip the job and allocate no runner. Later WIP commits do not trigger builds: keep the PR in draft during revisions, then mark it ready once final (or reopen it) to refresh the required `build` check. Branch protection requires a successful check for the current PR revision.

Documentation-only PRs still report the required `build` check, but skip dependency downloads, native compilation and artifact packaging. Workflow, script, test and unknown file changes receive full validation. Release-titled pushes always build both architectures, including a promotion that only changes release notes. Push path filters include every workflow file and release notes.

Concurrent validation runs for the same PR or branch cancel superseded work. Release promotions have a separate concurrency group and are not cancelled by ordinary validation. Manual dispatch remains available for an explicitly requested native test build; it may not satisfy a PR-associated merge check. Workflow structure, PowerShell parsing and regression tests run in the same job, with no duplicate workflow-check run. A normal code PR needs one final review validation and one maintained-branch validation after merging; stable publication adds the separate **Release** workflow.

The release and CI workflows share `scripts/validate-binaries.ps1` and `scripts/package-build.py`. One checked-in `scripts/package-manifest.json` defines their payloads, including CHANGELOG, shaders and docs. Both include BUILD-INFO with source version and full commit identity; CI identifies itself as a test candidate.

The resulting CI artifact is retained for 14 days and contains a checksummed test-candidate ZIP with both add-on binaries, shaders, docs and BUILD-INFO identifying the exact commit. It is intended for validation, not long-term distribution.

## Rollback

If a release has a runtime regression:

1. do not reuse or move the existing tag;
2. fix the issue on `main`;
3. validate the fix through the normal MSBuild workflow;
4. publish a new patch tag.

Keeping release tags immutable makes binary/source provenance much easier to reason about.

## Reported runtime validation — 19 October 2026

The maintainer tested the new builds in Baldur's Gate 3 on D3D11 and Vulkan and reports that functionality, including shader hunting, worked correctly with no crashes observed. Exact ReShade versions, architectures and separate modern/legacy coverage were not specified. This records the reported coverage without extending it to Warframe DX12, D3D10, HDR-specific cases or every case in the matrix below. Both maintenance releases were authorized following this testing.

## GPU hardening regression matrix

Record the game, renderer, architecture, ReShade version, commit and actual outcomes. CI tests eligibility and descriptor-offset bookkeeping; it cannot certify GPU execution.

- Warframe DX12, issue #15: select the previously failing hashes; alternate targets of different sizes/formats, including rapid browsing. Test Clear alpha off/on with unrelated effects disabled first, then with the reporter's effects restored.
- BG3 DX11: existing Before Fog setup with DLSS on/off, rapid hunting, and unchanged configuration persistence.
- BG3 Vulkan: same-target LOAD and post-pass-transition injection, rapid hunting, resolution changes, and effects below later fog/UI. Confirm a continuation draw works without explicitly rebinding its pipeline.
- Representative DX12 HDR title: FP16 and 10-bit targets, alpha-clearing previews and native staging. Record unavailable cases rather than mark them passed.
- Dynamic Vulkan descriptors: bind multiple sets, partially rebind a later set, inject effects, and confirm original offsets remain bound.
- Buffer lifetime: repeatedly change output/internal resolution and disable copied binding/alpha buffers with multiple frames in flight; collect debug-layer/validation output when possible.
- Run both x86 and x64 in representative games where available. A build pass alone is not runtime coverage.

## Optional BG3 configuration asset

BG3 configuration sources now live under `configs/BG3/`, including their original archive provenance. The shared packager creates `BG3-Configs-D3D11+Vulkan.zip` for both CI and releases. `SHA256SUMS.txt` covers both the main archive and the configuration archive. A separate `BG3-Configs-SHA256SUMS.txt` and provenance file are also published for convenience. Review configuration changes as source changes and runtime-test them separately; packaging does not establish game compatibility.

Dependencies are fetched with `scripts/fetch-dependencies.ps1`: shallow, parallel submodule downloads at the exact committed revisions. Main repository history remains available for complete PR diffs and release ancestry checks. No dependency follows its upstream tip automatically.

For HDR-specific validation, use [HDR handling and validation](HDR.md). Published releases are not rewritten by these packaging changes; they apply to subsequent builds/releases.
