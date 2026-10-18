#include "RenderingPreviewManager.h"
#include "RenderingManager.h"
#include "StateTracking.h"
#include "Util.h"
#include "resource.h"
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

void RenderingPreviewManager::RecordVulkanHuntedTarget(command_list* cmd_list, uint32_t stageIndex, uint32_t shaderHash) {
    if (cmd_list == nullptr || cmd_list->get_device() == nullptr || cmd_list->get_device()->get_api() != device_api::vulkan)
        return;

    device* device = cmd_list->get_device();
    DeviceDataContainer& deviceData = *device->get_private_data<DeviceDataContainer>();

    if (deviceData.current_runtime == nullptr || uiData.GetToggleGroupIdShaderEditing() < 0)
        return;

    HuntPreview& preview = deviceData.huntPreview;
    if (preview.vulkan_capture_pending)
        return;

    auto groupIt = uiData.GetToggleGroups().find(uiData.GetToggleGroupIdShaderEditing());
    if (groupIt == uiData.GetToggleGroups().end())
        return;

    ToggleGroup& group = groupIt->second;
    CommandListDataContainer& commandListData = *cmd_list->get_private_data<CommandListDataContainer>();
    const uint64_t previewAction = MATCH_PREVIEW_PS << stageIndex;

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
    cmd_list->get_private_data<state_tracking>()->start_resource_barrier_tracking(activeTarget.resource, resource_usage::render_target);
}

void RenderingPreviewManager::CaptureDeferredVulkanPreview(command_list* cmd_list) {
    if (cmd_list == nullptr || cmd_list->get_device() == nullptr || cmd_list->get_device()->get_api() != device_api::vulkan)
        return;

    device* device = cmd_list->get_device();
    DeviceDataContainer& deviceData = *device->get_private_data<DeviceDataContainer>();
    HuntPreview& preview = deviceData.huntPreview;

    if (!preview.vulkan_capture_pending || preview.target == 0 || preview.vulkan_command_list != cmd_list)
        return;

    // Consume the pending capture exactly once. If preview resources still need to
    // be recreated, CheckPreview does that at present and the next frame retries.
    preview.vulkan_capture_pending = false;
    deviceData.vulkanPreviewWorkPending.store(false, std::memory_order_release);

    state_tracking& trackedState = *cmd_list->get_private_data<state_tracking>();
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
    preview.status = "Captured at safe Vulkan render-pass boundary";
}

void RenderingPreviewManager::CancelDeferredVulkanPreview(device* device) {
    if (device == nullptr)
        return;

    DeviceDataContainer& deviceData = *device->get_private_data<DeviceDataContainer>();
    HuntPreview& preview = deviceData.huntPreview;
    if (preview.tracked_command_list != nullptr && preview.target != 0) {
        preview.tracked_command_list->get_private_data<state_tracking>()->stop_resource_barrier_tracking(preview.target);
        preview.tracked_command_list = nullptr;
    }

    if (preview.vulkan_capture_pending && preview.target != 0 && preview.vulkan_command_list != nullptr) {
        preview.vulkan_command_list->get_private_data<state_tracking>()->stop_resource_barrier_tracking(preview.target);
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
    CommandListDataContainer& commandListData = *cmd_list->get_private_data<CommandListDataContainer>();
    DeviceDataContainer& deviceData = *device->get_private_data<DeviceDataContainer>();

    // Remove call location from queue
    commandListData.commandQueue &= ~(invocation << (callLocation * MATCH_DELIMITER));

    if (deviceData.current_runtime == nullptr || uiData.GetToggleGroupIdShaderEditing() < 0) {
        return;
    }

    if (device->get_api() == device_api::d3d12 && commandListData.d3d12InsideRenderPass) {
        deviceData.huntPreview.status = "Preview unavailable inside a D3D12 render pass";
        return;
    }
    RuntimeDataContainer& runtimeData = *deviceData.current_runtime->get_private_data<RuntimeDataContainer>();

    ToggleGroup& group = uiData.GetToggleGroups().at(uiData.GetToggleGroupIdShaderEditing());

    // Set views during draw call since we can be sure the correct ones are bound at that point
    if (!callLocation && deviceData.huntPreview.target == 0) {
        ResourceViewData active_target;

        if (invocation & MATCH_PREVIEW_PS) {
            active_target = RenderingManager::GetCurrentResourceView(cmd_list, deviceData, &group, commandListData, 0, invocation & MATCH_PREVIEW_PS);
        } else if (invocation & MATCH_PREVIEW_VS) {
            active_target = RenderingManager::GetCurrentResourceView(cmd_list, deviceData, &group, commandListData, 1, invocation & MATCH_PREVIEW_VS);
        } else if (invocation & MATCH_PREVIEW_CS) {
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
                cmd_list->get_private_data<state_tracking>()->start_resource_barrier_tracking(active_target.resource, initialUsage);
                deviceData.huntPreview.tracked_command_list = cmd_list;
            }

            deviceData.huntPreview.target = active_target.resource;
            deviceData.huntPreview.target_desc = desc;
            deviceData.huntPreview.format = desc.texture.format;
            deviceData.huntPreview.view_format = active_target.format;
            deviceData.huntPreview.width = desc.texture.width;
            deviceData.huntPreview.height = desc.texture.height;
        } else {
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
    const resource source = preview.target;
    if (!ResourceManager::IsPreviewTargetSupported(device->get_resource_desc(source))) {
        preview.status = "Preview unavailable: unsupported target topology";
        return;
    }
    if (!resourceManager.IsCompatibleWithPreviewFormat(device, source, preview.view_format)) {
        preview.recreate_preview = true;
        return;
    }
    resource ping = {}, pong = {};
    resource_view pingSrv = {}, pongRtv = {}, pongSrv = {};
    resourceManager.SetPingPreviewHandles(device, &ping, nullptr, &pingSrv);
    resourceManager.SetPongPreviewHandles(device, &pong, &pongRtv, &pongSrv);
    if (pong == 0 || pongSrv == 0 || pongRtv == 0)
        return;

    const bool explicitStates = device->get_api() == device_api::d3d12;
    resource_usage sourceUsage = resource_usage::render_target;
    if (explicitStates) {
        if (preview.tracked_command_list != cmd_list)
            return;
        sourceUsage = cmd_list->get_private_data<state_tracking>()->stop_resource_barrier_tracking(source);
        preview.tracked_command_list = nullptr;
        if (sourceUsage == resource_usage::undefined)
            return;
    }
    const bool clearAlpha = group.getClearPreviewAlpha();
    const resource destination = clearAlpha ? ping : pong;
    if (destination == 0 || (clearAlpha && pingSrv == 0))
        return;
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
        cmd_list->get_private_data<state_tracking>()->apply(cmd_list);
    }
    preview.matched = copied;
    preview.status = copied ? "Captured preview" : "Preview unavailable: unsupported copy pipeline";
}
