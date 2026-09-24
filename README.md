# REST Enhanced — ReShade Effect Shader Toggler

> **Legacy ReShade 5.x maintenance branch.** This branch is retained for ReShade 5.8–5.9.2 users. New feature development targets ReShade 6.8+ on `main`. ReShade 5.9.2 is the recommended runtime for this legacy line; versions older than 5.8 are unsupported.

[![MSBuild](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/actions/workflows/msbuild.yml/badge.svg)](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/actions/workflows/msbuild.yml)
[![Release](https://img.shields.io/github/v/release/Pav-Osmolski/ReshadeEffectShaderToggler?label=release)](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/releases/latest)

A ReShade 5.8+ add-on for applying ReShade effects at specific points inside a game's rendering pipeline. REST groups user-selected shaders and can inject selected ReShade techniques immediately before those shaders are encountered.

Both 64-bit and 32-bit are first-class build targets. CI builds and validates both architectures, including PE machine type, version metadata and required add-on exports. Auto Scene Colour supports D3D10/D3D11/D3D12 and Vulkan through ReShade's generic API, with a Vulkan-specific native-staging blit path. Legacy game-specific hooks may remain architecture-specific; the FFXIV constant-copy hook is x64-only because its signatures target 64-bit game code.

## Highlights

- Create shader groups and toggle them on or off from the ReShade overlay.
- Apply all globally enabled ReShade techniques, a selected subset, or all except selected techniques to a group.
- Render effects at configurable render-target boundaries.
- Preview and inspect render targets while hunting shaders.
- Extract and reuse constant-buffer or texture-binding data where supported.
- **Automatic scene-colour injection for D3D10/D3D11/D3D12 and Vulkan games using DLSS or other dynamic-resolution/upscaling paths.**
- Preserve technique selections reliably across ReShade effect reloads and ordering changes.
- Search, filter and recollect shaders with mouse controls or configurable keyboard shortcuts.
- Track unsaved configuration changes, clone/import/export groups safely, confirm deletions and flag shortcut conflicts.
- Use crash-safe configuration saves with an automatic `ReshadeEffectShaderToggler.ini.bak` backup.

## Compatibility

REST requires a ReShade build with add-on support enabled.

The existing render-target, shader-hunting and binding features remain API/game dependent. D3D10/D3D11/D3D12 and Vulkan behaviour outside the paths that have been specifically tested may vary by title.

The **Auto scene colour** path supports D3D10, D3D11, D3D12 and Vulkan on both x86 and x64. D3D10/11/12 retain the existing shader-based native-staging copy path. Vulkan defers injection to a proven safe boundary: a new pass that **LOADs the same colour target**, or a **post-pass transition of that exact target out of render-target usage**. Subpass transitions are not treated as completed render passes. REST restores the game's requested resource state and guards against recursive injection.

The live Vulkan image must already have transfer-source usage; native staging additionally requires transfer-destination usage and uses image blits. REST does not add transfer flags to arbitrary game images or split active game passes. Unsupported targets or boundaries are skipped safely. Baldur's Gate 3 Vulkan testing confirmed stable Before Fog injection with `2560×1440 → 3840×2160` staging and rapid shader navigation after the hunting fixes. BG3 DX11 + DLSS remains the D3D regression reference; this does not imply every title or architecture has been runtime-tested.

See the [changelog](CHANGELOG.md) for the changes in **v1.6.0.633**.

## Installation

Place the appropriate add-on beside the game executable that ReShade is actually injected into:

- 64-bit: `ReshadeEffectShaderToggler.addon64`
- 32-bit: `ReshadeEffectShaderToggler.addon32`

For Unreal Engine games this is often the executable under a path such as:

`GameName\Binaries\Win64\GameName-Win64-Shipping.exe`

ReShade must be installed against that executable as well.

Start the game and open the ReShade overlay. The **Add-ons** tab should list **REST Enhanced — ReShade Effect Shader Toggler**.

## Basic workflow

1. Open the ReShade overlay and expand **REST Enhanced — ReShade Effect Shader Toggler**.
2. Click **New group** to create a toggle group.
3. Click **Edit** to give the group a useful name and optional hotkey.
4. Click **Settings** and keep the relevant scene visible while REST collects active shaders.
5. Use the shader search/filter controls, mouse navigation buttons or configured hunting shortcuts to locate and mark the shader(s) that define the desired boundary.
6. In the **Effects** tab, select the ReShade techniques the group should apply and enable **Auto scene colour** when appropriate.
7. Click **Done**, test the group, then click **Save changes** when the unsaved-changes indicator is shown.

The saved configuration is written to `ReshadeEffectShaderToggler.ini` beside the add-on. Saves are first written to a verified temporary file, the previous configuration is refreshed as `ReshadeEffectShaderToggler.ini.bak`, and the live INI is then replaced atomically. REST also persists the shader-collection frame count, overlay opacity and configurable hunting shortcuts.

### Group management

- **Clone** copies a group's shader hashes, effects and settings into a new inactive group with no hotkey, so it can be adjusted safely.
- **Copy group** places a self-contained REST group INI block on the clipboard; **Import group** restores one without replacing the rest of the configuration. Imported groups start inactive with no hotkey to avoid unexpected rendering changes or shortcut conflicts.
- **Delete** requires confirmation and is not written to disk until **Save changes** is used.
- Each group shows compact pixel/vertex/compute shader counts, selected-effect count and an **Auto Scene Colour** indicator when enabled.
- REST warns when group hotkeys conflict with another group or with a configured REST action.
- The **Saved / Unsaved changes** indicator reflects the configuration that would be written to disk, including the group's persisted Active state.

## Configuring effects

Each group can control a subset of the techniques currently known to ReShade. Techniques are executed in their global ReShade order.

REST supports three modes:

- **All globally enabled techniques are applied**: no technique filter is used.
- **Only ticked enabled techniques are applied**: only selected techniques that are globally enabled in ReShade are run.
- **Ticked techniques are EXCLUDED**: all globally enabled techniques except the selected entries are run.

Important behaviour:

- A technique enabled in the group but disabled globally in ReShade will not run.
- If the same technique is assigned to multiple active groups, it is normally rendered by the first applicable group encountered in the frame.
- Multi-pass effects must keep their required techniques enabled and in the correct ReShade order.
- Technique selections are stored by name and preserved when ReShade reloads or reorders its effect list.

## Automatic scene colour for D3D10/D3D11/D3D12 and Vulkan upscalers

For games that render the scene below output resolution and upscale later, rendering a ReShade effect directly into the lower-resolution scene target can break multi-pass effects or produce incorrectly scaled output.

Enable **Auto scene colour** in the group's render-target settings to use REST's automatic path.

In this mode REST:

1. Uses the **primary live colour render target** bound at the matched draw.
2. Matches the target by **aspect ratio**, so DLSS/dynamic-resolution changes do not require manual width/height configuration.
3. If the live scene is below the ReShade runtime/output resolution, copies it into a native-resolution staging target.
4. Runs the selected ReShade techniques at the native ReShade resolution.
5. Copies the completed result back into the live scene target.
6. Returns control to the game so later fog, post-processing and UI passes are composed normally.

This is why effects such as AO can remain **under the UI** while still using their normal full-resolution intermediate textures.

Auto mode intentionally ignores manual render-target index, SRV slot/binding, swapchain-match and alpha-preservation settings while it is active. Those manual settings are preserved unchanged underneath Auto mode and become effective again when Auto is disabled. You do not need to choose a shader stage, SRV slot or descriptor binding.

The group editor reports the live **scene resolution**, **effect resolution**, technique order, injection status and native-staging state, and keeps the eight most recent distinct Auto Scene Colour attempts in an expandable diagnostic history. **Copy diagnostics** places the relevant REST version, API, resolutions, technique information, render-call count and target handle on the clipboard for support reports.

For setup details, limitations and troubleshooting, see [Automatic Scene Colour](docs/automatic-scene-colour.md).

## Marking shaders

Make the element that defines your desired injection boundary visible before starting shader hunting. A debug-heavy effect such as AO can make it easier to see whether a UI, fog or other game pass is being drawn before or after the current shader.

Click **Settings** on the group, then **Start shader hunting**. REST first collects active shaders for the configured number of frames, then lets you browse them. The shader pane provides:

- case-insensitive hash search;
- **All / Marked / Unmarked** filtering;
- **Prev**, **Next**, **Prev marked**, **Mark / unmark** and **Next marked** mouse controls, with press-and-hold repeat for navigation;
- **Mark + Prev / Mark + Next** controls and optional shortcuts for rapidly classifying candidates;
- **Copy hash** for the currently selected candidate;
- collected and marked shader counts plus **Clear marked** for the current shader stage;
- **Rescan**, which starts a fresh collection for pixel, vertex and compute shaders while preserving the current marked hashes;
- a render-target preview below the settings pane. On Vulkan the hunted draw is still suppressed safely, while the preview copy is deferred to a legal render-pass boundary rather than copied from inside the active draw pass. The preview reports the selected hash, shader stage, target dimensions/format and an explicit reason when the image cannot be copied safely.

The traditional defaults remain available for pixel and vertex shader hunting:

- `Numpad 1` / `Numpad 2`: previous/next pixel shader.
- `Numpad 3`: add/remove the current pixel shader from the group.
- `Ctrl + Numpad 1` / `Ctrl + Numpad 2`: browse marked pixel shaders.
- `Numpad 4` / `Numpad 5`: previous/next vertex shader.
- `Numpad 6`: add/remove the current vertex shader from the group.
- `Ctrl + Numpad 4` / `Ctrl + Numpad 5`: browse marked vertex shaders.

All hunting shortcuts are configurable under **Shader hunting keybindings**, including optional Mark + Previous/Next bindings, making shader hunting practical on laptops and compact keyboards. The hunting pane is responsive with a 540 px preferred width, uses a second hash column automatically for large filtered result sets when space permits, and retains its width together with search text, filter mode, selected shader stage and preview channel while the current REST session remains open. Compute-shader hunting is also configurable but deliberately has no default shortcut. Shortcut matching uses the exact configured Ctrl/Shift/Alt modifiers, so a plain key does not also fire when a modified version is pressed.

Use the group's **Active** checkbox or assigned hotkey while testing. When finished, click **Done** and **Save changes**.

Rapid **Prev / Next** navigation uses owned shader-group snapshots, synchronised shader maps and collected lists, and atomic render-visible hunting state. Vulkan snapshots the selected shader for each draw. These fixes address the rapid-navigation crash without adding a button delay. Preview **RGB / R / G / B** controls help inspect individual channels; Vulkan previews wait for a safe copy boundary and do not support Clear alpha processing.

## Automatic scene-colour performance note

When native staging is required, REST performs an additional scene copy to native resolution and another copy back to the live scene target, and the selected ReShade techniques run at native resolution. This is intentional for correctness but can cost more GPU time than rendering directly at the game's internal resolution.

## Building

The repository builds x64 and x86 Release configurations through GitHub Actions and Visual Studio/MSBuild.

A normal pull request to `legacy/reshade-5.x` runs the full legacy build. Tagged releases use the format:

`vMAJOR.MINOR.PATCH.RESHADE`

For example:

`v1.6.0.633`

See [Release Process](docs/RELEASING.md) for the release checklist and packaging details.

## Credits

- [alex / 4lex4nder](https://github.com/4lex4nder) - ReShade Effect Shader Toggler development.
- **DeViLhoOD** - Automatic Scene Colour, Vulkan safe-boundary injection and previews, shader-hunting stability improvements, DLSS/upscaled rendering support, x86/x64 hardening, QoL workflow improvements, documentation and testing.
- [Frans Bouma](https://github.com/FransBouma) - original ShaderToggler.
- [Sinom](https://github.com/sinomsinom) - contributor.
- [crosire](https://github.com/crosire) - ReShade and effect-rendering examples.
- [Marty McFly](https://github.com/martymcmodding) - constant-buffer extraction idea.
- [darkarchan](https://github.com/darkarchan) - testing and contributions.

## Licence

This project retains the licence and notices from the upstream project. See [LICENSE](LICENSE).
