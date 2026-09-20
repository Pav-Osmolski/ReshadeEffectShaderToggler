# Automatic Scene Colour

Automatic Scene Colour is a rendering mode for REST groups that need to apply ReShade effects to the live scene before later game passes such as fog or UI, while still allowing those effects to execute at the normal ReShade runtime resolution. It supports D3D10, D3D11, D3D12 and Vulkan on both x86 and x64 through ReShade's generic graphics API.

It was developed against **Baldur's Gate 3 in DX11 mode with DLSS enabled**. The Vulkan implementation was subsequently validated in BG3 with native-resolution staging, Before Fog injection and rapid shader-hunting navigation.

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
6. Returns control to the game. D3D injection occurs at the matched boundary; Vulkan waits for a safe boundary after the matched pass as described below.

The result is part of the scene before the later game passes are composited.

### Vulkan render-pass boundary and native staging

Vulkan does not permit image-transfer barriers, blits or a nested ReShade effect render while the game's render pass is active. ReShade's draw callback occurs inside that pass, so REST records the matched live target and defers the Auto injection.

REST processes deferred work at either of two proven safe boundaries:

- A new render pass that references the **same colour target with LOAD semantics**, when pass tracking establishes that the command list is outside the previous pass. CLEAR/DISCARD continuations are not eligible.
- A **post-pass RT transition** where the exact pending target changes from render-target usage to non-render-target usage. This supports final scene targets that have no later same-target LOAD pass. After injection, REST restores the usage requested by the game.

ReShade also emits end/begin callbacks at Vulkan subpass transitions. An end callback alone therefore does not prove the pass has finished. REST waits for a subsequent barrier to establish that the pass ended and rejects ambiguous subpass boundaries for both effects and preview copies. A recursion guard prevents REST's own transitions from triggering another Auto injection. Pending effects are cleared at present and effect reload so stale work does not carry into a later frame. Lightweight atomic pending-work flags let REST bypass deferred matching/copy processing on Vulkan callbacks when neither Auto Scene Colour nor a deferred preview has work waiting; render-pass/subpass state tracking still runs unchanged.

ReShade's Vulkan `render_technique` path first copies the supplied colour target into its internal effect-colour texture, so the live game image must already have transfer-source usage even when scene and effect resolutions match. REST deliberately does not retrofit transfer flags onto arbitrary Vulkan game images because the generic ReShade resource descriptor does not expose every native image-creation constraint (for example transient attachments).

When the deferred Vulkan injection needs native-resolution staging, the live target must additionally have transfer-destination usage. REST then uses ReShade's generic `copy_texture_region` blit path rather than its embedded Direct3D fullscreen-copy shaders, with explicit `render target -> copy source/copy destination -> render target` transitions around the up/downscale blits. The staging path also requires a single-sample colour target and Vulkan blit support.

REST does not split an existing game render pass. It cannot insert effects between draws within the same pass: choose a candidate with an eligible boundary before the desired later fog/UI composition. If no safe boundary is found, pending Auto work is skipped for that frame.

In the validated BG3 Vulkan configuration, `0x782733c1` provided the Before Fog boundary using **post-pass RT transition**, with `2560×1440 → 3840×2160` Vulkan image-blit staging. The user confirmed that flicker was resolved and rapid Prev/Next hunting was stable after the concurrency fixes. This hash is an example from that game configuration, not a universal preset; game versions and graphics settings can change shader hashes.

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

The group editor separates **the current candidate/latest target attempt** from **the last successful injection** so a rejected draw cannot make an earlier successful render look contradictory.

Current-attempt diagnostics include:

- **Current attempt** - the latest target-match/injection status, including Vulkan rejection reasons.
- **Current target** - dimensions, a human-readable ReShade format name and the resource handle for the latest candidate.

Last-success diagnostics include:

- **Last successful injection** - scene resolution -> effect resolution from the most recent successful render.
- **Last successful staging** - Direct, Vulkan image blit or fullscreen shader copy.
- **Vulkan boundary** - the actual successful boundary: `same-target LOAD pass` or `post-pass RT transition`.
- **Last successful techniques** - technique count and execution order.
- **Successful renders** - successful effect-render count for the currently committed shader set.
- **Copy diagnostics** - copies both sections in a support-ready block.
- **Recent attempts** - keeps the eight most recent distinct target/status attempts in memory, including shader hash, target, resolution/format and the successful Vulkan boundary when available.

Committing a new shader set resets the diagnostic history, so values from a previous candidate are not carried into the next test.

Vulkan target-rejection messages use format names such as **R16_FLOAT**, **R16G16_FLOAT** and **R8_UNORM** rather than raw enum values. Aspect-ratio failures and scale-range failures are reported separately.

When DLSS is active and the game renders below output resolution, a healthy Vulkan native-staging configuration should show a lower current/last-success scene resolution, the native effect resolution and **Vulkan image blit** as the last successful staging path.

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

### Vulkan keeps waiting for a safe boundary

Finding the shader and its target does not guarantee that the target has an eligible later boundary. Check **Successful renders** and **Vulkan boundary** after clicking Done. If no successful render is recorded, select a different candidate with a same-target LOAD continuation or a post-pass transition out of render-target usage. Preview availability alone does not prove that Auto injection will be possible.

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
- Vulkan requires the live colour target to have existing transfer-source usage. Native staging additionally requires existing transfer-destination usage, a single-sample target and Vulkan blit support.
- BG3 Vulkan native-staging and rapid-navigation runtime validation was confirmed for v1.6.0.633. Other titles, direct-resolution configurations and x86 runtime behaviour still need representative testing; x86/x64 build validation is not a substitute for those runtime checks.
- The implementation targets the **primary colour RTV (slot 0)**.
- It is intended for scene-colour injection around a user-selected shader boundary, not as a general replacement for ReShade's depth-buffer detection.
