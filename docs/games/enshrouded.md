# Enshrouded: scene effects with sharp UI

The tested setup uses Vulkan, a suitable earlier scene shader and optional scene-transfer support. Simply marking the final HUD shaders is too late for pre-UI effect injection, although those shaders can still be useful for hiding UI.

## Launch with ReShade

Install the appropriate Modern/Legacy `addon64` with the game closed. Save this launcher as `Launch-Enshrouded-REST.cmd` beside `enshrouded.exe`:

```bat
@echo off
setlocal
set "RESHADE_DISABLE_GRAPHICS_HOOK=1"
"%~dp0enshrouded.exe" --keep-vulkan-layers
endlocal
```

`--keep-vulkan-layers` retains ReShade's Vulkan layer. The graphics-hook variable worked around a separate shutdown crash in the local test setup; it is not a Windows-wide recommendation or proof that every shutdown failure has the same cause. These settings apply to this launcher process and its child game.

Remove any old line setting `REST_VULKAN_SCENE_TRANSFERS` if you want the saved REST UI setting to control it.

## Enable scene transfers once

1. Open **REST → Options → Advanced Vulkan options**.
2. Enable **Vulkan scene transfers**, click **Save changes**, then fully restart the game.
3. Confirm **Active this launch: Enabled** with no launcher-override notice.
4. Leave **Resource Shim** set to **none**.

This setting is available from Modern v1.7.6.680 / Legacy v1.6.10.580. It defaults off and only requests usage on eligible images. [Setting and restart details](../vulkan-scene-transfer-setting.md).

## Configure the scene group

1. Use **Auto scene colour** on and **Hide marked shaders** off.
2. Hunt an **earlier scene shader**. The tested hash was `0x46c3d744` on build 1287369. Finish with **Done hunting**.
3. Include the effect you want. If the list says **Ticked techniques are EXCLUDED**, untick your desired effect to allow injection. Keep it globally enabled in ReShade.
4. Activate the group and confirm **Successful renders** increases. Compare the group off/on; later UI should remain sharp while the scene effect stays visible.
5. Save the configuration. Disable competing groups while comparing.

In the inspected configuration, three old HUD hashes and an excluded CinematicDOF caused the apparent failure even though the saved scene-transfer preference was active. A copied group using the earlier scene hash and allowing CinematicDOF restored successful injection. The original group was retained for comparison.

## What the tests establish

Both Modern and Legacy local passes were confirmed. Detailed Modern testing covered SDR/HDR placement, 100%/67% game render scale with DLSS Quality, group/Auto toggles, hunting, a windowed/fullscreen round-trip and clean exit. A later UI-setting check confirmed startup activation without a launcher override and successful direct 3840×2160 injection in the title-screen scene.

CinematicDOF was used for the detailed scene/UI comparison. This does not separately certify PerfectPerspectiveDual or every HDR effect. HDR colour accuracy was not measured, all game areas were not traversed, and an existing depth-stencil-in-use warning remained. [Full recorded coverage](../validation.md).

## If it still waits

Check the active scene-transfer state, Resource Shim, shader hash, technique mode and diagnostic target. A 0-render counter with a final HUD target still means no eligible injection. Do not disable boundary guards or expect Auto off to bypass Vulkan restrictions. See [troubleshooting](../troubleshooting.md).
