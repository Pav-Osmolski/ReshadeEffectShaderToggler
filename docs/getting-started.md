# Install REST and make your first group

You need ReShade with **full add-on support**, the REST build matching your ReShade line, and the binary matching the game's architecture.

## 1. Choose Modern or Legacy

| ReShade version | Download | Notes |
|---|---|---|
| 6.8 or newer | Modern | The GitHub **Latest** release; version suffix `.680` |
| 5.8–5.9.2 | Legacy / ReShade 5.x | Version suffix `.580`; ReShade 5.9.2 recommended |
| Older than 5.8 | Unsupported | Update ReShade before installing REST |

Open the [release list](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/releases). Future archives are named `REST-Enhanced-Modern-v<VERSION>.zip` or `REST-Enhanced-Legacy-v<VERSION>.zip`. Older releases use `release.zip`. Choose the archive attached to the correct release, not GitHub's automatic source-code downloads or a Testing Only pre-release.

Each main archive contains both architectures, helper shaders, documentation, licence notices and `BUILD-INFO.txt`. The optional BG3 configurations are a separate download. `SHA256SUMS.txt` covers the main and configuration ZIPs.

## 2. Install or upgrade

1. **Close the game.** Back up `ReshadeEffectShaderToggler.ini` and any customised REST helper shaders.
2. Find the executable ReShade actually loads into. A launcher may start a different executable, such as `GameName/Binaries/Win64/GameName-Win64-Shipping.exe`.
3. Copy **one** REST binary beside that executable:

   | Game process | File |
   |---|---|
   | 64-bit | `ReshadeEffectShaderToggler.addon64` |
   | 32-bit | `ReshadeEffectShaderToggler.addon32` |

4. Copy the package's `Shaders/REST_*.fx` files into a configured ReShade effect search folder, usually `reshade-shaders/Shaders`. Make sure ReShade's include paths can find `ReShade.fxh`.
5. Start the game and open ReShade. Under **Add-ons**, confirm **REST Enhanced — ReShade Effect Shader Toggler** appears. Reload effects if you changed helper files, and resolve compilation errors first.

Match the game's architecture, even on 64-bit Windows. Replace the previous REST binary rather than keeping duplicate versions. Helper techniques do not need to be enabled as ordinary global effects; REST invokes them when required.

Keep your working INI when upgrading. Changing REST line or renderer does not automatically require replacing the configuration. Game-specific shader hashes may still need revisiting.

## 3. Choose your workflow

### Place effects before UI or fog

Create a group, give it a useful name with **Edit**, open **Settings**, then **Start shader hunting**. Find an appropriate scene boundary, mark the shader and finish with **Done hunting**. Select your desired techniques under **Effects**, keeping them enabled in ReShade itself. Start with **Auto scene colour** on a supported backend.

Make sure the group includes your effects. In **Ticked techniques are EXCLUDED** mode, a tick prevents REST injecting that technique. See [effect placement](effect-placement.md).

### Hide the HUD for screenshots

Hunt and mark the pixel/vertex shaders drawing the interface, then enable **Hide marked shaders**. Test the group's Active checkbox or hotkey. Multiple shaders may draw different UI elements. See [UI control](ui-control.md).

Hiding UI and placing effects beneath UI are different tasks. The same marked shaders need not work for both, especially on Vulkan.

## 4. Verify and save

- Compare the group's Active state on and off while the scene is visible.
- For Auto Scene Colour, check **Successful renders** increases and **Last successful techniques** includes what you expect.
- Click **Save changes** to persist groups and preferences. Closing the overlay does not save automatically.

REST writes `ReshadeEffectShaderToggler.ini` beside the add-on. Saves use a verified temporary file and preserve the previous config as `.ini.bak`. See [configuration and recovery](groups-and-configuration.md).

## Where next?

[Shader hunting](shader-hunting.md) · [Upscaling and Auto Scene Colour](automatic-scene-colour.md) · [Game guides](game-guides.md) · [Troubleshooting](troubleshooting.md)
