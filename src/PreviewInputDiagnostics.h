#pragma once
#include <reshade_api.hpp>
#include <algorithm>
#include <cstddef>

namespace Rendering {
struct PreviewInputScan {
    size_t inspected = 0;
    size_t sampled_views = 0;
    bool truncated = false;
};

// Descriptor payloads are type-dependent. Never treat CBVs, samplers, UAVs or
// unobserved entries as a sampled-image view, even if another field is nonzero.
template <typename Descriptor>
reshade::api::resource_view DiagnosticSampledView(const Descriptor& descriptor) {
    using reshade::api::descriptor_type;
    switch (descriptor.type) {
        case descriptor_type::shader_resource_view: return descriptor.view;
        case descriptor_type::sampler_with_resource_view: return descriptor.sampler_and_view.view;
        default: return reshade::api::resource_view{ 0 };
    }
}

// Shared per-frame budget also bounds repeated unresolved matches. Layout and
// descriptor numbers are tracker coordinates, not shader register numbers.
template <typename State, typename Visitor>
PreviewInputScan ScanPreviewInputs(const State& state, uint32_t stage, size_t& remaining, Visitor visit) {
    PreviewInputScan result;
    constexpr size_t maxLayouts = 64, maxEntries = 256, maxViews = 64;
    const size_t layouts = state.get_root_table_size_at(stage);
    result.truncated = layouts > maxLayouts;
    for (size_t slot = 0; slot < std::min(layouts, maxLayouts); ++slot) {
        const size_t entries = state.get_root_table_entry_size_at(stage, static_cast<uint32_t>(slot));
        result.truncated |= entries > maxEntries;
        for (size_t binding = 0; binding < std::min(entries, maxEntries); ++binding) {
            if (remaining == 0 || result.sampled_views == maxViews) {
                result.truncated = true;
                return result;
            }
            --remaining;
            ++result.inspected;
            const auto* descriptor = state.get_descriptor_at(stage, static_cast<uint32_t>(slot), static_cast<uint32_t>(binding));
            if (descriptor == nullptr)
                continue;
            const auto view = DiagnosticSampledView(*descriptor);
            if (view == 0)
                continue;
            ++result.sampled_views;
            visit(static_cast<uint32_t>(slot), static_cast<uint32_t>(binding), view);
        }
    }
    return result;
}
}
