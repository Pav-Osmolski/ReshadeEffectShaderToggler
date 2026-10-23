#include "RenderingSafety.h"
#include "PreviewInputDiagnostics.h"
#include <stdexcept>

static void require(bool condition) {
    if (!condition) throw std::runtime_error("Rendering safety regression");
}

struct TestInputDescriptor {
    reshade::api::descriptor_type type = static_cast<reshade::api::descriptor_type>(UINT32_MAX);
    reshade::api::resource_view view = { 0 };
    reshade::api::sampler_with_resource_view sampler_and_view = {};
};

struct TestInputState {
    std::vector<size_t> sizes;
    std::vector<TestInputDescriptor> descriptors;
    bool repeat = false;
    size_t get_root_table_size_at(uint32_t stage) const { require(stage == 2); return sizes.size(); }
    size_t get_root_table_entry_size_at(uint32_t stage, uint32_t slot) const { require(stage == 2); return sizes.at(slot); }
    const TestInputDescriptor* get_descriptor_at(uint32_t stage, uint32_t, uint32_t binding) const {
        require(stage == 2);
        if (descriptors.empty()) return nullptr;
        return repeat ? &descriptors[0] : binding < descriptors.size() ? &descriptors[binding] : nullptr;
    }
};

static void TestPreviewInputs() {
    using namespace reshade::api;
    TestInputState state;
    state.sizes = { 7 };
    state.descriptors.resize(6);
    state.descriptors[0].type = descriptor_type::shader_resource_view;
    state.descriptors[0].view = { 11 };
    state.descriptors[1].type = descriptor_type::constant_buffer;
    state.descriptors[1].view = { 99 }; // Must never be queried as a view.
    state.descriptors[2].type = descriptor_type::unordered_access_view;
    state.descriptors[2].view = { 99 };
    state.descriptors[3].type = descriptor_type::sampler_with_resource_view;
    state.descriptors[3].sampler_and_view.view = { 22 };
    state.descriptors[3].view = { 99 }; // Combined descriptors use their own payload.
    state.descriptors[4].view = { 99 }; // Sparse/unobserved entry.
    state.descriptors[5].type = descriptor_type::shader_resource_view; // Null view.
    size_t budget = 1024;
    std::vector<uint64_t> views;
    auto scan = Rendering::ScanPreviewInputs(state, 2, budget, [&](uint32_t slot, uint32_t binding, resource_view view) {
        require(slot == 0 && (binding == 0 || binding == 3));
        views.push_back(view.handle);
    });
    require(views == (std::vector<uint64_t>{11, 22}));
    require(scan.inspected == 7 && !scan.truncated && budget == 1017);

    // A bindless-sized table must not cause an unbounded walk or GPU queries.
    state.descriptors.clear();
    state.sizes = { 1000000000 };
    auto forbidden = [](uint32_t, uint32_t, resource_view) { require(false); };
    budget = 1024;
    scan = Rendering::ScanPreviewInputs(state, 2, budget, forbidden);
    require(scan.inspected == 256 && scan.truncated);
    state.sizes.assign(70, 1);
    budget = 1024;
    scan = Rendering::ScanPreviewInputs(state, 2, budget, forbidden);
    require(scan.inspected == 64 && scan.truncated);
    state.sizes.assign(5, 256);
    budget = 1024;
    scan = Rendering::ScanPreviewInputs(state, 2, budget, forbidden);
    require(scan.inspected == 1024 && scan.truncated && budget == 0);
    scan = Rendering::ScanPreviewInputs(state, 2, budget, forbidden);
    require(scan.inspected == 0 && scan.truncated); // Repeated same-frame match.

    state.sizes = { 100 };
    state.descriptors.resize(1);
    state.descriptors[0].type = descriptor_type::shader_resource_view;
    state.descriptors[0].view = { 1 };
    state.repeat = true;
    budget = 1024;
    size_t queries = 0;
    scan = Rendering::ScanPreviewInputs(state, 2, budget, [&](uint32_t, uint32_t, resource_view) { ++queries; });
    require(queries == 64 && scan.truncated && scan.sampled_views == 64);
}

