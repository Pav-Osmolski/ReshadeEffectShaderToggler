# Changelog

## v1.7.0.680 — 2026-09-24

### ReShade 6.8 / API 20 modernization

- Establishes ReShade **6.8+** as the active REST Enhanced development line.
- Pins the ReShade dependency to the ReShade 6.8.0 / API 20 baseline.
- Updates add-on metadata and callback signatures for the current ReShade API.
- Updates swapchain lifecycle callbacks to the API 20 interface.
- Updates render-pass callbacks to API 20 and uses Vulkan render-pass suspend/resume flags to avoid treating resumed dynamic-rendering segments as safe injection boundaries.
- Retains the proven conservative Vulkan subpass fallback for traditional render-pass transitions.
- Keeps the ReShade 5.x compatibility implementation isolated on `legacy/reshade-5.x`.
- Builds and validates x64 and x86, including PE architecture, file version and required add-on exports.

### Runtime validation

Runtime-tested successfully with **ReShade 6.8.0** using Baldur's Gate 3. The tested v1.7.0.680 workflow behaved correctly with no reported regression.

Improvements, modernization, documentation and testing by **DeViLhoOD**, building on the upstream contributors credited in README.md.



## v1.6.3.633 — 2026-09-24

### Shader-hunting visibility

- Shows saved/marked shader hashes that were not observed during the latest collection pass as **red** entries instead of hiding them.
- Keeps collected + marked shaders yellow and collected + unmarked shaders in the normal text colour.
- Adds `collected | marked | not seen` counts to make the current collection state explicit.
- Includes not-seen hashes in the **Marked** filter and search while keeping **Unmarked** limited to collected unmarked shaders.
- Adds an explanatory red legend and tooltip noting that a not-seen shader may be scene/state dependent rather than invalid.
- Allows individual red/not-seen hashes to be removed by double-clicking them.

### Navigation safety

- Keeps not-seen hashes display-only: they are not added to the collected shader set and do not participate in Prev/Next, marked navigation or preview selection.
- Preserves the stable first-seen order for all actually collected shaders.

Runtime-tested successfully with a BG3 D3D11 UI group containing saved shaders absent from the current collection pass.

No rendering, preview-resource, Auto Scene Colour, Direct3D or Vulkan execution paths are changed by this release.

Improvements, documentation and testing by **DeViLhoOD**, building on the upstream contributors credited in README.md.


## v1.6.2.633 — 2026-09-24

### D3D shader-hunting stability

- Fixes a reproducible D3D11 crash-to-desktop when holding **Prev** or rapidly navigating shaders during shader hunting.
- Restores typed SRV/RTV creation over typeless Direct3D preview resources after the Vulkan preview-resource refactor caused D3D11 shader-resource-view creation to fail.
- Guards preview compatibility checks against missing SRV handles so failed preview-view creation cannot lead to a null backend view query on the next hunting step.
- Falls back to the captured resource format when the source view format is unknown.
- The corrected preview path is shared by D3D10, D3D11 and D3D12. BG3 D3D11 was runtime-tested successfully after the fix; D3D10/D3D12 were not separately game-tested.

### CI and maintenance

- Makes MSBuild validation lifecycle-based rather than commit-based: PRs validate when opened, reopened or marked ready, while build-affecting changes merged to `main` still validate normally.
- Keeps manual workflow dispatch for deliberate WIP/test builds.
- Removes the obsolete branch-specific Vulkan per-push test workflow, avoiding redundant Actions runs during development.

Vulkan preview capture and Auto Scene Colour behaviour remain unchanged.

Improvements, documentation and testing by **DeViLhoOD**, building on the upstream contributors credited in README.md.


## v1.6.1.633 — 2026-09-21

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

- Flushes ReShade's immediate command list and synchronizes the Vulkan graphics queue before replacing preview images, preventing rapid shader-hunting preview churn from destroying resources still referenced by recorded or in-flight GPU work.

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
