# Saved Vulkan scene-transfer option

Available from Modern v1.7.6.680 and Legacy v1.6.10.580: **Options > Advanced Vulkan options > Enable Vulkan scene transfers (requires restart)**. It defaults to off and is saved as `VulkanSceneTransfers=True/False` in `[General]` of the game's `ReshadeEffectShaderToggler.ini`, using REST's normal **Save changes** button. It applies to the whole REST configuration, not individual shader groups.

REST reads the preference before registering resource-creation callbacks and freezes the effective policy for this launch. The overlay shows **Active this launch** and a pending-change notice. Enabling, disabling, saving or reloading settings during play does not modify the active image-creation policy. Save and fully restart the game to apply a change. Reloading ReShade effects is insufficient.

`REST_VULKAN_SCENE_TRANSFERS=1` still forces on, and `=0` forces off, for diagnostic launches. An absent or invalid value uses the saved preference. Overrides never overwrite that preference. The overlay identifies a launcher override; remove it and restart to use the saved value.

For Enshrouded, keep `RESHADE_DISABLE_GRAPHICS_HOOK=1` and `--keep-vulkan-layers` in the launcher. Remove only `REST_VULKAN_SCENE_TRANSFERS` when testing configuration control. Leave Resource Shim set to none. All existing FP16 eligibility and Vulkan boundary checks remain in place; enabling the preference does not certify every game's allocation compatibility.

## Setup and verification

1. Install the appropriate build with the game closed. Launch without `REST_VULKAN_SCENE_TRANSFERS` in the environment. An existing config without the new key should show the option off and Active this launch: Disabled.
2. Enable the option and save. Confirm the pending-restart notice and that Active this launch stays Disabled.
3. Exit and relaunch. Confirm the option remains checked, Active this launch is Enabled, and the previously tested scene group injects normally.
4. Disable and save. Active remains Enabled until the next restart; after restarting it should be Disabled.
5. If using the old launcher, the override notice is expected. Remove its scene-transfer line before checking saved-setting behaviour.

Startup logs record configured/active values and whether a launcher override was present. Earlier v1.7.5.680/v1.6.9.580 builds use the launcher variable only.

A working scene boundary and included effects are still required. In the tested Enshrouded build, mark scene shader `0x46c3d744` instead of the three final HUD shaders. If the group says **Ticked techniques are EXCLUDED**, untick CinematicDOF (or your desired technique) to allow its injection. Confirm **Successful renders** increases.
