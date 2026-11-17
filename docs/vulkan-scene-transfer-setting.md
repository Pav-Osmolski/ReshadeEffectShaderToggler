# Save Vulkan scene-transfer support per game

Use this optional setting when a tested Vulkan setup needs eligible FP16 scene images to support REST's copies. It is available from **Modern v1.7.6.680 / Legacy v1.6.10.580** and defaults **off**.

## Enable it

1. Open **REST → Options → Advanced Vulkan options**.
2. Tick **Enable Vulkan scene transfers (requires restart)**.
3. Click REST's **Save changes** button.
4. Fully exit and restart the game. Reloading ReShade effects is not enough.
5. Confirm **Active this launch: Enabled**. Then test an appropriate scene group and check **Successful renders** increases.

The checkbox saves `VulkanSceneTransfers=True/False` under `[General]` in the game's `ReshadeEffectShaderToggler.ini`. It applies to that REST configuration, not one shader group. Resource Shim must be **none** for the optional path to run.

## Active versus pending

| UI state | What it means |
|---|---|
| Saved checkbox differs from Active this launch | Save and restart to apply the change |
| Active this launch: Enabled | The startup policy is enabled; a suitable target/boundary is still needed |
| Launcher override notice | The launch environment is controlling the policy instead of the saved preference |

Disabling also requires saving and restarting. REST freezes image-creation policy before registering callbacks; changes in the overlay do not retrofit or remove usage flags on existing images.

## Remove the old launcher setting

If your launcher contains a line setting `REST_VULKAN_SCENE_TRANSFERS`, **remove that line** to use UI control. Do not add an empty assignment as part of normal setup.

For the tested Enshrouded setup, keep `RESHADE_DISABLE_GRAPHICS_HOOK=1` and `--keep-vulkan-layers`. Those belong to ReShade/game startup and are separate from this REST preference. See the [complete Enshrouded guide](games/enshrouded.md).

## Diagnostic overrides

For targeted diagnosis, the process environment can still force `REST_VULKAN_SCENE_TRANSFERS=1` (on) or `=0` (off). Overrides take precedence without overwriting the saved preference. Absent or invalid values use the saved preference. The UI identifies an active override; remove it and restart to return to config control.

Startup logs record configured/active values and override status. For example, `configured=true, active=true, launcher override=false` confirms the saved setting was used.

## What this option cannot fix

It requests transfer usage only for eligible, originally sampled FP16 colour images. It does not make final HUD passes suitable for pre-UI injection, include an excluded technique or bypass Vulkan boundary checks. Native allocation properties are not fully exposed by ReShade; the option is not a universal compatibility fix. Read the [safety review](vulkan-transfer-safety.md).
