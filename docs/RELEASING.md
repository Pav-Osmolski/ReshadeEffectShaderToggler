# Release Process

This fork uses GitHub Actions for validation and tagged releases.

## Version format

Release tags must use:

`vMAJOR.MINOR.PATCH.RESHADE`

Example:

`v1.6.6.580`

The final component preserves the upstream convention used for the ReShade baseline.

The source tree contains a human-readable default version in `src/version.h`. During a tagged release, the workflow validates the tag and runs `src/set_release_version.ps1` so the packaged binaries are stamped from the tag itself.

## Pre-release checklist

Before tagging a release:

1. Confirm the intended changes are merged into `legacy/reshade-5.x`.
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

For a release PR, commit the matching `src/version.h`, changelog entry and `docs/releases/vMAJOR.MINOR.PATCH.RESHADE.md`, then merge it into `legacy/reshade-5.x` with the merge commit title:

`Release vMAJOR.MINOR.PATCH.RESHADE`

The guarded **Tag Release** workflow is hosted on the default branch and starts only after a successful push-triggered **MSBuild** run on `main` or `legacy/reshade-5.x`. It checks out that validated commit, reads its source version, verifies the release title and notes, and checks any existing tag points to that exact commit. It then creates the tag and explicitly dispatches **Release** at the tag. Ordinary commits do not create tags. Release titles must match exactly on their first line.

The publisher validates its maintained line's version suffix and branch ancestry, rebuilds both architectures, runs regression tests and shared binary validation, then publishes. Per-tag concurrency prevents overlapping publishers. Existing releases/assets are immutable; a manual publisher retry against an existing release fails instead of replacing files. Never move or reuse a tag.

A tag may still be created manually from the desired `legacy/reshade-5.x` commit when needed.

The tag-driven **Release** workflow will then:

1. validate the tag format;
2. stamp `src/version.h` from the tag inside the build runner;
3. build x64 and x86 Release configurations;
4. package:
   - `ReshadeEffectShaderToggler.addon64`;
   - `ReshadeEffectShaderToggler.addon32`;
   - `LICENSE`;
   - `README.md`;
   - `Shaders/`;
   - `docs/`;
5. create `release.zip`;
6. generate `SHA256SUMS.txt`;
7. publish a GitHub Release using the committed `docs/releases/<tag>.md` notes.

No release is published if either architecture fails to build.

## CI artifacts

Build-affecting pushes to `legacy/reshade-5.x` build both architectures. Pull requests targeting `legacy/reshade-5.x` validate on opened, reopened or ready-for-review when source, dependencies, build scripts or MSBuild workflow paths change. Later PR commits do not trigger automatic builds: return the PR to draft and mark it ready after final revisions (or reopen it) to refresh required PR-associated checks. Manual dispatch is available for branch test artifacts; GitHub may not associate those runs with the PR merge check. Documentation-only PRs also validate on these lifecycle events, so the required build check cannot remain missing solely because of a path filter. Workflow checks cover every workflow YAML and embedded PowerShell block.

The release and CI workflows share `scripts/validate-binaries.ps1`, avoiding divergent architecture/version/export checks.

The resulting CI artifact is retained for 14 days and contains a checksummed test-candidate ZIP with both add-on binaries, shaders, docs and BUILD-INFO identifying the exact commit. It is intended for validation, not long-term distribution.

## Rollback

If a release has a runtime regression:

1. do not reuse or move the existing tag;
2. fix the issue on `legacy/reshade-5.x`;
3. validate the fix through the normal MSBuild workflow;
4. publish a new patch tag.

Keeping release tags immutable makes binary/source provenance much easier to reason about.

## GPU hardening regression matrix

Record the game, renderer, architecture, ReShade version, commit and actual outcomes. CI tests eligibility and descriptor-offset bookkeeping; it cannot certify GPU execution.

- Warframe DX12, issue #15: select the previously failing hashes; alternate targets of different sizes/formats, including rapid browsing. Test Clear alpha off/on with unrelated effects disabled first, then with the reporter's effects restored.
- BG3 DX11: existing Before Fog setup with DLSS on/off, rapid hunting, and unchanged configuration persistence.
- BG3 Vulkan: same-target LOAD and post-pass-transition injection, rapid hunting, resolution changes, and effects below later fog/UI. Confirm a continuation draw works without explicitly rebinding its pipeline.
- Representative DX12 HDR title: FP16 and 10-bit targets, alpha-clearing previews and native staging. Record unavailable cases rather than mark them passed.
- Legacy Vulkan buffer descriptor layouts: confirm the explicit API 7 offset-restoration diagnostic and ordinary end-of-frame fallback. API 7 does not expose offsets or distinguish dynamic buffer descriptors; these layouts are conservatively skipped. Dynamic-offset injection tests apply to the modern API 20 line.
- Buffer lifetime: repeatedly change output/internal resolution and disable copied binding/alpha buffers with multiple frames in flight; collect debug-layer/validation output when possible.
- Run both x86 and x64 in representative games where available. A build pass alone is not runtime coverage.

## Optional BG3 configuration asset

The BG3 config ZIP is maintained separately from the source release package. It is not automatically regenerated by Release. Before attaching it, verify its D3D11/Vulkan INIs against the release, record its source/version and SHA-256, and attach a separate checksum file. Do not replace existing release assets silently. The automatically generated SHA256SUMS covers release.zip only.

Legacy fixes are backported onto `legacy/reshade-5.x` using the ReShade 5.8/API 7 SDK. Do not merge the API 20 migration into this branch. The default-branch tagger routes `.580` releases here and publishes them with `--latest=false`.
