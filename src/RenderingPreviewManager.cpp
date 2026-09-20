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
    DeviceDataContainer& deviceData = device->get_private_data<DeviceDataContainer>();

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
    preview.status = "Captured at safe Vulkan render-pass boundary";
}

void RenderingPreviewManager::CancelDeferredVulkanPreview(device* device) {
    if (device == nullptr || device->get_api() != device_api::vulkan)
        return;

    DeviceDataContainer& deviceData = device->get_private_data<DeviceDataContainer>();
    HuntPreview& preview = deviceData.huntPreview;

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

    // Remove call location from queue
    commandListData.commandQueue &= ~(invocation << (callLocation * MATCH_DELIMITER));

    if (deviceData.current_runtime == nullptr || uiData.GetToggleGroupIdShaderEditing() < 0) {
        return;
    }

    RuntimeDataContainer& runtimeData = deviceData.current_runtime->get_private_data<RuntimeDataContainer>();

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
            // cmd_list->get_private_data<state_tracking>().start_resource_barrier_tracking(res, resource_usage::render_target);

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

    if (group.getId() == uiData.GetToggleGroupIdShaderEditing() && !deviceData.huntPreview.matched) {
        resource rs = deviceData.huntPreview.target;
        // resource_usage rs_usage = cmd_list->get_private_data<state_tracking>().stop_resource_barrier_tracking(rs);
        // if (rs_usage == resource_usage::undefined)
        //{
        //     return;
        // }

        if (!resourceManager.IsCompatibleWithPreviewFormat(device, rs, deviceData.huntPreview.view_format)) {
            deviceData.huntPreview.recreate_preview = true;
        } else {
            bool supportsAlphaClear = device->get_api() == device_api::d3d9 || device->get_api() == device_api::d3d10 ||
                                      device->get_api() == device_api::d3d11 || device->get_api() == device_api::d3d12;

            resource previewResPing = resource{ 0 };
            resource previewResPong = resource{ 0 };
            resource_view preview_pong_rtv = resource_view{ 0 };
            resource_view preview_ping_srv = resource_view{ 0 };

            resourceManager.SetPingPreviewHandles(device, &previewResPing, nullptr, &preview_ping_srv);
            resourceManager.SetPongPreviewHandles(device, &previewResPong, &preview_pong_rtv, nullptr);

            if (previewResPong != 0 && (!group.getClearPreviewAlpha() || !supportsAlphaClear)) {
                // resource resources[2] = { rs, previewResPong };
                // resource_usage from[2] = { rs_usage, resource_usage::shader_resource };
                // resource_usage to[2] = { resource_usage::copy_source, resource_usage::copy_dest };

                // cmd_list->barrier(2, resources, from, to);
                cmd_list->copy_resource(rs, previewResPong);
                // cmd_list->barrier(2, resources, to, from);
            } else if (previewResPing != 0 && previewResPong != 0 && preview_ping_srv != 0 && preview_pong_rtv != 0 && supportsAlphaClear) {
                // resource resources[2] = { rs, previewResPing };
                // resource_usage from[2] = { rs_usage, resource_usage::shader_resource };
                // resource_usage to[2] = { resource_usage::copy_source, resource_usage::copy_dest };

                // cmd_list->barrier(2, resources, from, to);
                cmd_list->copy_resource(rs, previewResPing);
                // cmd_list->barrier(2, resources, to, from);

                // cmd_list->barrier(previewResPong, resource_usage::shader_resource, resource_usage::render_target);
                shaderManager.CopyResource(cmd_list, preview_ping_srv, preview_pong_rtv, deviceData.huntPreview.width, deviceData.huntPreview.height);
                // cmd_list->barrier(previewResPong, resource_usage::render_target, resource_usage::shader_resource);
            }

            if (group.getFlipBuffer() && runtimeData.specialEffects[REST_FLIP].technique != 0) {
                deviceData.current_runtime->render_technique(runtimeData.specialEffects[REST_FLIP].technique, cmd_list, preview_pong_rtv, preview_pong_rtv);
            }

            if (group.getToneMap() && runtimeData.specialEffects[REST_TONEMAP_TO_SDR].technique != 0) {
                deviceData.current_runtime->render_technique(
                  runtimeData.specialEffects[REST_TONEMAP_TO_SDR].technique, cmd_list, preview_pong_rtv, preview_pong_rtv);
            }
        }

        deviceData.huntPreview.matched = true;
    }
}