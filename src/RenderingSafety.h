#pragma once
#include <reshade_api.hpp>
#include <vector>
#include <cmath>

namespace Rendering {
inline bool MatchesAutomaticSceneDimensions(uint32_t targetWidth, uint32_t targetHeight,
                                           uint32_t outputWidth, uint32_t outputHeight) {
    if (targetWidth == 0 || targetHeight == 0 || outputWidth == 0 || outputHeight == 0)
        return false;
    const double widthScale = static_cast<double>(outputWidth) / targetWidth;
    const double heightScale = static_cast<double>(outputHeight) / targetHeight;
    const double aspectDelta = static_cast<double>(outputWidth) / outputHeight -
                               static_cast<double>(targetWidth) / targetHeight;
    // Marked scene passes may run below half output size when an upscaler is
    // combined with the game's render scale. Keep aspect and size bounds, but
    // admit targets down to one quarter of output dimensions. Manual matching
    // uses its existing policy, independently of this automatic-mode filter.
    return std::fabs(aspectDelta) <= 0.1 && widthScale >= 0.5 && widthScale <= 4.0 &&
           heightScale >= 0.5 && heightScale <= 4.0;
}

inline bool SupportsShaderCopy(reshade::api::device_api api) {
    using reshade::api::device_api;
    return api == device_api::d3d9 || api == device_api::d3d10 ||
           api == device_api::d3d11 || api == device_api::d3d12;
}

// D3D9 exposes standalone render targets as surfaces, rather than textures.
inline bool IsColorTargetType(reshade::api::device_api api, reshade::api::resource_type type) {
    return type == reshade::api::resource_type::texture_2d ||
           (api == reshade::api::device_api::d3d9 && type == reshade::api::resource_type::surface);
}

// Manual Vulkan injection has no deferred pass-boundary resolver. Never let
// a draw/pipeline callback enter ReShade's copy/render path inside a game pass.
enum class VulkanManualEffectBlock { none, render_pass, transfer_source, format_support };
inline VulkanManualEffectBlock CheckVulkanManualEffect(bool insideRenderPass,
                                                       const reshade::api::resource_desc& target,
                                                       bool copySourceFormatSupported) {
    if (insideRenderPass)
        return VulkanManualEffectBlock::render_pass;
    if ((target.usage & reshade::api::resource_usage::copy_source) == reshade::api::resource_usage::undefined)
        return VulkanManualEffectBlock::transfer_source;
    if (!copySourceFormatSupported)
        return VulkanManualEffectBlock::format_support;
    return VulkanManualEffectBlock::none;
}

inline bool ShouldAddShaderResourceUsage(reshade::api::device_api api,
                                         const reshade::api::resource_desc& desc) {
    using namespace reshade::api;
    // Vulkan's callback omits transient-attachment usage. Never promote an
    // attachment to sampled usage when its native restrictions are unknown.
    return api != device_api::vulkan && desc.type == resource_type::texture_2d &&
           (desc.usage & resource_usage::render_target) != resource_usage::undefined &&
           (desc.usage & resource_usage::shader_resource) == resource_usage::undefined;
}

// Opt-in FP16 experiment using only properties exposed by ReShade. No-flags
// here means no API resource_flags; native alias/protected/tiling information
// is not fully represented and cannot be certified by this predicate.
inline bool IsVulkanSceneTransferCandidate(reshade::api::device_api api,
                                          const reshade::api::resource_desc& desc,
                                          reshade::api::resource_usage initialState = reshade::api::resource_usage::undefined) {
    using namespace reshade::api;
    return api == device_api::vulkan && initialState == resource_usage::undefined &&
           desc.type == resource_type::texture_2d &&
           desc.texture.format == format::r16g16b16a16_float &&
           desc.texture.width != 0 && desc.texture.height != 0 &&
           desc.texture.samples == 1 && desc.texture.depth_or_layers == 1 && desc.texture.levels == 1 &&
           // Vulkan create_image runs before memory is bound, so heap is unknown.
           (desc.heap == memory_heap::unknown || desc.heap == memory_heap::default_) && desc.flags == resource_flags::none &&
           (desc.usage & resource_usage::render_target) != resource_usage::undefined &&
           // An originally sampled image cannot legally be transient. Do not
           // infer this from usage that REST itself added earlier in the hook.
           (desc.usage & resource_usage::shader_resource) != resource_usage::undefined &&
           (desc.usage & resource_usage::depth_stencil) == resource_usage::undefined;
}

struct ColorViewFormats {
    reshade::api::format linear;
    reshade::api::format srgb;
};

inline ColorViewFormats GetColorViewFormats(reshade::api::device_api api,
                                          reshade::api::format storage,
                                          reshade::api::format requested = reshade::api::format::unknown) {
    using namespace reshade::api;
    using format = reshade::api::format;
    const format selected = requested != format::unknown ? requested : storage;
    // Vulkan images cannot be assumed to allow another format interpretation.
    // D3D (including swapchain back buffers) and OpenGL retain their existing
    // linear/sRGB view attempts; callers fall back when an alternate is invalid.
    if (api == device_api::vulkan)
        return { selected, selected };
    return { format_to_default_typed(selected, 0), format_to_default_typed(selected, 1) };
}

inline bool SameColorViewFormat(reshade::api::format a, reshade::api::format b) {
    using namespace reshade::api;
    using format = reshade::api::format;
    // sRGB and linear variants share channel encoding for raw copies, but a
    // FLOAT/UNORM/UINT reinterpretation must recreate the cached typed views.
    return a != format::unknown && b != format::unknown &&
           format_to_default_typed(a, 0) == format_to_default_typed(b, 0);
}

inline bool SupportsWholePreviewCopy(const reshade::api::resource_desc& desc) {
    return desc.type == reshade::api::resource_type::texture_2d && desc.texture.samples == 1 &&
           desc.texture.levels == 1 && desc.texture.depth_or_layers == 1 &&
           desc.texture.width != 0 && desc.texture.height != 0;
}

// A partial descriptor-set rebind must replace only that set's dynamic offsets.
inline bool SaveDynamicOffsets(std::vector<uint32_t>& saved, uint32_t needed, uint32_t& cursor,
                               uint32_t count, const uint32_t* offsets) {
    saved.clear();
    if (cursor > count || needed > count - cursor || (needed != 0 && offsets == nullptr))
        return false;
    if (needed != 0)
        saved.assign(offsets + cursor, offsets + cursor + needed);
    cursor += needed;
    return true;
}
}
