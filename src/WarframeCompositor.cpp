#include "WarframeCompositor.h"
#include "WarframeShaders.h"
#include "PipelinePrivateData.h"
#include <reshade.hpp>

using namespace reshade::api;

namespace Rendering {
namespace {
bool warframe = false;
bool splitEnabled = false;
std::filesystem::path compilerDirectory;
}

void ConfigureWarframeSplit(const std::filesystem::path& executable, const std::filesystem::path& addon, bool enabled) {
#ifdef _WIN64
    warframe = _wcsicmp(executable.filename().c_str(), L"Warframe.x64.exe") == 0;
    splitEnabled = warframe && enabled;
    if (splitEnabled)
        compilerDirectory = addon.parent_path() / L"REST-DXC";
#else
    (void)executable;
    (void)addon;
    (void)enabled;
#endif
}

bool WarframeSplitAvailable() { return warframe; }
bool WarframeSplitEnabled() { return splitEnabled; }

WarframeSplitPipeline::~WarframeSplitPipeline() {
    if (scene != 0) device->destroy_pipeline(scene);
    if (ui != 0) device->destroy_pipeline(ui);
}

static void CreateSplitPipeline(WarframeSplitPipeline& result, uint32_t count, const pipeline_subobject* objects) {
    const shader_desc* pixel = nullptr;
    const shader_desc* vertex = nullptr;
    const blend_desc* blend = nullptr;
    size_t pixelIndex = 0, formatIndex = 0, blendIndex = 0;
    bool target = false, rasterizer = false, depthStencil = false, topology = false;
    for (uint32_t i = 0; i < count; ++i) {
        const auto& object = objects[i];
        if (object.count == 0) continue;
        if (!object.data) return;
        switch (object.type) {
            case pipeline_subobject_type::vertex_shader:
                if (vertex || object.count != 1) return;
                vertex = static_cast<const shader_desc*>(object.data);
                break;
            case pipeline_subobject_type::pixel_shader:
                if (pixel || object.count != 1) return;
                pixel = static_cast<const shader_desc*>(object.data);
                pixelIndex = i;
                break;
            case pipeline_subobject_type::render_target_formats:
                if (target || object.count != 1 || *static_cast<const format*>(object.data) != format::r10g10b10a2_unorm) return;
                target = true;
                formatIndex = i;
                break;
            case pipeline_subobject_type::blend_state:
                if (blend || object.count != 1) return;
                blend = static_cast<const blend_desc*>(object.data);
                if (blend->blend_enable[0] || blend->logic_op_enable[0] || blend->alpha_to_coverage_enable ||
                    blend->render_target_write_mask[0] != 0xF) return;
                blendIndex = i;
                break;
            case pipeline_subobject_type::depth_stencil_state: {
                if (depthStencil || object.count != 1) return;
                const auto& state = *static_cast<const depth_stencil_desc*>(object.data);
                if (state.depth_enable || state.stencil_enable) return;
                depthStencil = true;
                break;
            }
            case pipeline_subobject_type::rasterizer_state: {
                if (rasterizer || object.count != 1) return;
                const auto& state = *static_cast<const rasterizer_desc*>(object.data);
                if (state.fill_mode != fill_mode::solid || state.conservative_rasterization != 0) return;
                rasterizer = true;
                break;
            }
            case pipeline_subobject_type::primitive_topology: {
                if (topology || object.count != 1) return;
                const auto type = *static_cast<const primitive_topology*>(object.data);
                if (type != primitive_topology::triangle_list && type != primitive_topology::triangle_strip) return;
                topology = true;
                break;
            }
            case pipeline_subobject_type::sample_mask:
                if (object.count != 1 || (*static_cast<const uint32_t*>(object.data) & 1) == 0) return;
                break;
            case pipeline_subobject_type::sample_count:
            case pipeline_subobject_type::viewport_count:
                if (object.count != 1 || *static_cast<const uint32_t*>(object.data) != 1) return;
                break;
            case pipeline_subobject_type::depth_stencil_format:
                if (object.count != 1) return;
                break;
            case pipeline_subobject_type::flags:
                if (object.count != 1 || *static_cast<const pipeline_flags*>(object.data) != pipeline_flags::none) return;
                break;
            case pipeline_subobject_type::input_layout:
            case pipeline_subobject_type::dynamic_pipeline_states:
                break;
            default:
                return; // No extra shader stages, stream output or unknown state.
        }
    }
    if (result.layout == 0 || !pixel || !vertex || !target || !blend || !depthStencil || !rasterizer || !topology)
        return;
    if (!IsWarframeVertexShader(vertex->code, vertex->code_size)) {
        result.status = "Warframe split: vertex shader revision is unsupported";
        return;
    }
    WarframeShaderPair pair;
    if (!BuildWarframeShaderPair(compilerDirectory, pixel->code, pixel->code_size, pair, result.status))
        return;

    std::vector<pipeline_subobject> clone(objects, objects + count);
    shader_desc replacement = *pixel;
    replacement.code = pair.ui.data();
    replacement.code_size = pair.ui.size();
    clone[pixelIndex].data = &replacement;
    result.status = "Warframe split: could not create compositor UI pipeline";
    if (!result.device->create_pipeline(result.layout, count, clone.data(), &result.ui))
        return;
    replacement.code = pair.scene.data();
    replacement.code_size = pair.scene.size();
    format targets[]{format::r32g32b32a32_float, format::r32_float};
    clone[formatIndex] = {pipeline_subobject_type::render_target_formats, 2, targets};
    blend_desc sceneBlend = *blend;
    sceneBlend.blend_enable[1] = sceneBlend.logic_op_enable[1] = false;
    sceneBlend.render_target_write_mask[1] = 1;
    clone[blendIndex].data = &sceneBlend;
    result.status = "Warframe split: could not create graded scene pipeline";
    if (!result.device->create_pipeline(result.layout, count, clone.data(), &result.scene))
        return;
    result.status = "Warframe split: pipeline ready";
}

void InitWarframeSplitPipeline(device* dev, pipeline_layout layout, uint32_t count,
                              const pipeline_subobject* objects, pipeline original) {
    if (!WarframeSplitEnabled() || dev->get_api() != device_api::d3d12)
        return;
    for (uint32_t i = 0; i < count; ++i) {
        const auto& object = objects[i];
        if (object.type != pipeline_subobject_type::pixel_shader || object.count != 1 || !object.data)
            continue;
        const auto& pixel = *static_cast<const shader_desc*>(object.data);
        if (IdentifyWarframePixelShader(pixel.code, pixel.code_size) == WarframeShader::unknown)
            return;
        auto result = std::make_shared<WarframeSplitPipeline>();
        result->device = dev;
        result->layout = layout;
        try {
            CreateSplitPipeline(*result, count, objects);
        } catch (const std::exception&) {
            result->status = "Warframe split: compositor pipeline preparation failed";
        }
        reshade::log::message(result->scene != 0 && result->ui != 0 ? reshade::log::level::info : reshade::log::level::warning,
                              result->status);
        auto& data = *dev->get_private_data<DeviceDataContainer>();
        std::lock_guard lock(data.scenePipelineMutex);
        data.warframePipelines.insert_or_assign(original.handle, std::move(result));
        return;
    }
}
}
