#include "RenderingPreviewManager.h"
#include "RenderingManager.h"
#include "StateTracking.h"
#include "Util.h"
#include "resource.h"
#include "PreviewInputDiagnostics.h"
#include <d3d12.h>

using namespace Rendering;
using namespace ShaderToggler;
using namespace reshade::api;
using namespace std;

RenderingPreviewManager::RenderingPreviewManager(AddonImGui::AddonUIData& data, ResourceManager& rManager, RenderingShaderManager& shManager)
  : uiData(data)
  , resourceManager(rManager)
  , shaderManager(shManager) {}

RenderingPreviewManager::~RenderingPreviewManager() {}

// Snapshot the selected draw, not the subsequently bound pipeline at a deferred
// capture boundary. This also records null slots and alternate colour targets.
static void RecordPreviewDiagnostics(command_list* cmd_list, HuntPreview& preview,
                                     ToggleGroup& group, uint32_t stage, uint32_t hash, bool trackDescriptors) {
    device* device = cmd_list->get_device();
    preview.hunted_stage = stage;
    preview.hunted_shader_hash = hash;
    preview.target_selection = group.isAutoSceneColourActive(device->get_api()) ? "Auto Scene Colour / RTV 0" :
      group.getRenderToResourceViews() ? "Manual resource view" : std::format("Manual RTV {}", group.getRenderTargetIndex());
    preview.capture_timing.clear();
    preview.capture_path.clear();
    preview.bound_targets.clear();
    const auto& targets = cmd_list->get_private_data<state_tracking>().render_targets;
    for (size_t slot = 0; slot < targets.size(); ++slot) {
        const auto view = targets[slot];
        const auto res = view != 0 ? device->get_resource_from_view(view) : resource{ 0 };
        if (res == 0) {
            preview.bound_targets += std::format("RTV {}: null\n", slot);
            continue;
        }
        const auto desc = device->get_resource_desc(res);
        preview.bound_targets += std::format("RTV {}: {}x{} | storage {} | view {} | samples {} | 0x{:x}\n",
          slot, desc.texture.width, desc.texture.height, RenderingManager::FormatName(desc.texture.format),
          RenderingManager::FormatName(device->get_resource_view_desc(view).format),
          desc.texture.samples, res.handle);
    }
    if (targets.empty())
        preview.bound_targets = "(no colour render targets tracked)\n";

    preview.bound_inputs.clear();
    if (!trackDescriptors) {
        preview.bound_inputs = "Descriptor tracking disabled. Enable Track descriptors in REST Options and restart the game.\n";
        return;
    }
    const auto& state = cmd_list->get_private_data<state_tracking>();
    const auto scan = ScanPreviewInputs(state, stage, preview.input_scan_remaining,
      [&](uint32_t slot, uint32_t binding, resource_view view) {
        const auto res = device->get_resource_from_view(view);
        if (res == 0)
            return;
        const auto desc = device->get_resource_desc(res);
        if (desc.type != resource_type::texture_1d && desc.type != resource_type::texture_2d && desc.type != resource_type::texture_3d)
            return;
        const char* type = desc.type == resource_type::texture_1d ? "1D" : desc.type == resource_type::texture_2d ? "2D" : "3D";
        preview.bound_inputs += std::format(
          "Layout {} / descriptor {}: {} {}x{} | depth/layers {} | mips {} | samples {} | storage {} | view {} | usage 0x{:x} | resource 0x{:x}\n",
          slot, binding, type, desc.texture.width, desc.texture.height, desc.texture.depth_or_layers,
          desc.texture.levels, desc.texture.samples, RenderingManager::FormatName(desc.texture.format),
          RenderingManager::FormatName(device->get_resource_view_desc(view).format), static_cast<uint32_t>(desc.usage), res.handle);
      });
    if (preview.bound_inputs.empty())
        preview.bound_inputs = "(no sampled textures found in the inspected tracked descriptors)\n";
    preview.bound_inputs += std::format("Inspected descriptors: {}; sampled views: {}; scan truncated: {}\n",
      scan.inspected, scan.sampled_views, scan.truncated);
    preview.bound_inputs += "Inputs describe the selected draw, before any deferred output capture.\n"
      "Layout/descriptor coordinates are not HLSL register numbers. UAVs and buffer views are not listed.\n"
      "Read-only metadata: input pixels are not inspected, copied or modified; scene/UI roles are not inferred.\n";
}

