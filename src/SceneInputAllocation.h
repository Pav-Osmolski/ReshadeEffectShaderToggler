#pragma once
#include <reshade_api.hpp>
#include <algorithm>
#include <vector>

namespace Rendering {
// Native game root signatures do not necessarily have the private allocation
// metadata attached by ReShade's create_pipeline_layout implementation. Create
// our own allocation layout; bind the resulting tables with the GAME layout.
// Uniform capacity keeps this temporary layout to one root parameter. Tables
// are allocations in the device heap and outlive the temporary root signature.
template<class Device>
bool AllocateSceneInputTables(Device& device, const std::vector<uint32_t>& sizes,
                              std::vector<reshade::api::descriptor_table>& tables) {
    using namespace reshade::api;
    if (sizes.empty() || std::any_of(tables.begin(), tables.end(), [](auto table) { return table != 0; })) return false;
    const uint32_t capacity = *std::max_element(sizes.begin(), sizes.end());
    if (capacity == 0 || capacity > 255) return false;
    struct Layout {
        Device& device;
        pipeline_layout handle = {0};
        ~Layout() { if (handle != 0) device.destroy_pipeline_layout(handle); }
    } allocation{device};
    const descriptor_range range{0, 0, 0, capacity, shader_stage::all, 1, descriptor_type::shader_resource_view};
    pipeline_layout_param param;
    param.type = pipeline_layout_param_type::descriptor_table;
    param.descriptor_table.count = 1;
    param.descriptor_table.ranges = &range;
    if (!device.create_pipeline_layout(1, &param, &allocation.handle)) return false;
    tables.assign(sizes.size(), descriptor_table{0});
    for (size_t i = 0; i < sizes.size(); ++i) {
        if (sizes[i] == 0) continue;
        if (!device.allocate_descriptor_table(allocation.handle, 0, &tables[i])) {
            for (auto& table : tables) {
                if (table != 0) device.free_descriptor_table(table);
                table = {0};
            }
            return false;
        }
    }
    return true;
}
}
