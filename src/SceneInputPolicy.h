#pragma once
#include "DescriptorTableCapture.h"
#include "RenderingSafety.h"

namespace Rendering {
inline bool IsStaticSamplerParameter(const reshade::api::pipeline_layout_param& param) {
    using namespace reshade::api;
    if (param.type != pipeline_layout_param_type::descriptor_table_with_flags || param.descriptor_table_with_flags.count == 0 ||
        param.descriptor_table_with_flags.ranges == nullptr)
        return false;
    for (uint32_t i = 0; i < param.descriptor_table_with_flags.count; ++i) {
        const auto& range = param.descriptor_table_with_flags.ranges[i];
        if (range.type != descriptor_type::sampler || range.static_samplers == nullptr)
            return false;
    }
    return true;
}
inline bool IsSceneInputFormat(reshade::api::format f) {
    using reshade::api::format;
    return f == format::r11g11b10_float || f == format::r16g16b16a16_float || f == format::r8g8b8a8_unorm || f == format::b8g8r8a8_unorm ||
           f == format::r10g10b10a2_unorm;
}
inline const char* SceneInputShapeRejection(const reshade::api::resource_desc& desc, const reshade::api::resource_view_desc& view) {
    using namespace reshade::api;
    if (!SupportsWholePreviewCopy(desc))
        return "Scene input rejected: resource must be a single-sample, single-layer, single-mip 2D texture";
    if (view.type != resource_view_type::texture_2d)
        return "Scene input rejected: view is not a plain 2D SRV";
    // ReShade's native D3D12 SRV conversion leaves first_layer/layers at zero
    // for a non-array 2D view. Both fields are ignored for that view type. The
    // resource topology and explicit view type above still reject arrays/MSAA.
    if (view.texture.first_level != 0 || (view.texture.levels != 1 && view.texture.levels != UINT32_MAX))
        return "Scene input rejected: view does not cover the single source mip";
    if (!IsSceneInputFormat(view.format))
        return "Scene input rejected: unsupported typed colour format";
    if (format_to_typeless(desc.texture.format) != format_to_typeless(view.format))
        return "Scene input rejected: view format does not match resource storage";
    if (static_cast<uint32_t>(desc.usage & resource_usage::shader_resource) == 0)
        return "Scene input rejected: resource lacks shader-read usage";
    return nullptr;
}
inline bool IsSceneInputShape(const reshade::api::resource_desc& desc, const reshade::api::resource_view_desc& view) {
    return SceneInputShapeRejection(desc, view) == nullptr;
}
inline bool CanCloneSceneTable(const DescriptorTableCoverage& coverage, size_t size) {
    return coverage.supported && coverage.ranges != 0 && coverage.samplers == 0 && coverage.unbounded == 0 && coverage.missing == 0 && coverage.clipped == 0 &&
           size > 0 && size <= 255;
}
}
