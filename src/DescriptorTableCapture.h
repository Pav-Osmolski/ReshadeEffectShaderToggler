#pragma once
#include <reshade_api.hpp>
#include <algorithm>
#include <vector>
#include <utility>

namespace Rendering {
struct DescriptorTableCoverage {
    bool supported = false;
    bool flagged = false;
    uint32_t ranges = 0;
    uint32_t unbounded = 0;
    uint32_t samplers = 0;
    uint32_t missing = 0;
    uint32_t clipped = 0;
};

template <typename Descriptor>
struct DescriptorTableSnapshot {
    DescriptorTableCoverage coverage;
    std::vector<Descriptor> descriptors;
};

// Snapshot metadata only. GPU table replay must retain the original table
// handle independently, even for empty, unbounded or partially observed tables.
template <typename Descriptor, typename Reader>
DescriptorTableSnapshot<Descriptor> CaptureDescriptorTable(const reshade::api::pipeline_layout_param& param, Reader read) {
    using namespace reshade::api;
    DescriptorTableSnapshot<Descriptor> result;
    auto& coverage = result.coverage;
    std::vector<descriptor_range> ranges;
    if (param.type == pipeline_layout_param_type::descriptor_table) {
        coverage.supported = true;
        coverage.ranges = param.descriptor_table.count;
        if (coverage.ranges != 0 && param.descriptor_table.ranges == nullptr) {
            coverage.missing = coverage.ranges;
            return result;
        }
        for (uint32_t i = 0; i < coverage.ranges; ++i)
            ranges.push_back(param.descriptor_table.ranges[i]);
    } else if (param.type == pipeline_layout_param_type::descriptor_table_with_flags) {
        coverage.supported = true;
        coverage.flagged = true;
        coverage.ranges = param.descriptor_table_with_flags.count;
        if (coverage.ranges != 0 && param.descriptor_table_with_flags.ranges == nullptr) {
            coverage.missing = coverage.ranges;
            return result;
        }
        // Index the derived array before slicing: base-pointer arithmetic would
        // read the wrong stride from the second flagged range onwards.
        for (uint32_t i = 0; i < coverage.ranges; ++i)
            ranges.push_back(param.descriptor_table_with_flags.ranges[i]);
    } else {
        return result;
    }

    constexpr uint32_t maxEntries = 65536;
    uint32_t size = 0;
    for (const auto& range : ranges) {
        if (range.type == descriptor_type::sampler) { ++coverage.samplers; continue; }
        if (range.count == UINT32_MAX) { ++coverage.unbounded; continue; }
        if (range.count == 0) continue;
        if (range.binding >= maxEntries) { ++coverage.clipped; continue; }
        const uint32_t count = std::min(range.count, maxEntries - range.binding);
        coverage.clipped += count != range.count;
        size = std::max(size, range.binding + count);
    }
    result.descriptors.resize(size);
    for (const auto& range : ranges) {
        if (range.type == descriptor_type::sampler || range.count == UINT32_MAX || range.count == 0 || range.binding >= maxEntries)
            continue;
        const uint32_t count = std::min(range.count, maxEntries - range.binding);
        if (!read(range, count, result.descriptors))
            ++coverage.missing;
    }
    return result;
}

template <typename Entries, typename Buffers, typename Handle, typename Snapshot>
void StoreDescriptorTableSnapshot(Entries& entries, Buffers& buffers, uint32_t slot, Handle table, Snapshot&& snapshot) {
    using RootType = decltype(entries[slot].type);
    if (entries.size() <= slot)
        entries.resize(static_cast<size_t>(slot) + 1);
    buffers.push_back(std::move(snapshot.descriptors));
    entries[slot] = { RootType::descriptor_table, static_cast<int32_t>(buffers.size()) - 1, table };
    entries[slot].coverage = snapshot.coverage;
}
}
