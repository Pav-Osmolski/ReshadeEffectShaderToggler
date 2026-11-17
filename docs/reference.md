# Technical reference

This page collects implementation details that are useful for maintainers and advanced configurations. For normal setup, start with the [user guides](README.md).

## Rendering and formats

Auto Scene Colour uses the primary live colour target. D3D native staging uses fullscreen shader copies; Vulkan uses image blits at proven safe boundaries. The automatic size predicate accepts output/input ratios 0.5–4.0 and aspect delta up to 0.1; manual matching is independent.

Typed-view caches distinguish incompatible interpretations such as FLOAT, UNORM and UINT. Some formats have no separate sRGB view; aliases are destroyed only once. Vulkan keeps the concrete selected view rather than assuming mutable linear/sRGB interpretations. D3D12 copy pipelines are keyed by destination format.

Previews require supported single-sample, single-layer, single-mip 2D targets. Vulkan preview work is deferred outside the active game pass. D3D12 skips preview/effect work in native render passes. D3D9 standalone render-target surfaces need not be shader-sampleable.

## Allocation and lifetime

Preview/group-buffer replacement synchronises the device's known queues at maintenance boundaries when existing allocations need disposal or replacement, rather than every rendered frame. Resolution/format changes can therefore briefly stall.

Removed groups are retired before cleanup and their CPU objects are retained until add-on teardown to protect stale references. Native D3D9 target/depth state is captured/restored and owned resources released before reset. The FFXIV constant hook is x64-only. These guards do not establish arbitrary concurrent mutation safety for every setting.

## Vulkan scene-transfer policy

Saved preference and diagnostic override are resolved before resource callbacks. The effective policy is frozen for the process; UI edits update the saved preference only. Overrides do not overwrite the preference.

The opt-in predicate requires originally sampled FP16 2D colour images with one sample/layer/mip, valid dimensions, undefined initial state, acceptable heap representation, no exposed resource flags and no active shim. Combined usage must pass format support. It does not expose every native allocation constraint. [Full allocation safety review](vulkan-transfer-safety.md).

## Descriptor limitations

Dynamic-offset state must survive partial descriptor rebinds. Legacy/API 7 cannot represent all Modern descriptor and pass information; unsupported Vulkan buffer layouts retain conservative fallback behaviour. Manual copied bindings require separate resource-state/boundary validation.

## Optional bounded tracing

For a diagnostic launch, `REST_VULKAN_BOUNDARY_TRACE=1` starts one capture per device when the first Auto work is queued. It records at most 512 events across three subsequent presents. Logs include shader hashes, targets, pass callbacks and barriers, but callback sequences alone are not proof of native pass completion. Title-screen frames can consume the capture before gameplay. Leave the variable unset in normal use and restart to capture again.

## Source and evidence

Relevant files include `RenderingSafety.h`, `RenderingEffectManager.cpp`, `ResourceManager.cpp`, `VulkanSceneTransferSettings.h` and backend state tracking. [Validation records](validation.md) tie results to releases/commits and distinguish native/API-host tests from injected game testing. [Build](BUILDING.md) · [Release](RELEASING.md).
