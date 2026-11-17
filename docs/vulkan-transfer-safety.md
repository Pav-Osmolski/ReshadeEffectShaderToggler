# Vulkan scene-transfer experiment: safety review

Status: optional feature disabled by default; not approved as a default allocation policy. The automatic target-size fix and manual injection guard are independent of this switch.

## Concrete correction

REST previously added shader-resource usage to render targets before testing transfer eligibility. ReShade's Vulkan descriptor conversion omits transient-attachment usage, so a native transient attachment could look eligible after that promotion. Vulkan forbids sampling/transfer usage on transient attachments ([image-creation valid usage](https://docs.vulkan.org/refpages/latest/refpages/source/VkImageCreateInfo.html#VUID-VkImageCreateInfo-usage-00963)).

The generic sampling promotion now skips Vulkan. The transfer experiment requires sampling support in the descriptor as received, before REST or its resource shim modifies it. A valid originally sampled image cannot be a transient attachment. The experiment also rejects non-undefined initial states, explicit CPU heaps, ReShade resource flags, non-FP16 formats, depth, multisampling, arrays and mip chains. Active resource shims disable the experiment. Format-support checks and all existing injection/pass-boundary checks remain mandatory.

## What the callback can and cannot establish

Reviewed against the pinned ReShade 6.8 SDK/source commit `18deaa52de0c425a78b329e9cb3c497281cd00ec`:

- `vulkan_hooks_device.cpp`, `vkCreateImage`: the add-on event receives a converted descriptor before native creation; the original native create-info is not passed to the add-on.
- `vulkan_impl_type_convert.cpp`, image conversion: dimensions, format, samples, selected usage and selected flags are exposed. Native tiling, protected/alias flags and the complete extension chain are not represented. External memory and sparse binding have partial API representations.
- `vulkan_impl_device.cpp`, `check_format_support`: tests optimal-tiling format feature bits. It does not query the complete native image-create combination, memory size/alignment or aliasing compatibility.

The Vulkan specification guarantees colour-image memory-type bits under certain unchanged native properties; this is not a blanket guarantee for size, alignment or data interpretation after changing usage. Pre-creation memory queries must use the actual creation parameters ([resource memory requirements](https://docs.vulkan.org/spec/latest/chapters/resources.html#resources-association)). Applications may precompute allocations from an unmodified create-info. The add-on callback cannot reconcile that plan with a changed image or prove a safe aliasing arrangement.

## Release decision

Keep the saved Vulkan scene-transfer option off and `REST_VULKAN_SCENE_TRANSFERS` unset for normal use. A full stable release may include this disabled-by-default advanced option with restart instructions and a known tested profile; it must not enable it globally or claim universal Vulkan or HDR correctness. Native creation/allocation validation or an upstream interface exposing the missing information is needed before considering a default policy. Do not bypass boundary guards or silently fall back to processing the already-composited UI.

The hardened Modern Enshrouded candidate passed loaded-world SDR/HDR placement with DLSS at 100% scale, and reduced-scale placement at 67% with a 1718×967 input and 3840×2160 output. Those observations support that profile on the tested system; they do not establish generic allocation safety. Regression tests cover descriptor admission/rejection. The stricter Modern candidate was game-tested on 17 November: SDR/HDR placement, hunting, toggles, scale and mode changes, and clean exit passed in that setup. The maintainer also confirmed a Legacy Enshrouded SDR/HDR pass; individual Legacy operations and host version were not separately recorded.
