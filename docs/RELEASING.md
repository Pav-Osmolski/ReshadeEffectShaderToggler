# Release Process

This fork uses GitHub Actions for validation and tagged releases.

## Version format

Release tags must use:

`vMAJOR.MINOR.PATCH.RESHADE`

Example:

`v1.5.0.633`

The final component preserves the upstream convention used for the ReShade baseline.

The source tree contains a human-readable default version in `src/version.h`. During a tagged release, the workflow validates the tag and runs `src/set_release_version.ps1` so the packaged binaries are stamped from the tag itself.

## Pre-release checklist

Before tagging a release:

1. Confirm the intended changes are merged into `main`.
2. Confirm the **MSBuild** workflow is green for the merge commit.
3. Confirm both **x64** and **x86** Release configurations build successfully.
4. Check that `README.md` and files under `docs/` describe any user-visible changes.
5. Confirm the default version in `src/version.h` matches the release you intend to tag.
6. Runtime-test the add-on in at least one representative configuration for the release's main feature. For architecture/API changes, smoke-test both x64 and x86 in representative titles where available.
7. For Automatic Scene Colour changes, run the BG3 DX11 regression matrix below before tagging a release.

### Automatic Scene Colour regression matrix

The primary release gate is **Baldur's Gate 3 launched through `bg3_dx11.exe`**. Do not substitute Vulkan or assume D3D12 represents the BG3 path.

1. **DX11 + DLSS enabled**
   - Auto scene colour is clickable.
   - The editor reports `D3D11 (BG3 validated)`.
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
   - Auto Scene Colour must not activate on Vulkan.
   - A saved Auto preference must not suppress or overwrite the group's manual render-target configuration on an unsupported API.

D3D10, D3D11 and D3D12 are supported on x86 and x64. BG3 DX11 remains the primary runtime regression reference because it exercises the full dynamic-resolution/native-staging path.

For API-specific changes, verify in a representative title that Auto Scene Colour is clickable, the live scene/effect resolutions are reported correctly, native staging activates only when needed, and the effect remains at the intended shader boundary.

### Architecture parity

CI must validate both release binaries after every x86/x64 build:

- x86 output is a PE32/i386 DLL with the `.addon32` extension;
- x64 output is a PE32+/x86-64 DLL with the `.addon64` extension;
- both carry the committed REST version;
- both export the required REST add-on metadata;
- ReShade resource/resource-view handles remain 64-bit in both builds.

The D3D10/D3D11/D3D12 Auto Scene Colour code path itself does not contain architecture-specific branches; API behaviour is supplied by the corresponding ReShade backend. REST Release builds treat compiler warnings as errors on both architectures.

Legacy game-specific hooks may be architecture-specific. The FFXIV constant-copy hook is x64-only and is excluded from Win32 builds because its signatures target 64-bit game code.

## Creating a release

Create and push a tag from the desired `main` commit:

`v1.5.0.633`

The **Release** workflow will then:

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
7. publish a GitHub Release with generated release notes.

No release is published if either architecture fails to build.

## CI artifacts

Normal pushes to `main`, pull requests targeting `main`, and manual workflow runs build both architectures.

The resulting CI artifact is retained for 14 days and contains both add-on binaries. It is intended for validation, not long-term distribution.

## Rollback

If a release has a runtime regression:

1. do not reuse or move the existing tag;
2. fix the issue on `main`;
3. validate the fix through the normal MSBuild workflow;
4. publish a new patch tag.

Keeping release tags immutable makes binary/source provenance much easier to reason about.
