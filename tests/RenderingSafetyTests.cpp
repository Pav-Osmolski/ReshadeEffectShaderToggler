#include "RenderingSafety.h"
#include "PreviewInputDiagnostics.h"
#include "DescriptorTableCapture.h"
#include "DescriptorMetadata.h"
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

enum class TestRootType { undefined, descriptor_table };
struct TestRoot {
    TestRootType type = TestRootType::undefined;
    int32_t buffer_index = -1;
    uint64_t descriptor_table = 0;
    Rendering::DescriptorTableCoverage coverage;
};

static void TestDescriptorTables() {
    using namespace reshade::api;
    descriptor_range ranges[3];
    ranges[0].binding = 3; ranges[0].count = 2; ranges[0].type = descriptor_type::shader_resource_view;
    ranges[1].binding = 9; ranges[1].count = 1; ranges[1].type = descriptor_type::constant_buffer;
    ranges[2].binding = 0; ranges[2].count = 1; ranges[2].type = descriptor_type::sampler;
    pipeline_layout_param param;
    param.type = pipeline_layout_param_type::descriptor_set;
    param.descriptor_set = {3, ranges};
    std::vector<uint32_t> copied;
    auto reader = [&](const descriptor_range& range, uint32_t count, auto& output) {
        copied.push_back(range.binding);
        for (uint32_t i = 0; i < count; ++i) {
            output[range.binding + i].type = range.type;
            output[range.binding + i].view = {100 + range.binding + i};
        }
        return true;
    };
    auto snapshot = Rendering::CaptureDescriptorTable<TestInputDescriptor>(param, reader);
    require(snapshot.coverage.supported && !snapshot.coverage.flagged && snapshot.coverage.samplers == 1);
    require(copied == (std::vector<uint32_t>{3, 9}) && snapshot.descriptors.size() == 10);
    require(snapshot.descriptors[0].type == static_cast<descriptor_type>(UINT32_MAX));
    require(snapshot.descriptors[3].view.handle == 103 && snapshot.descriptors[9].type == descriptor_type::constant_buffer);

#ifndef REST_LEGACY_TABLE_API
    descriptor_range_with_flags flagged[2];
    static_cast<descriptor_range&>(flagged[0]) = ranges[0];
    static_cast<descriptor_range&>(flagged[1]) = ranges[1];
    flagged[0].flags = descriptor_range_flags::descriptors_volatile;
    flagged[1].flags = descriptor_range_flags::data_static;
    param.type = pipeline_layout_param_type::descriptor_table_with_flags;
    param.descriptor_table_with_flags = {2, flagged};
    copied.clear();
    snapshot = Rendering::CaptureDescriptorTable<TestInputDescriptor>(param, reader);
    require(snapshot.coverage.flagged && snapshot.coverage.ranges == 2);
    require(copied == (std::vector<uint32_t>{3, 9})); // Second range uses derived-array stride.
    require(snapshot.descriptors[9].type == descriptor_type::constant_buffer);
    require(flagged[1].flags == descriptor_range_flags::data_static); // Original flags stay intact.
#endif

    std::vector<TestRoot> roots(1);
    roots[0].descriptor_table = 123;
    std::vector<std::vector<TestInputDescriptor>> buffers;
    Rendering::StoreDescriptorTableSnapshot(roots, buffers, 1, uint64_t{77}, std::move(snapshot));
    require(roots[0].descriptor_table == 123 && roots[1].descriptor_table == 77);
    require(roots[1].type == TestRootType::descriptor_table && roots[1].buffer_index == 0);
    require(buffers[0][9].type == descriptor_type::constant_buffer);

    param.type = pipeline_layout_param_type::descriptor_set;
    param.descriptor_set = {1, ranges};
    ranges[0].count = UINT32_MAX;
    snapshot = Rendering::CaptureDescriptorTable<TestInputDescriptor>(param, reader);
    require(snapshot.coverage.unbounded == 1 && snapshot.descriptors.empty());
    Rendering::StoreDescriptorTableSnapshot(roots, buffers, 1, uint64_t{88}, std::move(snapshot));
    require(roots[1].descriptor_table == 88 && buffers[roots[1].buffer_index].empty()); // Still replayable.

    ranges[0].binding = 65535; ranges[0].count = 8;
    snapshot = Rendering::CaptureDescriptorTable<TestInputDescriptor>(param, reader);
    require(snapshot.coverage.clipped == 1 && snapshot.descriptors.size() == 65536);
    require(snapshot.descriptors.back().view.handle == 65635);
    ranges[0].binding = UINT32_MAX - 1;
    snapshot = Rendering::CaptureDescriptorTable<TestInputDescriptor>(param, reader);
    require(snapshot.coverage.clipped == 1 && snapshot.descriptors.empty()); // No offset overflow.
    ranges[0].binding = 0; ranges[0].count = 1;
    snapshot = Rendering::CaptureDescriptorTable<TestInputDescriptor>(param, [](const auto&, uint32_t, auto&) { return false; });
    require(snapshot.coverage.missing == 1 && snapshot.descriptors[0].type == static_cast<descriptor_type>(UINT32_MAX));
    param.descriptor_set = {0, nullptr};
    snapshot = Rendering::CaptureDescriptorTable<TestInputDescriptor>(param, reader);
    require(snapshot.coverage.supported && snapshot.descriptors.empty());
    param.descriptor_set = {1, nullptr};
    snapshot = Rendering::CaptureDescriptorTable<TestInputDescriptor>(param, reader);
    require(snapshot.coverage.supported && snapshot.coverage.missing == 1);
    param.descriptor_set = {1, &ranges[2]};
    snapshot = Rendering::CaptureDescriptorTable<TestInputDescriptor>(param, reader);
    require(snapshot.coverage.samplers == 1 && snapshot.descriptors.empty());
    param.type = pipeline_layout_param_type::push_constants;
    snapshot = Rendering::CaptureDescriptorTable<TestInputDescriptor>(param, reader);
    require(!snapshot.coverage.supported && snapshot.descriptors.empty());
}