int main() {
    TestPreviewInputs();
    using namespace reshade::api;
    require(Rendering::SupportsShaderCopy(device_api::d3d9));
    require(Rendering::SupportsShaderCopy(device_api::d3d10));
    require(Rendering::SupportsShaderCopy(device_api::d3d11));
    require(Rendering::SupportsShaderCopy(device_api::d3d12));
    require(!Rendering::SupportsShaderCopy(device_api::vulkan));
    require(!Rendering::SupportsShaderCopy(device_api::opengl));
    require(!Rendering::SameColorViewFormat(format::r16g16b16a16_float, format::r16g16b16a16_unorm));
    require(!Rendering::SameColorViewFormat(format::r10g10b10a2_uint, format::r10g10b10a2_unorm));
    require(Rendering::SameColorViewFormat(format::r8g8b8a8_unorm, format::r8g8b8a8_unorm_srgb));
    require(!Rendering::SameColorViewFormat(format::unknown, format::unknown));
    for (const auto api : {device_api::d3d10, device_api::d3d11, device_api::d3d12, device_api::vulkan}) {
        const auto hdr = Rendering::GetColorViewFormats(api, format::r16g16b16a16_float);
        require(hdr.linear == format::r16g16b16a16_float && hdr.srgb == hdr.linear);
        const auto hdr10 = Rendering::GetColorViewFormats(api, format::r10g10b10a2_unorm);
        require(hdr10.linear == format::r10g10b10a2_unorm && hdr10.srgb == hdr10.linear);
        const auto srgb = Rendering::GetColorViewFormats(api, format::r8g8b8a8_unorm_srgb);
        if (api == device_api::vulkan)
            require(srgb.linear == format::r8g8b8a8_unorm_srgb && srgb.srgb == srgb.linear);
        else
            require(srgb.linear == format::r8g8b8a8_unorm && srgb.srgb == format::r8g8b8a8_unorm_srgb);
    }
    const auto typeless = Rendering::GetColorViewFormats(device_api::d3d11, format::r8g8b8a8_typeless);
    require(typeless.linear == format::r8g8b8a8_unorm && typeless.srgb == format::r8g8b8a8_unorm_srgb);

    require(Rendering::SupportsLegacyVulkanDescriptor(descriptor_type::shader_resource_view));
    require(!Rendering::SupportsLegacyVulkanDescriptor(descriptor_type::constant_buffer));
    require(!Rendering::SupportsLegacyVulkanDescriptor(descriptor_type::shader_storage_buffer));
    resource_desc desc(1920, 1080, 1, 1, format::r16g16b16a16_float, 1,
                       memory_heap::gpu_only, resource_usage::render_target);
    require(Rendering::SupportsWholePreviewCopy(desc));
    desc.texture.samples = 4;
    require(!Rendering::SupportsWholePreviewCopy(desc));
    desc.texture.samples = 1;
    desc.texture.levels = 2;
    require(!Rendering::SupportsWholePreviewCopy(desc));
    desc.texture.levels = 1;
    desc.texture.depth_or_layers = 6;
    require(!Rendering::SupportsWholePreviewCopy(desc));
    desc.texture.depth_or_layers = 1;
    desc.texture.width = 0;
    require(!Rendering::SupportsWholePreviewCopy(desc));
    desc.texture.width = 1920;
    desc.type = resource_type::texture_3d;
    require(!Rendering::SupportsWholePreviewCopy(desc));

    std::vector<uint32_t> set0, set1;
    uint32_t cursor = 0;
    const uint32_t initial[] = { 256, 512, 1024 };
    require(Rendering::SaveDynamicOffsets(set0, 1, cursor, 3, initial));
    require(Rendering::SaveDynamicOffsets(set1, 2, cursor, 3, initial));
    require(set0 == std::vector<uint32_t>{256});
    require(set1 == (std::vector<uint32_t>{512, 1024}));
    // Bind only set 1: set 0's offset must survive.
    const uint32_t changed[] = { 2048, 4096 };
    cursor = 0;
    require(Rendering::SaveDynamicOffsets(set1, 2, cursor, 2, changed));
    require(set0 == std::vector<uint32_t>{256});
    require(set1 == (std::vector<uint32_t>{2048, 4096}));
    cursor = 0;
    require(!Rendering::SaveDynamicOffsets(set1, 3, cursor, 2, changed));
    require(set1.empty());
    require(!Rendering::SaveDynamicOffsets(set1, 1, cursor, 1, nullptr));
    require(Rendering::SaveDynamicOffsets(set1, 0, cursor, 0, nullptr));
}
