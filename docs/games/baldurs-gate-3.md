# Baldur's Gate 3

BG3 is the primary D3D11 regression reference for Auto Scene Colour and has also been tested separately on Vulkan. Use the configuration matching the renderer you launch.

## Choose a renderer and config

| Game executable / mode | Configuration folder |
|---|---|
| `bg3_dx11.exe` / DirectX 11 | `D3D11` |
| Vulkan mode | `Vulkan` |

The optional release asset `BG3-Configs-D3D11+Vulkan.zip` contains both configurations, a README and original provenance. Back up your current INI before copying one: replacing the whole file is not a merge. To keep unrelated groups, use **Copy group / Import group** instead. Imports start inactive with no hotkey.

Use one REST build matching your installed ReShade line. Choose Modern for ReShade 6.8+, or Legacy for the supported 5.x line. Both main release archives include x86/x64 binaries; BG3 uses `addon64`.

## Verify effect placement

Open the desired group, check its Active state and technique mode, and keep all required techniques enabled in ReShade. With Auto Scene Colour, compare the scene and effect resolutions and check increasing successful renders.

With DLSS, a scene such as `2560×1440` at `3840×2160` output can use native staging. A multi-pass chain must keep its required order, for example `Lumenite_Kernel → Lumenite_LSAO`. Compare with the group off/on while moving the camera and opening UI.

The tested Vulkan Before Fog configuration used `0x782733c1` with a **post-pass RT transition**. Treat that hash as a configuration-specific example; game versions and graphics settings can change it. Vulkan's usable boundary is separate from D3D11's.

## Hiding UI

The supplied UI group can be used for shader suppression with **Hide marked shaders**. Review its settings and Active state before enabling it; supplied configs have historically kept that group disabled by default. UI hiding and pre-UI effect placement are separate workflows.

## Recorded coverage

The maintainer confirmed Modern and Legacy BG3 passes on both D3D11 and Vulkan for the November stable candidates. Earlier testing also included `renodx-dlss5.addon64`; that detail is not assumed for every later report. Exact HDR settings and individual substeps were not recorded in every report. See [validation records](../validation.md).

If a supplied hash is not seen, show the relevant scene, rescan and check renderer/settings before replacing marks. [Hunting guide](../shader-hunting.md) · [Auto Scene Colour](../automatic-scene-colour.md).