void RenderingPreviewManager::RecordVulkanHuntedTarget(command_list* cmd_list, uint32_t stageIndex, uint32_t shaderHash) {
    if (cmd_list == nullptr || cmd_list->get_device() == nullptr || cmd_list->get_device()->get_api() != device_api::vulkan)
        return;

    device* device = cmd_list->get_device();
    DeviceDataContainer& deviceData = device->get_private_data<DeviceDataContainer>();
    std::lock_guard previewLock(deviceData.previewMutex);

    if (deviceData.current_runtime == nullptr || uiData.GetToggleGroupIdShaderEditing() < 0)
        return;

    HuntPreview& preview = deviceData.huntPreview;
    if (preview.vulkan_capture_pending)
        return;

    auto groupIt = uiData.GetToggleGroups().find(uiData.GetToggleGroupIdShaderEditing());
    if (groupIt == uiData.GetToggleGroups().end())
        return;

    ToggleGroup& group = groupIt->second;
    CommandListDataContainer& commandListData = cmd_list->get_private_data<CommandListDataContainer>();
    const uint64_t previewAction = MATCH_PREVIEW_PS << stageIndex;
    RecordPreviewDiagnostics(cmd_list, preview, group, stageIndex, shaderHash, uiData.GetTrackDescriptors());

    const ResourceViewData activeTarget =
      RenderingManager::GetCurrentResourceView(cmd_list, deviceData, &group, commandListData, stageIndex, previewAction);

    if (activeTarget.resource == 0) {
        preview.status = "Preview unavailable: no render target resolved for hunted draw";
        preview.target = resource{ 0 };
        preview.matched = false;
        return;
    }

    const resource_desc desc = device->get_resource_desc(activeTarget.resource);

    preview.target = activeTarget.resource;
    preview.target_desc = desc;
    preview.format = desc.texture.format;
    preview.view_format = activeTarget.format != format::unknown ? activeTarget.format : desc.texture.format;
    preview.width = desc.texture.width;
    preview.height = desc.texture.height;
    preview.hunted_shader_hash = shaderHash;
    preview.hunted_stage = stageIndex;
    preview.vulkan_command_list = cmd_list;
    preview.vulkan_capture_pending = true;
    deviceData.vulkanPreviewWorkPending.store(true, std::memory_order_release);
    preview.matched = false;
    preview.status = "Waiting for safe Vulkan preview boundary...";

    if (!resourceManager.IsCompatibleWithPreviewFormat(device, activeTarget.resource, preview.view_format))
        preview.recreate_preview = true;

    // Track explicit resource barriers between the suppressed draw and the next
    // render-pass boundary so the preview copy can restore the best-known usage.
    cmd_list->get_private_data<state_tracking>().start_resource_barrier_tracking(activeTarget.resource, resource_usage::render_target);
}

