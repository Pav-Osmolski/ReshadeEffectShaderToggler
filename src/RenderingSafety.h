#pragma once
#include <reshade_api.hpp>
#include <vector>

namespace Rendering {
inline bool SupportsShaderCopy(reshade::api::device_api api) {
    using reshade::api::device_api;
    return api == device_api::d3d9 || api == device_api::d3d10 ||
           api == device_api::d3d11 || api == device_api::d3d12;
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
