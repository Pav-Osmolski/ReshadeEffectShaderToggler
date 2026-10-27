#pragma once
#include "PipelinePrivateData.h"
#include "StateTracking.h"

namespace ProducerDiagnostics {
using namespace reshade::api;
inline DeviceDataContainer& Device(command_list* cmd) { return *cmd->get_device()->get_private_data<DeviceDataContainer>(); }
inline CommandListDataContainer& List(command_list* cmd) { return *cmd->get_private_data<CommandListDataContainer>(); }
inline bool Enabled(command_list* cmd) {
    return cmd->get_device()->get_api() == device_api::d3d12 && Device(cmd).traceProducers.load(std::memory_order_relaxed);
}
inline void Invalidate(command_list* cmd, const char* reason) {
    if (Enabled(cmd)) List(cmd).producerTrace.Reset(reason);
}
inline void Forget(command_list* cmd, resource dest, const char* reason) {
    if (Enabled(cmd)) List(cmd).producerTrace.Forget(dest.handle, reason);
}
inline void RecordDraw(command_list* cmd) {
    if (!Enabled(cmd)) return;
    auto& list = List(cmd);
    auto& trace = list.producerTrace;
    trace.Sync(Device(cmd).producerGeneration.load(std::memory_order_relaxed));
    if (!trace.BeginDraw()) return;
    const auto& state = *cmd->get_private_data<state_tracking>();
    for (size_t slot = 0; slot < std::min(state.render_targets.size(), size_t{8}); ++slot) {
        const auto view = state.render_targets[slot];
        if (view != 0) trace.Record(cmd->get_device()->get_resource_from_view(view).handle,
          list.ps.activeShaderHash, list.vs.activeShaderHash, static_cast<uint32_t>(slot));
    }
}
inline void Secondary(command_list* cmd_list, command_list*) { Invalidate(cmd_list, "secondary command list: execution provenance unknown"); }
inline bool copy_resource(reshade::api::command_list *cmd_list, reshade::api::resource source, reshade::api::resource dest) { Forget(cmd_list, dest, "copy_resource: destination history discarded"); return false; }
inline bool copy_texture_region(reshade::api::command_list *cmd_list, reshade::api::resource source, uint32_t source_subresource, const reshade::api::subresource_box *source_box, reshade::api::resource dest, uint32_t dest_subresource, const reshade::api::subresource_box *dest_box, reshade::api::filter_mode filter) { Forget(cmd_list, dest, "copy_texture_region: destination history discarded"); return false; }
inline bool copy_buffer_to_texture(reshade::api::command_list *cmd_list, reshade::api::resource source, uint64_t source_offset, uint32_t row_length, uint32_t slice_height, reshade::api::resource dest, uint32_t dest_subresource, const reshade::api::subresource_box *dest_box) { Forget(cmd_list, dest, "copy_buffer_to_texture: destination history discarded"); return false; }
inline bool resolve_texture_region(reshade::api::command_list *cmd_list, reshade::api::resource source, uint32_t source_subresource, const reshade::api::subresource_box *source_box, reshade::api::resource dest, uint32_t dest_subresource, uint32_t dest_x, uint32_t dest_y, uint32_t dest_z, reshade::api::format format) { Forget(cmd_list, dest, "resolve_texture_region: destination history discarded"); return false; }
inline bool clear_render_target_view(reshade::api::command_list *cmd_list, reshade::api::resource_view rtv, const float color[4], uint32_t rect_count, const reshade::api::rect *rects) { if (Enabled(cmd_list)) Forget(cmd_list, cmd_list->get_device()->get_resource_from_view(rtv), "clear_render_target_view: destination history discarded"); return false; }
inline bool clear_unordered_access_view_uint(reshade::api::command_list *cmd_list, reshade::api::resource_view uav, const uint32_t values[4], uint32_t rect_count, const reshade::api::rect *rects) { if (Enabled(cmd_list)) Forget(cmd_list, cmd_list->get_device()->get_resource_from_view(uav), "clear_unordered_access_view_uint: destination history discarded"); return false; }
inline bool clear_unordered_access_view_float(reshade::api::command_list *cmd_list, reshade::api::resource_view uav, const float values[4], uint32_t rect_count, const reshade::api::rect *rects) { if (Enabled(cmd_list)) Forget(cmd_list, cmd_list->get_device()->get_resource_from_view(uav), "clear_unordered_access_view_float: destination history discarded"); return false; }
}