void RenderingPreviewManager::CaptureDeferredVulkanPreview(command_list* cmd_list) {
    if (cmd_list == nullptr || cmd_list->get_device() == nullptr || cmd_list->get_device()->get_api() != device_api::vulkan)
        return;

    device* device = cmd_list->get_device();
    DeviceDataContainer& deviceData = device->get_private_data<DeviceDataContainer>();
    std::lock_guard previewLock(deviceData.previewMutex);
    HuntPreview& preview = deviceData.huntPreview;

    if (!preview.vulkan_capture_pending || preview.target == 0 || preview.vulkan_command_list != cmd_list)
        return;

    // Consume the pending capture exactly once. If preview resources still need to
    // be recreated, CheckPreview does that at present and the next frame retries.
    preview.vulkan_capture_pending = false;
    deviceData.vulkanPreviewWorkPending.store(false, std::memory_order_release);

    state_tracking& trackedState = cmd_list->get_private_data<state_tracking>();
    resource_usage sourceUsage = trackedState.stop_resource_barrier_tracking(preview.target);
    if (sourceUsage == resource_usage::undefined)
        sourceUsage = resource_usage::render_target;

    const resource_desc desc = device->get_resource_desc(preview.target);

    if (desc.texture.samples != 1) {
        preview.status = "Preview unavailable: multisampled target";
        return;
    }

    if (static_cast<uint32_t>(desc.usage & resource_usage::copy_source) == 0) {
        preview.status = "Preview unavailable: target lacks transfer-source usage";
        return;
    }

    if (!device->check_format_support(desc.texture.format, resource_usage::copy_source)) {
        preview.status = "Preview unavailable: format cannot be copied";
        return;
    }

    if (!resourceManager.IsCompatibleWithPreviewFormat(device, preview.target, preview.view_format)) {
        preview.recreate_preview = true;
        preview.status = "Preparing Vulkan preview resource...";
        return;
    }

    resource previewPong = resource{ 0 };
    resource_view previewSrv = resource_view{ 0 };
    resourceManager.SetPongPreviewHandles(device, &previewPong, nullptr, &previewSrv);

    if (previewPong == 0 || previewSrv == 0) {
        preview.recreate_preview = true;
        preview.status = "Preparing Vulkan preview resource...";
        return;
    }

    if (sourceUsage != resource_usage::copy_source)
        cmd_list->barrier(preview.target, sourceUsage, resource_usage::copy_source);

    cmd_list->barrier(previewPong, resource_usage::shader_resource, resource_usage::copy_dest);
    cmd_list->copy_resource(preview.target, previewPong);
    cmd_list->barrier(previewPong, resource_usage::copy_dest, resource_usage::shader_resource);

    if (sourceUsage != resource_usage::copy_source)
        cmd_list->barrier(preview.target, resource_usage::copy_source, sourceUsage);

    preview.matched = true;
    preview.capture_timing = "Safe Vulkan render-pass boundary after suppressed draw";
    preview.capture_path = "Direct resource copy";
    preview.status = "Capture recorded (GPU pixels not verified)";
}

void RenderingPreviewManager::CancelDeferredVulkanPreview(device* device) {
    if (device == nullptr)
        return;

    DeviceDataContainer& deviceData = device->get_private_data<DeviceDataContainer>();
    std::lock_guard previewLock(deviceData.previewMutex);
    HuntPreview& preview = deviceData.huntPreview;
    if (preview.tracked_command_list != nullptr && preview.target != 0) {
        preview.tracked_command_list->get_private_data<state_tracking>().stop_resource_barrier_tracking(preview.target);
        preview.tracked_command_list = nullptr;
    }

    if (preview.vulkan_capture_pending && preview.target != 0 && preview.vulkan_command_list != nullptr) {
        preview.vulkan_command_list->get_private_data<state_tracking>().stop_resource_barrier_tracking(preview.target);
    }

    preview.vulkan_capture_pending = false;
    deviceData.vulkanPreviewWorkPending.store(false, std::memory_order_release);
    preview.vulkan_command_list = nullptr;
}

