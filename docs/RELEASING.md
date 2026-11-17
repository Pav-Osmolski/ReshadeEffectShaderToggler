# Release and packaging process

## Maintained lines

| Line | Branch | Version suffix | Public title | Latest |
|---|---|---|---|---|
| Modern | `main` | `.680` | REST Enhanced vMAJOR.MINOR.PATCH.680 | Yes |
| Legacy | `legacy/reshade-5.x` | `.580` | REST Enhanced vMAJOR.MINOR.PATCH.580 (ReShade 5.x) | No |

Tags remain `vMAJOR.MINOR.PATCH.RESHADE`. The exact first line of the release merge commit must be **`Release vMAJOR.MINOR.PATCH.RESHADE`**. Public titles, archive names and commit titles are separate contracts. The manually maintained Testing Only pre-release keeps its own naming.

## Archive names and contents

The shared packager derives the release line from the source version suffix:

- `REST-Enhanced-Modern-v1.7.6.680.zip` (example)
- `REST-Enhanced-Legacy-v1.6.10.580.zip` (example)

These names apply to future packages from this change. Existing published `release.zip` assets remain unchanged. Do not move a tag or replace assets to rename historical releases.

The main archive contains both add-on binaries, helper shaders, licence/notices, README, changelog, docs and BUILD-INFO. The optional `BG3-Configs-D3D11+Vulkan.zip` is built from tracked config sources and has checksum/provenance files. `SHA256SUMS.txt` names and covers both ZIPs. Workflows consume the main archive name emitted by the packager, rather than duplicating naming logic.

## Before opening the final PR

1. Select the correct maintained branch and keep experimental work separate.
2. Set `src/version.h`, add the changelog entry and write `docs/releases/<tag>.md` with the correct public heading.
3. Update affected user guides. Preserve honest distinctions between source support, automated tests and game-tested results.
4. Build/test x86 and x64. Runtime-test representative configurations for changed behaviour, including relevant Auto/upscaler, hunting, toggle, resize and HDR cases. Record unrun cases explicitly.
5. Backport applicable changes to Legacy with its SDK-specific adaptations and separate validation.

Do not describe a pre-release or feature candidate as a stable published build. A successful CPU build does not establish HDR colour correctness or every game's compatibility.

## Review and merge

Open the PR when final or mark a draft ready. MSBuild runs on opened, reopened and ready-for-review events. Later pushes do not trigger synchronize builds; return a changed PR to draft/ready or reopen it for a fresh required check. Branch protection requires the successful current check and an up-to-date branch.

After the final PR check passes, merge with the exact `Release v...` title. The maintained-branch MSBuild job validates that merge result, both architectures, regressions and package metadata. Only after it succeeds does the guarded tag job validate the title/version/notes and create the tag at that commit.

The tagger explicitly dispatches the publisher because tags pushed with GITHUB_TOKEN do not trigger another workflow automatically. Ordinary merge titles do not create release tags. [Build commands](BUILDING.md).

## Publication and verification

The Release workflow validates the suffix, source version, notes and branch ancestry, stamps the version, rebuilds both architectures, reruns checks and packages the named archive. It publishes using the committed release notes. Modern is Latest; Legacy is not.

Existing tags must point to the same commit, and existing releases/assets are immutable. A retry must not overwrite files. After publication, verify the public title, stable/prerelease status, Latest designation, assets, checksums, PE architectures, versions and BUILD-INFO commit. Confirm source logic matches the tested candidate except for the intended version stamp.

The publisher requests a documentation refresh from `main` after releasing either line. Site-refresh failure does not replace or invalidate an already published archive; inspect the separate Pages workflow. See [website maintenance](website.md).

## Runtime validation scope

For Auto Scene Colour changes, test BG3 D3D11 separately from Vulkan. Compare internal/output resolutions, technique ordering and live updates with DLSS on/off where available. Verify included effects stay below the intended later UI/fog, settings persist, and resizing recovers. Vulkan tests should distinguish same-target LOAD, post-pass transition and intentionally rejected same-pass/unsupported targets.

For hunting/lifetime changes, exercise rapid Prev/Next, marked navigation, Rescan, Done, reopening settings, group removal and resolution/fullscreen resets. For HDR, record target encoding, effects and expected/actual colour rather than inferring correctness from a format name. See the [validation index](validation.md) and [HDR report matrix](HDR.md#report-an-hdr-problem).

## Workflow scope and history

One MSBuild workflow handles native/release validation. A separate lightweight documentation workflow builds the site and publishes only from `main`. Avoid adding duplicate relays or triggering native builds for every documentation revision. CI test artifacts expire after their configured retention; stable releases are the durable distribution.

Keep release tags and historical notes unchanged. Fix a released regression on a maintained branch, validate it and publish a new patch version. Record rollback instructions without reusing a tag.
