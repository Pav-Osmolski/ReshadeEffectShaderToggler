# ReshadeEffectShaderToggler

[![MSBuild](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/actions/workflows/msbuild.yml/badge.svg)](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/actions/workflows/msbuild.yml)
[![Release](https://img.shields.io/github/v/release/Pav-Osmolski/ReshadeEffectShaderToggler?label=release)](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/releases/latest)

A ReShade 5.8+ add-on for applying ReShade effects at specific points inside a game's rendering pipeline. REST groups user-selected shaders and can inject selected ReShade techniques immediately before those shaders are encountered.

The primary build target is 64-bit. A 32-bit build is also produced, but it is not actively tested.

## Highlights

- Create shader groups and toggle them on or off from the ReShade overlay.
- Apply all globally enabled ReShade techniques, a selected subset, or all except selected techniques to a group.
- Render effects at configurable render-target boundaries.
- Preview and inspect render targets while hunting shaders.
- Extract and reuse constant-buffer or texture-binding data where supported.
- **Automatic scene-colour injection for D3D12 games using DLSS or other dynamic-resolution/upscaling paths.**
- Preserve technique selections reliably across ReShade effect reloads and ordering changes.

## Compatibility

REST requires a ReShade build with add-on support enabled.

The existing render-target, shader-hunting and binding features remain API/game dependent. D3D12 and Vulkan behaviour outside the paths that have been specifically tested may vary by title.

The **Auto scene colour** path is currently a D3D12 feature. It has been developed and tested with **Baldur's Gate 3 using DLSS**, where the scene is rendered below the swapchain resolution and later composed with fog and UI.

## Installation

Place the appropriate add-on beside the game executable that ReShade is actually injected into:

- 64-bit: `ReshadeEffectShaderToggler.addon64`
- 32-bit: `ReshadeEffectShaderToggler.addon32`

For Unreal Engine games this is often the executable under a path such as:

`GameName\Binaries\Win64\GameName-Win64-Shipping.exe`

ReShade must be installed against that executable as well.

Start the game and open the ReShade overlay. The **Add-ons** tab should list **Reshade Effect Shader Toggler**.

## Basic workflow

1. Open the ReShade overlay and expand **Reshade Effect Shader Toggler**.
2. Click **New** to create a toggle group.
3. Click **Edit** to give the group a useful name and optional hotkey.
4. Use **Change Shaders** to hunt and mark the shader(s) that define where the group should run.
5. Use **Change Effects** to select which ReShade techniques the group should apply.
6. Test the group, then click **Save all Toggle Groups**.

The saved configuration is written to `ReshadeEffectShaderToggler.ini` beside the add-on.

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

## Automatic scene colour for D3D12 upscalers

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

Auto mode intentionally ignores stale manual render-target index, SRV slot/binding, swapchain-match and alpha-preservation settings. You do not need to choose a shader stage, SRV slot or descriptor binding.

The group editor reports the live **scene resolution**, **effect resolution**, technique order and render status to make validation easier.

For setup details, limitations and troubleshooting, see [Automatic Scene Colour](docs/automatic-scene-colour.md).

## Marking shaders

Make the element that defines your desired injection boundary visible before starting shader hunting. A debug-heavy effect such as AO can make it easier to see whether a UI, fog or other game pass is being drawn before or after the current shader.

Click **Change Shaders**. REST first collects active shaders for the configured number of frames, then lets you browse them.

Default controls:

- `Numpad 1` / `Numpad 2`: previous/next pixel shader.
- `Numpad 3`: add/remove the current pixel shader from the group.
- `Ctrl + Numpad 1` / `Ctrl + Numpad 2`: browse shaders already marked in the current group.
- `Numpad 4` / `Numpad 5`: previous/next vertex shader.
- `Numpad 6`: add/remove the current vertex shader from the group.
- `Ctrl + Numpad 4` / `Ctrl + Numpad 5`: browse marked vertex shaders.

Use the group's **Active** checkbox or assigned hotkey while testing. When finished, click **Done** and save the groups.

## Automatic scene-colour performance note

When native staging is required, REST performs an additional scene copy to native resolution and another copy back to the live scene target, and the selected ReShade techniques run at native resolution. This is intentional for correctness but can cost more GPU time than rendering directly at the game's internal resolution.

## Building

The repository builds x64 and x86 Release configurations through GitHub Actions and Visual Studio/MSBuild.

A normal pull request to `main` runs the full build. Tagged releases use the format:

`vMAJOR.MINOR.PATCH.RESHADE`

For example:

`v1.4.0.633`

See [Release Process](docs/RELEASING.md) for the release checklist and packaging details.

## Credits

- [alex / 4lex4nder](https://github.com/4lex4nder) - ReshadeEffectShaderToggler development.
- **DeViLhoOD** - Automatic Scene Colour, DLSS/upscaled rendering support, release hardening, documentation and testing.
- [Frans Bouma](https://github.com/FransBouma) - original ShaderToggler.
- [Sinom](https://github.com/sinomsinom) - contributor.
- [crosire](https://github.com/crosire) - ReShade and effect-rendering examples.
- [Marty McFly](https://github.com/martymcmodding) - constant-buffer extraction idea.
- [darkarchan](https://github.com/darkarchan) - testing and contributions.

## Licence

This project retains the licence and notices from the upstream project. See [LICENSE](LICENSE).