void RenderingPreviewManager::UpdatePreview(command_list* cmd_list, uint64_t callLocation, uint64_t invocation) {
    if (cmd_list == nullptr || cmd_list->get_device() == nullptr) {
        return;
    }

    device* device = cmd_list->get_device();
    CommandListDataContainer& commandListData = cmd_list->get_private_data<CommandListDataContainer>();
    DeviceDataContainer& deviceData = device->get_private_data<DeviceDataContainer>();
    std::lock_guard previewLock(deviceData.previewMutex);

    // The caller can pass several stage bits; only the queued stages matched.
    const uint64_t queuedPreview = (commandListData.commandQueue >> (callLocation * MATCH_DELIMITER)) & invocation & MATCH_PREVIEW;
    // Remove call location from queue
    commandListData.commandQueue &= ~(invocation << (callLocation * MATCH_DELIMITER));

    if (deviceData.current_runtime == nullptr || uiData.GetToggleGroupIdShaderEditing() < 0) {
        return;
    }

    RuntimeDataContainer& runtimeData = deviceData.current_runtime->get_private_data<RuntimeDataContainer>();

    ToggleGroup& group = uiData.GetToggleGroups().at(uiData.GetToggleGroupIdShaderEditing());

    // Set views during draw call since we can be sure the correct ones are bound at that point
    if (!callLocation && deviceData.huntPreview.target == 0) {
        if (queuedPreview == 0)
            return;
        const uint32_t stage = (queuedPreview & MATCH_PREVIEW_PS) ? 0 : (queuedPreview & MATCH_PREVIEW_VS) ? 1 : 2;
        const uint32_t hash = stage == 0 ? commandListData.ps.activeShaderHash :
                              stage == 1 ? commandListData.vs.activeShaderHash : commandListData.cs.activeShaderHash;
        RecordPreviewDiagnostics(cmd_list, deviceData.huntPreview, group, stage, hash, uiData.GetTrackDescriptors());
        if (device->get_api() == device_api::d3d12 && commandListData.d3d12InsideRenderPass) {
            deviceData.huntPreview.status = "Preview unavailable inside a D3D12 render pass";
            return;
        }
        ResourceViewData active_target;

        if (queuedPreview & MATCH_PREVIEW_PS) {
            active_target = RenderingManager::GetCurrentResourceView(cmd_list, deviceData, &group, commandListData, 0, invocation & MATCH_PREVIEW_PS);
        } else if (queuedPreview & MATCH_PREVIEW_VS) {
            active_target = RenderingManager::GetCurrentResourceView(cmd_list, deviceData, &group, commandListData, 1, invocation & MATCH_PREVIEW_VS);
        } else if (queuedPreview & MATCH_PREVIEW_CS) {
            active_target = RenderingManager::GetCurrentResourceView(cmd_list, deviceData, &group, commandListData, 2, invocation & MATCH_PREVIEW_CS);
        }

        if (active_target.resource != 0) {
            resource_desc desc = device->get_resource_desc(active_target.resource);
            if (!ResourceManager::IsPreviewTargetSupported(desc)) {
                deviceData.huntPreview.status = "Preview unavailable: unsupported target topology";
                return;
            }
            if (device->get_api() == device_api::d3d12) {
                const resource_usage initialUsage = !group.isAutoSceneColourActive(device_api::d3d12) && group.getRenderToResourceViews()
                    ? resource_usage::shader_resource : resource_usage::render_target;
                cmd_list->get_private_data<state_tracking>().start_resource_barrier_tracking(active_target.resource, initialUsage);
                deviceData.huntPreview.tracked_command_list = cmd_list;
            }

            deviceData.huntPreview.target = active_target.resource;
            deviceData.huntPreview.target_desc = desc;
            deviceData.huntPreview.format = desc.texture.format;
            deviceData.huntPreview.view_format = active_target.format;
            deviceData.huntPreview.width = desc.texture.width;
            deviceData.huntPreview.height = desc.texture.height;
        } else {
            deviceData.huntPreview.status = "Preview unavailable: no eligible target resolved for selected shader";
            return;
        }
    }

    if (deviceData.huntPreview.target == 0 ||
        !(!callLocation && !deviceData.huntPreview.target_invocation_location || callLocation & deviceData.huntPreview.target_invocation_location)) {
        return;
    }

    HuntPreview& preview = deviceData.huntPreview;
    if (preview.matched)
        return;
    if (device->get_api() == device_api::d3d12 && commandListData.d3d12InsideRenderPass) {
        preview.status = "Preview unavailable inside a D3D12 render pass";
        return;
    }
    const resource source = preview.target;
    if (!ResourceManager::IsPreviewTargetSupported(device->get_resource_desc(source))) {
        preview.status = "Preview unavailable: unsupported target topology";
        return;
    }
    if (!resourceManager.IsCompatibleWithPreviewFormat(device, source, preview.view_format)) {
        preview.recreate_preview = true;
        preview.status = "Preparing preview resource for selected view format...";
        return;
    }
    resource ping = {}, pong = {};
    resource_view pingSrv = {}, pongRtv = {}, pongSrv = {};
    resourceManager.SetPingPreviewHandles(device, &ping, nullptr, &pingSrv);
    resourceManager.SetPongPreviewHandles(device, &pong, &pongRtv, &pongSrv);
    if (pong == 0 || pongSrv == 0 || pongRtv == 0) {
        preview.status = "Preview unavailable: missing preview resource or view (see ReShade.log)";
        return;
    }

    const bool explicitStates = device->get_api() == device_api::d3d12;
    resource_usage sourceUsage = resource_usage::render_target;
    if (explicitStates) {
        if (preview.tracked_command_list != cmd_list) {
            preview.status = "Preview unavailable: capture boundary is on another command list";
            return;
        }
        sourceUsage = cmd_list->get_private_data<state_tracking>().stop_resource_barrier_tracking(source);
        preview.tracked_command_list = nullptr;
        if (sourceUsage == resource_usage::undefined) {
            preview.status = "Preview unavailable: source resource state is unknown";
            return;
        }
    }
    const bool clearAlpha = group.getClearPreviewAlpha();
    const resource destination = clearAlpha ? ping : pong;
    if (destination == 0 || (clearAlpha && pingSrv == 0)) {
        preview.status = "Preview unavailable: missing alpha-copy resource or view";
        return;
    }
    preview.capture_timing = callLocation == CALL_DRAW ? "Before draw / dispatch" :
      callLocation == CALL_BIND_PIPELINE ? "Next pipeline bind" : "Next render-target bind";
    preview.capture_path = clearAlpha ? "Resource copy + alpha-clearing shader copy" : "Direct resource copy";
    if (explicitStates) {
        cmd_list->barrier(source, sourceUsage, resource_usage::copy_source);
        cmd_list->barrier(destination, resource_usage::shader_resource, resource_usage::copy_dest);
    }
    cmd_list->copy_resource(source, destination);
    if (explicitStates) {
        cmd_list->barrier(destination, resource_usage::copy_dest, resource_usage::shader_resource);
        cmd_list->barrier(source, resource_usage::copy_source, sourceUsage);
        cmd_list->barrier(pong, resource_usage::shader_resource, resource_usage::render_target);
    }
    bool copied = true;
    if (clearAlpha)
        copied = shaderManager.CopyResource(cmd_list, pingSrv, pongRtv, preview.width, preview.height);
    if (copied && group.getFlipBuffer() && runtimeData.specialEffects[REST_FLIP].technique != 0)
        deviceData.current_runtime->render_technique(runtimeData.specialEffects[REST_FLIP].technique, cmd_list, pongRtv, pongRtv);
    if (copied && group.getToneMap() && runtimeData.specialEffects[REST_TONEMAP_TO_SDR].technique != 0)
        deviceData.current_runtime->render_technique(runtimeData.specialEffects[REST_TONEMAP_TO_SDR].technique, cmd_list, pongRtv, pongRtv);
    if (explicitStates) {
        cmd_list->barrier(pong, resource_usage::render_target, resource_usage::shader_resource);
        cmd_list->get_private_data<state_tracking>().apply(cmd_list);
    }
    preview.matched = copied;
    preview.status = copied ? "Capture recorded (GPU pixels not verified)" : "Preview unavailable: unsupported copy pipeline";
}