static void TestDescriptorMetadata() {
    using Store = Rendering::DescriptorMetadata<TestInputDescriptor, 4>;
    auto known = [](uint32_t i) { TestInputDescriptor d; d.type = reshade::api::descriptor_type::shader_resource_view; d.view = {100 + i}; return d; };
    auto unknown = [](const TestInputDescriptor& d) { return static_cast<uint32_t>(d.type) == UINT32_MAX; };
    Store high;
    require(high.Write(UINT32_MAX - 3, 4, known));
    require(high.Size() == 4 && high.ReadOne(UINT32_MAX).view.handle == 103);
    require(!high.Write(UINT32_MAX, 2, [](uint32_t) -> TestInputDescriptor { require(false); return {}; }));
    require(high.Size() == 4 && unknown(high.ReadOne(7)));

    Store source, destination;
    require(source.Write(20, 1, known));
    require(destination.Write(30, 3, known));
    const auto partial = source.Snapshot(20, 3);
    require(!destination.Write(30, 3, [&](uint32_t i) { return partial[i]; }));
    require(destination.ReadOne(30).view.handle == 100 && unknown(destination.ReadOne(31)) && unknown(destination.ReadOne(32)));
    const auto missing = source.Snapshot(90, 1);
    require(!destination.Write(30, 1, [&](uint32_t i) { return missing[i]; }));
    require(unknown(destination.ReadOne(30))); // Unknown copy must erase old valid metadata.

    Store same;
    require(same.Write(0, 3, known));
    const auto overlap = same.Snapshot(0, 3);
    require(same.Write(1, 3, [&](uint32_t i) { return overlap[i]; }));
    require(same.ReadOne(1).view.handle == 100 && same.ReadOne(3).view.handle == 102);
    require(!same.Write(1000, 1, known) && unknown(same.ReadOne(1000)) && same.Size() == 4);

    Store limited;
    require(limited.Write(7, 1, known));
    require(!limited.Write(0, 100, known));
    require(limited.Size() == 4 && unknown(limited.ReadOne(7))); // Invalidate skipped suffix too.
    require(limited.Snapshot(0, 100).size() == 4);
    require(limited.Snapshot(0, 0).empty());
    require(limited.Write(0, 0, known));
}

int main() {
    TestDescriptorMetadata();
    TestDescriptorTables();
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
