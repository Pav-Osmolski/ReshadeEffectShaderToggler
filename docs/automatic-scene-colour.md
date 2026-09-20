# Automatic Scene Colour

Automatic Scene Colour is a rendering mode for REST groups that need to apply ReShade effects to the live scene before later game passes such as fog or UI, while still allowing those effects to execute at the normal ReShade runtime resolution. It supports D3D10, D3D11, D3D12 and Vulkan on both x86 and x64 through ReShade's generic graphics API.

It was developed and validated against **Baldur's Gate 3 in DX11 mode with DLSS enabled**.

## Why it exists

With an upscaler enabled, a game may render its scene at an internal resolution that is lower than the swapchain/output resolution.

For example:

- live scene target: `2560x1440`
- ReShade runtime/output: `3840x2160`

Running a ReShade technique directly against the lower-resolution target can be insufficient for effects whose intermediate textures, kernels or dependent techniques are created at the ReShade runtime resolution. A multi-pass AO chain may therefore fail, render only partially, or appear at the wrong scale.

Rendering the effect at the end of the frame avoids that mismatch, but then it is also applied over UI and other later composition passes.

Automatic Scene Colour solves both problems.

## Rendering path

When **Auto scene colour** is enabled for a group, REST:

1. Waits until a shader in that group is encountered.
2. Uses render-target slot 0, the primary live colour RTV bound at that draw.
3. Validates it as a colour target using automatic aspect-ratio matching.
4. If the target already matches the ReShade runtime resolution, renders the selected techniques directly into it.
5. If the target is lower resolution:
   - transitions the live target for sampling;
   - copies/upscales it into a native-resolution staging target;
   - runs the selected ReShade techniques against that staging target;
   - transitions the resources back;
   - copies/downscales the completed result into the original live scene target.
6. Lets the game's matched draw and all subsequent passes continue normally.

The result is part of the scene before the later game passes are composited.

### Vulkan render-pass boundary and native staging

Vulkan does not permit image-transfer barriers, blits or a nested ReShade effect render while the game's render pass is active. ReShade's draw callback occurs inside that pass, so REST records the matched live target and defers the Auto injection.

REST processes that deferred work at the next ReShade `begin_render_pass` callback that references the **same colour target with LOAD semantics**. ReShade emits that callback before the underlying Vulkan render pass begins, giving REST a legal command-buffer boundary. A CLEAR or DISCARD continuation is ignored because it would immediately overwrite the injected result.

When the deferred Vulkan injection needs native-resolution staging, REST does not use its embedded Direct3D fullscreen-copy shaders. Instead it uses ReShade's generic `copy_texture_region` blit path. Compatible Vulkan colour targets are opted into transfer-source and transfer-destination usage when they are created, then REST performs explicit `render target -> copy source/copy destination -> render target` transitions around the up/downscale blits. The staging path requires a single-sample colour target plus Vulkan transfer/blit support for the selected format.

This first Vulkan implementation deliberately does not split an existing game render pass. If the desired later UI/fog/post-processing composition occurs inside the same pass as the matched draw, REST leaves the pending injection untouched rather than recording invalid Vulkan commands.

## What is automatic

Auto mode deliberately does not use the old manual SRV-selection path.

The following manual settings do not control Auto mode while it is active:

- render-target index;
- SRV shader stage;
- SRV slot;
- SRV descriptor/binding;
- manual swapchain matching mode;
- preserve-target-alpha.

These settings are not overwritten by Auto mode. They remain saved with the group and become active again if Auto scene colour is disabled.

The implementation always uses the primary live colour RTV and aspect-ratio matching.

This also protects existing configurations that still contain values saved by older or experimental builds.

## Recommended setup

1. Create or select the group that represents the desired injection boundary.
2. Hunt and mark the shader that occurs immediately after the scene point where the effect should be visible.
3. Open the group's render-target settings.
4. Enable **Auto scene colour**.
5. Open the effect list.
6. Select the techniques that should execute at that boundary.
7. Keep those techniques enabled globally in ReShade.
8. Verify the reported **Scene resolution** and **Effect resolution**.
9. Move the camera and open/close UI elements to confirm that the effect updates every frame and remains below later UI/fog passes.

No manual SRV stage, slot or binding search should be necessary.

## Multi-pass effects

REST executes selected techniques in the global order defined by ReShade.

If an effect depends on a preparation/kernel technique, select every required technique and keep them in the correct global order.

For the BG3 test case, the successful Lumenite AO chain ran:

`Lumenite_Kernel -> Lumenite_LSAO`

The exact technique names and requirements depend on the shader package being used.

## Diagnostics

The group editor displays:

- **Target** - confirms that Auto mode is using the live render target.
- **Scene resolution** - resolution of the matched live game target.
- **Effect resolution** - resolution at which the ReShade techniques are running.
- **Technique order** - the techniques REST rendered on the last successful injection.
- **Injection** - reports whether an effect dispatch has completed successfully and how many techniques were rendered.
- **Native staging** - reports whether the native-resolution staging path is active or not required.
- **Copy diagnostics** - copies a support-ready block containing the REST version, graphics API, group name, scene/effect resolutions, staging state, technique count/order, render-call count and last target handle.

When DLSS is active and the game renders below output resolution, a healthy configuration should normally show a lower scene resolution, the native effect resolution and **Native staging: Active**.

## Troubleshooting

### The effect appears over the UI

The marked shader boundary is probably too late in the frame, or the group is not the group actually applying the technique.

Verify that Auto scene colour is enabled on the intended group and hunt a shader boundary that occurs before the UI pass.

### The effect is frozen for one frame

A frozen result usually indicates that the effect was injected into a history/copy surface rather than the live target.

Current Auto mode does not use the experimental SRV candidate-selection path. Confirm that you are running the current build and that the editor reports **Live render target (matched draw)**.

### The effect does not render

Check that:

- the group is active;
- the intended shader is marked and is actually encountered;
- the technique is enabled globally in ReShade;
- the group's effect mode includes the technique;
- the reported target is a valid colour target with the same aspect ratio as the output;
- required companion techniques are also selected.

### The first matching frame is skipped

When native staging needs to be created or resized, REST may defer the effect until the staging resource has been created by its resource manager. Subsequent matching frames should render normally.

### The scene and effect resolutions are identical

That is valid. Native staging is only needed when the live scene resolution differs from the ReShade runtime/output resolution.

## Performance

Native staging adds two fullscreen copy/resample operations:

1. live scene -> native staging;
2. completed native result -> live scene.

The selected ReShade techniques also execute at the native runtime resolution rather than the lower internal game resolution. GPU cost therefore depends on output resolution and the effects being run.

## Current scope

- **D3D10, D3D11, D3D12 and Vulkan are supported on x86 and x64.** They share the same live-target selection and effect-dispatch architecture through ReShade's generic API.
- Baldur's Gate 3 DX11 + DLSS remains the primary runtime-tested D3D configuration and regression reference for scene-colour behaviour.
- D3D10/11/12 use REST's existing fullscreen shader-copy path when native staging is required.
- Vulkan uses ReShade's image-blit path for native staging and therefore requires a single-sample colour target with transfer/blit support.
- Vulkan runtime validation is a separate release gate; support should not be considered release-ready until a representative Vulkan title has exercised both direct and native-staging paths.
- The implementation targets the **primary colour RTV (slot 0)**.
- It is intended for scene-colour injection around a user-selected shader boundary, not as a general replacement for ReShade's depth-buffer detection.
