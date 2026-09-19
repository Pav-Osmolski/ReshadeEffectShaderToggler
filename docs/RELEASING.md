# Release Process

This fork uses GitHub Actions for validation and tagged releases.

## Version format

Release tags must use:

`vMAJOR.MINOR.PATCH.RESHADE`

Example:

`v1.4.0.633`

The final component preserves the upstream convention used for the ReShade baseline.

The source tree contains a human-readable default version in `src/version.h`. During a tagged release, the workflow validates the tag and runs `src/set_release_version.ps1` so the packaged binaries are stamped from the tag itself.

## Pre-release checklist

Before tagging a release:

1. Confirm the intended changes are merged into `main`.
2. Confirm the **MSBuild** workflow is green for the merge commit.
3. Confirm both **x64** and **x86** Release configurations build successfully.
4. Check that `README.md` and files under `docs/` describe any user-visible changes.
5. Confirm the default version in `src/version.h` matches the release you intend to tag.
6. Test the x64 add-on in at least one representative configuration for the release's main feature.
7. For Automatic Scene Colour changes, verify:
   - the effect updates every frame;
   - scene and effect resolutions are reported correctly;
   - multi-pass techniques run in the expected order;
   - the effect stays below later UI/fog passes;
   - enabling/disabling DLSS or changing resolution does not require manual SRV selection.

## Creating a release

Create and push a tag from the desired `main` commit:

`v1.4.0.633`

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
