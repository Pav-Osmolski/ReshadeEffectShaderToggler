# Build REST

## Requirements

- Windows, Visual Studio 2022, C++ v143 tools and a suitable Windows SDK.
- Git and the committed dependency revisions.
- Python 3.9+ for packaging/script tests; PyYAML 6.0.3 for workflow validation.
- For the documentation site, use its separate pinned environment; see [Website maintenance](website.md).

## Choose a branch and dependencies

```powershell
git clone https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler.git
cd ReshadeEffectShaderToggler
```

Modern builds from `main`. For Legacy, switch to `legacy/reshade-5.x` before fetching dependencies:

```powershell
git switch legacy/reshade-5.x
```

Then run `./scripts/fetch-dependencies.ps1`. It fetches the pinned submodule commits shallowly and in parallel. Do not advance the ReShade SDK to upstream HEAD as a routine build step.

## Build x64 and x86

Open `src/ReshadeEffectShaderToggler.sln`, or run in a VS2022 Developer PowerShell:

```powershell
msbuild /m /p:Configuration=Release /p:Platform=x64 src/ReshadeEffectShaderToggler.sln
msbuild /m /p:Configuration=Release /p:Platform=x86 src/ReshadeEffectShaderToggler.sln
```

The x86 solution configuration maps to Win32. Outputs are `src/x64/Release/ReshadeEffectShaderToggler.addon64` and `src/Win32/Release/ReshadeEffectShaderToggler.addon32`.

## Run checks

```powershell
python -m pip install PyYAML==6.0.3
python scripts/validate-workflows.py
./scripts/validate-powershell.ps1
python -m unittest discover -s tests -p 'test_*.py'
./scripts/test-rendering-safety.ps1
./scripts/validate-binaries.ps1
```

Binary validation needs both builds. It verifies architecture, version and exports. CPU/native regressions do not replace game/GPU testing. VS2019 cannot compile the production C++20 format paths used by the full checks; do not substitute compatibility shims and call that a production build.

## Package without publishing

```powershell
python scripts/package-build.py --kind test
```

This creates a test archive, BG3 configs and checksums under `test-package/`. The test name includes source version and commit identity. `--kind release` instead produces `REST-Enhanced-Modern-v<VERSION>.zip` or `REST-Enhanced-Legacy-v<VERSION>.zip` in the repository root. It does not create a tag or publish anything.

`scripts/package-manifest.json` is the shared payload list. Both architectures, shaders, README, changelog, docs and BUILD-INFO are included. `SHA256SUMS.txt` covers the main archive and optional BG3 configuration ZIP. Use a clean committed tree when relying on BUILD-INFO provenance.

## CI scope

MSBuild validates final PR review events and maintained-branch pushes. Keep revisions in draft, then mark ready for review when final; a changed ready PR needs a fresh ready/reopen event because synchronize does not trigger another run. Manual dispatch is available for branch test builds.

Known documentation/site-only edits skip native compilation in MSBuild but still run workflow/script checks. The site has its own build/link checks. Release-titled promotions always build both architectures. [Exact release procedure](RELEASING.md).
