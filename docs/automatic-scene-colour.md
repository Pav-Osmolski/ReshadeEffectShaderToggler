# Automatic Scene Colour

**Auto scene colour** applies selected ReShade techniques to the live scene at an eligible shader boundary. When scene and output sizes differ, it can use native-resolution staging so multi-pass effects keep the resolution they expect.

It supports **D3D10, D3D11, D3D12 and Vulkan**. It does not enable Auto on D3D9 or OpenGL. Start with [effect placement](effect-placement.md) if you have not configured a group yet.

## Set it up

1. Hunt a suitable scene shader and finish with **Done hunting**.
2. Enable **Auto scene colour** in the group's **Effects** settings.
3. Include the desired techniques, keep them globally enabled in ReShade and retain their required global order.
4. Activate the group. Check **Successful renders** increases and **Last successful techniques** names the expected techniques.
5. Compare the visible result with the group off/on. Move the camera and open UI elements before saving.

An active group, a marked shader and the checkbox alone do not prove that injection succeeded. Inspect the diagnostics below.

## What REST does

REST selects the **primary live colour target (slot 0)** at the matched draw. It validates the target and compatible aspect ratio. When dimensions already match ReShade's output, it can render directly. For a resolution mismatch, it copies/resamples scene colour into a native-resolution staging target, runs the effects there and copies the result back. Later game rendering then continues.

For example, a `2560×1440` scene and `3840×2160` output can use native staging while keeping later UI above the effect. D3D uses the existing fullscreen shader-copy path; Vulkan uses image blits.

Automatic matching accepts output/input scale factors from **0.5 to 4.0** in both dimensions, with a compatible aspect ratio. This admits quarter-size through double-size targets. It supports combinations such as DLSS plus a reduced game render scale. It does not certify an arbitrary matched texture as scene colour. Manual matching modes retain their separate policy.

## Manual settings are preserved

While Auto is active, manual render-target index, SRV stage/slot/descriptor, swapchain-match mode and target-alpha preservation do not control this path. Their saved values are unchanged and become applicable again when Auto is disabled. Auto does not require a manual SRV search.

## Vulkan boundaries

Vulkan effect rendering and copies cannot run inside the game's active render pass. REST queues the matched target and waits for a proven safe later opportunity:

- A compatible new pass uses the same target with **LOAD** semantics, with tracking proving the previous pass ended.
- The pending target makes a **post-pass transition out of render-target usage**, allowing injection before its next use.

End/begin callbacks can also describe subpass transitions, so an end callback alone is insufficient. REST retains pass tracking, recursion guards and present/reload cleanup. It does not split the game pass or force a copy at an ambiguous point.

A final HUD pass may have no suitable continuation before the UI is already composed. Choose an earlier scene shader. **Waiting for safe Vulkan continuation or target transition** is an eligibility result, not a reason to disable safety checks.

The target needs transfer-source usage even for same-resolution rendering, because ReShade copies the supplied colour target internally. Native staging also needs transfer-destination usage, single sampling and supported blits. Unsupported images/boundaries are skipped.

### Optional scene-transfer support

Some games create suitable FP16 scene images without the transfer usage REST needs. **Options → Advanced Vulkan options → Enable Vulkan scene transfers (requires restart)** can request those flags for narrowly eligible images at creation.

It defaults off. Save and fully restart; the UI shows the current startup state and pending changes. It does not alter existing images or bypass boundary rules. An active resource shim disables the optional path. See the [setting guide](vulkan-scene-transfer-setting.md) and [allocation safety review](vulkan-transfer-safety.md).

## Read the diagnostics

| Field | Meaning |
|---|---|
| Current attempt / Current target | Most recent candidate and eligibility result; may change between draws |
| Last successful injection | Scene → effect resolution of the most recent successful render |
| Last successful staging | Direct, fullscreen shader copy or Vulkan image blit |
| Vulkan boundary | Successful same-target LOAD or post-pass RT transition boundary, when reported |
| Last successful techniques | Actual count and global execution order |
| Successful renders | Successful render count for the committed shader set; should increase during use |
| Recent candidates | Up to eight distinct recent shader/target candidates and their outcomes |
| Copy diagnostics | Support-ready details including REST version, API, targets and successes |

A rejected current draw can coexist with an earlier successful injection. Read both sections. Committing a new shader set resets the diagnostic history. A preview's availability alone does not prove effect-injection eligibility.

## Common outcomes

- **No successful renders:** check Active, encountered hashes, global technique state, inclusion/exclusion mode, required companion techniques and target eligibility.
- **Effects over the UI:** check that your technique is actually injected and the boundary precedes UI composition. An excluded or skipped effect may use ordinary end-of-frame rendering.
- **First frame skipped after a resize:** staging/permutations may need preparation. Subsequent eligible frames should resume; persistent failure needs its displayed reason.
- **Scene and effect sizes match:** valid; native staging is not required.
- **Frozen or stale image:** confirm the selected live scene target and test with a moving camera rather than a history/copy surface.

## Performance and HDR

Native staging adds two scene resampling copies, and the effects execute at native ReShade resolution. A resource/format change can briefly stall during safe replacement of allocations. This cost depends on output size and the techniques used.

Auto does not detect PQ/scRGB/HLG encoding or make SDR effects HDR-aware. See [HDR](HDR.md). [Validation records](validation.md) document the tested BG3 and Enshrouded configurations without extending them to every title, architecture or HDR mode.
