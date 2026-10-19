# REST Enhanced documentation

Start with [Getting started](getting-started.md) for installation, upgrading and the first shader group.

| Task | Guide |
|---|---|
| Install REST, choose Modern/Legacy or use the BG3 configs | [Getting started](getting-started.md) |
| Inject effects before later fog/UI or work with DLSS/upscaling | [Automatic Scene Colour](automatic-scene-colour.md) |
| Understand HDR formats, preview colour and tone-map clamping | [HDR handling and validation](HDR.md) |
| Diagnose missing effects, configuration problems or crashes | [Troubleshooting](troubleshooting.md) |
| Build or package locally | [Building](BUILDING.md) |
| Maintain CI or publish a release | [Release process](RELEASING.md) |
| Review changes and historical release notes | [Changelog](../CHANGELOG.md) and [release-note archive](releases/) |

Modern uses `main` and the ReShade 6.8/API 20 baseline. Legacy uses `legacy/reshade-5.x` and the ReShade 5.8/API 7 baseline; ReShade 5.9.2 is recommended for that line. A feature's presence in the code or a successful x86/x64 build does not establish runtime coverage for every game, renderer or HDR setup.
