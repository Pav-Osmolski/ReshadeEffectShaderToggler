# Building and packaging

[Documentation index](README.md) · [Release process](RELEASING.md)

## Requirements

- Windows with Visual Studio 2022, the C++ v143 toolset and a suitable Windows SDK.
- Git, including initialized submodules at the committed revisions.
- Python 3.9+ for the shared packager and script tests. Workflow validation additionally uses PyYAML 6.0.3.

Modern and Legacy use different pinned ReShade SDKs. Check out the intended maintenance branch before building; do not update its ReShade submodule to upstream HEAD as a routine build step.

## Build both architectures

Clone this repository, enter it, then initialize pinned dependencies:

```powershell
git clone https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler.git
cd ReshadeEffectShaderToggler
./scripts/fetch-dependencies.ps1
```

For Legacy, check out `legacy/reshade-5.x` before running the dependency helper. It uses shallow parallel fetches; full repository history is retained for CI diffs and release ancestry checks.

Open `src/ReshadeEffectShaderToggler.sln` in Visual Studio, or run these commands from a Visual Studio 2022 Developer PowerShell:

```powershell
msbuild /m /p:Configuration=Release /p:Platform=x64 src/ReshadeEffectShaderToggler.sln
msbuild /m /p:Configuration=Release /p:Platform=x86 src/ReshadeEffectShaderToggler.sln
```

The x86 solution configuration maps to Win32. Outputs are `src/x64/Release/ReshadeEffectShaderToggler.addon64` and `src/Win32/Release/ReshadeEffectShaderToggler.addon32`.

## Checks

```powershell
python -m pip install PyYAML==6.0.3
python scripts/validate-workflows.py
./scripts/validate-powershell.ps1
python -m unittest discover -s tests -p 'test_*.py'
./scripts/test-rendering-safety.ps1
./scripts/validate-binaries.ps1
```

Binary validation requires both completed Release builds and checks their architecture, source version and required exports. Rendering safety tests are CPU regressions, not GPU/game tests.

## Package without publishing

```powershell
python scripts/package-build.py --kind test
```

This writes the test candidate, BG3 configs and checksums under `test-package/`. The same packager with `--kind release` writes `release.zip` and the configuration/checksum/provenance assets to the repository root. Packaging locally does not create a tag or publish anything. Use a clean, committed source tree when relying on BUILD-INFO's Git identity.

The file lists come from `scripts/package-manifest.json`; configuration sources and original provenance live in `configs/BG3/`. Both main archives contain the README, changelog, shaders, docs and BUILD-INFO. `SHA256SUMS.txt` covers the main archive and the BG3 config archive. Do not edit duplicate file lists in workflow YAML.

## CI policy

Keep WIP PRs in draft. Once final, wait until GitHub shows the current head/merge revision, then mark ready for review. Checks run on opened, reopened and ready-for-review events; ordinary synchronize events do not start a new build. A newer commit still needs a new successful required check before merging. Current branch protection requires `build` and an up-to-date branch.

Documentation-only PRs report that check without native compilation. Native/build/config changes validate x86 and x64. Maintained-branch merges validate again. A release-titled merge starts the tag job only after its branch build succeeds; publication is a separate guarded workflow. See [Release process](RELEASING.md) for exact titles, version suffixes and immutable tag/assets rules.
