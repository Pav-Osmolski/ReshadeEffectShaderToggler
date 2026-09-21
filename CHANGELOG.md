# Changelog

## Unreleased

### QoL and configuration safety

- Uses one stable first-seen shader order for the hunting list, Prev/Next, marked navigation and direct selection; the two-column view now runs top-to-bottom down the left column before continuing in the right column.

- Fixes hash-list clicks selecting a different shader when the responsive two-column view is active by resolving UI selections directly by hash rather than copied unordered-set index.

- Keeps committed shader hashes intact while hunting so finishing an unchanged hunt is a true no-op and does not falsely mark the configuration dirty.

- Saves configuration through a verified temporary file, refreshes `ReshadeEffectShaderToggler.ini.bak` and atomically replaces the live INI; adds `ConfigVersion` for future migrations.
- Replaces per-overlay full configuration signature generation with cached dirty-state tracking.
- Adds clipboard **Copy group / Import group** using REST's existing INI group serializer.
- Adds press-and-hold shader browsing, **Mark + Prev / Mark + Next** controls and optional shortcuts, **Copy hash**, and retained hunting search/filter/stage/channel/pane-width state for the current session.
- Makes the shader-hunting workspace responsive with a 540 px preferred pane, a wider default Group Settings window, structured two-row controls and an adaptive two-column hash list for large filtered result sets.
- Adds a bounded **Recent candidates** Auto Scene Colour history that groups repeated status transitions by shader/target candidate and tracks successful render counts.

### Stability

- Synchronizes Vulkan preview resource replacement with the graphics queue before destroying old preview images, preventing rapid shader-hunting preview churn from invalidating in-flight GPU work and causing device loss.

### Optimisation

- Returns committed shader-hash sets by const reference where snapshots are not required.
- Caches sorted technique-picker metadata on ReShade effect reload/reorder instead of rebuilding and sorting it every overlay frame.
- Avoids copying the selected-technique set every overlay frame; it is copied only when the user actually changes a checkbox.
- Adds atomic Vulkan pending-work flags so render-pass tracking remains intact while deferred Auto Scene Colour and preview matching are skipped when there is no pending work.


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
