# Changelog

## v1.6.0.633 — 2026-09-20

### Vulkan Auto Scene Colour

- Adds Vulkan support on x86 and x64, including native-resolution image-blit staging for upscaled scene targets.
- Defers injection to a proven same-target LOAD pass or a post-pass transition of the exact target out of render-target usage.
- Rejects ambiguous subpass boundaries, restores the game's requested resource state, guards against recursive injection and clears stale pending work at present/effect reload.
- Requires existing Vulkan transfer usage and eligible formats/sample counts instead of modifying arbitrary game images.
- Separates current target attempts from the last successful injection and reports the actual Vulkan boundary, staging path, technique order and successful-render count.

### Shader hunting

- Fixes the rapid Prev/Next navigation race with owned command-list shader-group snapshots, locked shader-group/pipeline maps, stable collected-shader snapshots and atomic render-visible hunting state.
- Snapshots hunted hashes per Vulkan draw, suppresses hunted calls safely and clears work for all stages in a skipped draw.
- Defers Vulkan preview copies to safe pass boundaries, restores tracked resource usage and cancels stale preview work.
- Adds explicit preview status, hash/stage/format metadata and RGB/R/G/B channel viewing.
- Keeps group settings open after hunting and clarifies pending marks and Clear marked.

### Validation and compatibility

- User-confirmed BG3 Vulkan Before Fog injection at `0x782733c1`, with `2560×1440 → 3840×2160` staging, no observed flicker and stable rapid shader navigation.
- Retains the D3D10/11/12 shader-copy staging path and established binary/INI filenames.
- Release builds validate x86/x64 architecture, version metadata and required add-on exports. Runtime coverage remains configuration-specific; not every API/title/architecture combination was tested.

Improvements, documentation and testing by **DeViLhoOD**, building on the upstream contributors credited in README.md.

Earlier release notes are available in [GitHub Releases](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/releases).
