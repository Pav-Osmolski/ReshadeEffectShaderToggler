#pragma once
#include "DescriptorTableCapture.h"
#include "RenderingSafety.h"

namespace Rendering {
inline bool IsSceneInputFormat(reshade::api::format f) {
    using reshade::api::format;
    return f == format::r11g11b10_float || f == format::r16g16b16a16_float || f == format::r8g8b8a8_unorm || f == format::b8g8r8a8_unorm ||
           f == format::r10g10b10a2_unorm;
}
inline bool IsSceneInputShape(const reshade::api::resource_desc& desc, const reshade::api::resource_view_desc& view) {
    using namespace reshade::api;
    return SupportsWholePreviewCopy(desc) && view.type == resource_view_type::texture_2d && view.texture.first_level == 0 &&
           (view.texture.levels == 1 || view.texture.levels == UINT32_MAX) && view.texture.first_layer == 0 &&
           (view.texture.layers == 1 || view.texture.layers == UINT32_MAX) && IsSceneInputFormat(view.format) &&
           format_to_typeless(desc.texture.format) == format_to_typeless(view.format) &&
           static_cast<uint32_t>(desc.usage & resource_usage::shader_resource) != 0;
}
inline bool CanCloneSceneTable(const DescriptorTableCoverage& coverage, size_t size) {
    return coverage.supported && coverage.ranges != 0 && coverage.samplers == 0 && coverage.unbounded == 0 && coverage.missing == 0 && coverage.clipped == 0 &&
           size > 0 && size <= 255;
}
}
