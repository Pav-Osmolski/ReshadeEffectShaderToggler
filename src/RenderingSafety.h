#pragma once
#include <reshade_api.hpp>
#include <vector>

namespace Rendering {
// API 7 conflates static and dynamic buffer descriptors and cannot replay offsets.
inline bool SupportsLegacyVulkanDescriptor(reshade::api::descriptor_type type) {
    return type != reshade::api::descriptor_type::constant_buffer &&
           type != reshade::api::descriptor_type::shader_storage_buffer;
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
