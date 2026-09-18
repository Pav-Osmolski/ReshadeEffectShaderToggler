#include "RenderingManager.h"
#include "PipelinePrivateData.h"

using namespace Rendering;
using namespace ShaderToggler;
using namespace reshade::api;
using namespace std;

size_t RenderingManager::g_charBufferSize = CHAR_BUFFER_SIZE;
char RenderingManager::g_charBuffer[CHAR_BUFFER_SIZE];

void RenderingManager::EnumerateTechniques(effect_runtime* runtime, function<void(effect_runtime*, effect_technique, string&, string&)> func) {
    runtime->enumerate_techniques(nullptr, [func](effect_runtime* rt, effect_technique technique) {
        g_charBufferSize = CHAR_BUFFER_SIZE;
        rt->get_technique_name(technique, g_charBuffer, &g_charBufferSize);
        string name(g_charBuffer);

        g_charBufferSize = CHAR_BUFFER_SIZE;
        rt->get_technique_effect_name(technique, g_charBuffer, &g_charBufferSize);
        string eff_name(g_charBuffer);

        func(rt, technique, name, eff_name);
    });
}

bool RenderingManager::IsColorBuffer(reshade::api::format value) {
    switch (value) {
        default:
            return false;
        case reshade::api::format::b5g6r5_unorm:
        case reshade::api::format::b5g5r5a1_unorm:
        case reshade::api::format::b5g5r5x1_unorm:
        case reshade::api::format::r8g8b8a8_typeless:
        case reshade::api::format::r8g8b8a8_unorm:
        case reshade::api::format::r8g8b8a8_unorm_srgb:
        case reshade::api::format::r8g8b8x8_unorm:
        case reshade::api::format::r8g8b8x8_unorm_srgb:
        case reshade::api::format::b8g8r8a8_typeless:
        case reshade::api::format::b8g8r8a8_unorm:
        case reshade::api::format::b8g8r8a8_unorm_srgb:
        case reshade::api::format::b8g8r8x8_typeless:
        case reshade::api::format::b8g8r8x8_unorm:
        case reshade::api::format::b8g8r8x8_unorm_srgb:
        case reshade::api::format::r10g10b10a2_typeless:
        case reshade::api::format::r10g10b10a2_unorm:
        case reshade::api::format::r10g10b10a2_xr_bias:
        case reshade::api::format::b10g10r10a2_typeless:
        case reshade::api::format::b10g10r10a2_unorm:
        case reshade::api::format::r11g11b10_float:
        case reshade::api::format::r16g16b16a16_typeless:
        case reshade::api::format::r16g16b16a16_float:
        case reshade::api::format::r16g16b16a16_unorm:
        case reshade::api::format::r32g32b32_typeless:
        case reshade::api::format::r32g32b32_float:
        case reshade::api::format::r32g32b32a32_typeless:
        case reshade::api::format::r32g32b32a32_float:
            return true;
    }
}

// Checks whether the aspect ratio of the two sets of dimensions is similar or not, stolen from ReShade's generic_depth addon
bool RenderingManager::check_aspect_ratio(float width_to_check, float height_to_check, uint32_t width, uint32_t height, uint32_t matchingMode) {
    if (width_to_check == 0.0f || height_to_check == 0.0f)
        return true;

    const float w = static_cast<float>(width);
    float w_ratio = w / width_to_check;
    const float h = static_cast<float>(height);
    float h_ratio = h / height_to_check;
    const float aspect_ratio = (w / h) - (width_to_check / height_to_check);

    // Accept if dimensions are similar in value or almost exact multiples
    return std::fabs(aspect_ratio) <= 0.1f && ((w_ratio <= 1.85f && w_ratio >= 0.5f && h_ratio <= 1.85f && h_ratio >= 0.5f) ||
                                               (matchingMode == ShaderToggler::SWAPCHAIN_MATCH_MODE_EXTENDED_ASPECT_RATIO &&
                                                std::modf(w_ratio, &w_ratio) <= 0.02f && std::modf(h_ratio, &h_ratio) <= 0.02f));
}

void RenderingManager::CycleDescriptors(ShaderToggler::ToggleGroup* group,
                                        state_tracking& state,
                                        const descriptor_tracking::descriptor_data* buf,
                                        uint32_t stageIndex,
                                        int32_t slot,
                                        int32_t& desc,
                                        int32_t& desc_size,
                                        std::function<void(uint32_t)> descSetter) {
    DescriptorCycle cycle = group->consumeSRVCycle();
    if (cycle != CYCLE_NONE) {
        if (cycle == CYCLE_UP) {
            desc = std::min(++desc, desc_size - 1);
            buf = state.get_descriptor_at(stageIndex, slot, desc);

            while ((buf == nullptr || buf->view == 0) && desc < desc_size - 2) {
                buf = state.get_descriptor_at(stageIndex, slot, ++desc);
            }
        } else {
            desc = desc > 0 ? --desc : 0;
            buf = state.get_descriptor_at(stageIndex, slot, desc);

            while ((buf == nullptr || buf->view == 0) && desc > 0) {
                buf = state.get_descriptor_at(stageIndex, slot, --desc);
            }
        }

        if (buf != nullptr && buf->view != 0) {
            group->setBindingSRVDescriptorIndex(desc);
        }
    }
}

bool RenderingManager::ValidFormat(effect_runtime* runtime, const resource_desc& desc, uint32_t swapChainMatchType, bool checkColorFormat) {
    if (checkColorFormat && !IsColorBuffer(desc.texture.format)) {
        return false;
    }

    // Make sure our target matches swap buffer dimensions when applying effects or it's explicitly requested
    if (swapChainMatchType < ShaderToggler::SWAPCHAIN_MATCH_MODE_NONE) {
        uint32_t width, height;
        runtime->get_screenshot_width_and_height(&width, &height);

        if ((swapChainMatchType >= ShaderToggler::SWAPCHAIN_MATCH_MODE_ASPECT_RATIO &&
             !check_aspect_ratio(static_cast<float>(desc.texture.width), static_cast<float>(desc.texture.height), width, height, swapChainMatchType)) ||
            (swapChainMatchType == ShaderToggler::SWAPCHAIN_MATCH_MODE_RESOLUTION && (width != desc.texture.width || height != desc.texture.height))) {
            return false;
        }
    }

    return true;
}

const ResourceViewData RenderingManager::FindAutoRenderResourceView(command_list* cmd_list,
                                                                    DeviceDataContainer& deviceData,
                                                                    ToggleGroup* group) {
    ResourceViewData best;
    if (cmd_list == nullptr || group == nullptr || deviceData.current_runtime == nullptr)
        return best;

    device* device = cmd_list->get_device();
    if (device == nullptr)
        return best;

    state_tracking& state = cmd_list->get_private_data<state_tracking>();
    uint32_t frameWidth = 0, frameHeight = 0;
    deviceData.current_runtime->get_screenshot_width_and_height(&frameWidth, &frameHeight);

    int32_t bestScore = INT32_MIN;
    uint32_t bestStage = 0, bestSlot = 0, bestDescriptor = 0;
    resource_desc bestDesc = {};
    format bestFormat = format::unknown;

    auto formatScore = [](format value) -> int32_t {
        switch (format_to_default_typed(value, 0)) {
            case format::r16g16b16a16_float:
                return 6000;
            case format::r11g11b10_float:
                return 5500;
            case format::r32g32b32_float:
            case format::r32g32b32a32_float:
                return 5000;
            case format::r10g10b10a2_unorm:
            case format::b10g10r10a2_unorm:
                return 2500;
            case format::r16g16b16a16_unorm:
                return 1800;
            default:
                return 500;
        }
    };

    const uint32_t matchMode = group->getMatchSwapchainResolution();

    for (uint32_t stage = 0; stage < 3; ++stage) {
        const size_t slotCount = state.get_root_table_size_at(stage);
        for (uint32_t slot = 0; slot < slotCount; ++slot) {
            const size_t descriptorCount = state.get_root_table_entry_size_at(stage, slot);
            for (uint32_t descriptor = 0; descriptor < descriptorCount; ++descriptor) {
                const descriptor_tracking::descriptor_data* data = state.get_descriptor_at(stage, slot, descriptor);
                if (data == nullptr || data->view == 0)
                    continue;

                resource candidate = device->get_resource_from_view(data->view);
                if (candidate == 0)
                    continue;

                const resource_desc desc = device->get_resource_desc(candidate);
                if (desc.type != resource_type::texture_2d)
                    continue;

                const resource_view_desc viewDesc = device->get_resource_view_desc(data->view);
                if (!IsColorBuffer(viewDesc.format) && !IsColorBuffer(desc.texture.format))
                    continue;

                if (!static_cast<uint32_t>(desc.usage & resource_usage::render_target))
                    continue;

                const bool exactResolution = frameWidth != 0 && frameHeight != 0 && desc.texture.width == frameWidth && desc.texture.height == frameHeight;
                const bool aspectCompatible =
                  frameWidth != 0 && frameHeight != 0 &&
                  check_aspect_ratio(static_cast<float>(desc.texture.width),
                                     static_cast<float>(desc.texture.height),
                                     frameWidth,
                                     frameHeight,
                                     matchMode == SWAPCHAIN_MATCH_MODE_EXTENDED_ASPECT_RATIO ? matchMode : SWAPCHAIN_MATCH_MODE_ASPECT_RATIO);

                if (matchMode == SWAPCHAIN_MATCH_MODE_RESOLUTION && !exactResolution)
                    continue;
                if ((matchMode == SWAPCHAIN_MATCH_MODE_ASPECT_RATIO || matchMode == SWAPCHAIN_MATCH_MODE_EXTENDED_ASPECT_RATIO) && !aspectCompatible)
                    continue;

                int32_t score = 0;

                if (exactResolution) {
                    score += 100000;
                } else if (aspectCompatible) {
                    score += 15000;

                    const double candidatePixels = static_cast<double>(desc.texture.width) * static_cast<double>(desc.texture.height);
                    const double framePixels = static_cast<double>(frameWidth) * static_cast<double>(frameHeight);
                    if (framePixels > 0.0) {
                        const double ratio = std::min(candidatePixels, framePixels) / std::max(candidatePixels, framePixels);
                        score += static_cast<int32_t>(ratio * 10000.0);
                    }
                }

                score += formatScore(viewDesc.format != format::unknown ? viewDesc.format : desc.texture.format);

                if (stage == 0)
                    score += 1500;
                else if (stage == 2)
                    score += 250;

                if (static_cast<uint32_t>(desc.usage & resource_usage::shader_resource))
                    score += 500;

                if (group->hasAutoRenderSRVSelection() &&
                    group->getAutoRenderSRVSelectedStage() == stage &&
                    group->getAutoRenderSRVSelectedSlot() == slot &&
                    group->getAutoRenderSRVSelectedDescriptor() == descriptor) {
                    score += 750;
                }

                if (score <= bestScore)
                    continue;

                bestScore = score;
                best = ResourceViewData(candidate, viewDesc.format);
                bestStage = stage;
                bestSlot = slot;
                bestDescriptor = descriptor;
                bestDesc = desc;
                bestFormat = viewDesc.format;
            }
        }
    }

    if (best.resource != 0) {
        group->setAutoRenderSRVSelection(bestStage,
                                         bestSlot,
                                         bestDescriptor,
                                         bestDesc.texture.width,
                                         bestDesc.texture.height,
                                         bestFormat,
                                         bestScore);
    } else {
        group->clearAutoRenderSRVSelection();
    }

    return best;
}

const ResourceViewData RenderingManager::GetCurrentResourceView(command_list* cmd_list,
                                                                DeviceDataContainer& deviceData,
                                                                ToggleGroup* group,
                                                                CommandListDataContainer& commandListData,
                                                                uint32_t descIndex,
                                                                uint64_t action) {
    ResourceViewData active_data;

    if (deviceData.current_runtime == nullptr) {
        return active_data;
    }

    device* device = deviceData.current_runtime->get_device();

    state_tracking& state = cmd_list->get_private_data<state_tracking>();
    const vector<resource_view>& rtvs = state.render_targets;

    size_t index = group->getRenderTargetIndex();
    size_t bindingRTindex = group->getBindingRenderTargetIndex();
    if (!rtvs.empty()) {
        index = std::min(index, rtvs.size() - 1);
        bindingRTindex = std::min(bindingRTindex, rtvs.size() - 1);
    } else {
        index = 0;
        bindingRTindex = 0;
    }

    // Only return SRVs in case of bindings
    if (action & MATCH_BINDING && group->getExtractResourceViews()) {
        uint32_t stageIndex = std::min(static_cast<uint32_t>(2), group->getSRVShaderStage());

        int32_t slot_size = static_cast<int32_t>(state.get_root_table_size_at(stageIndex));
        int32_t slot = std::min(static_cast<int32_t>(group->getBindingSRVSlotIndex()), slot_size - 1);

        if (slot_size <= 0)
            return active_data;

        int32_t desc_size = static_cast<uint32_t>(state.get_root_table_entry_size_at(stageIndex, slot));
        int32_t desc = std::min(static_cast<int32_t>(group->getBindingSRVDescriptorIndex()), desc_size - 1);

        if (desc_size <= 0)
            return active_data;

        const descriptor_tracking::descriptor_data* buf = state.get_descriptor_at(stageIndex, slot, desc);

        CycleDescriptors(group, state, buf, stageIndex, slot, desc, desc_size, [group](uint32_t idx) { group->setBindingSRVDescriptorIndex(idx); });

        if (buf != nullptr && buf->view != 0) {
            active_data.resource = device->get_resource_from_view(buf->view);
            active_data.format = device->get_resource_view_desc(buf->view).format;
        }
    } else if (action & MATCH_BINDING && !group->getExtractResourceViews() && rtvs.size() > 0 && rtvs[bindingRTindex] != 0) {
        resource rs = device->get_resource_from_view(rtvs[bindingRTindex]);

        if (rs == 0) {
            // Render targets may not have a resource bound in D3D12, in which case writes to them are discarded
            return active_data;
        }

        resource_desc desc = device->get_resource_desc(rs);
        resource_view_desc v_desc = device->get_resource_view_desc(rtvs[bindingRTindex]);

        if (!ValidFormat(deviceData.current_runtime, desc, group->getBindingMatchSwapchainResolution())) {
            return active_data;
        }

        active_data.resource = rs;
        active_data.format = v_desc.format;
    } else if (action & (MATCH_EFFECT | MATCH_PREVIEW) && !group->getRenderToResourceViews() && rtvs.size() > 0 && rtvs[index] != 0) {
        resource rs = device->get_resource_from_view(rtvs[index]);

        if (rs == 0) {
            // Render targets may not have a resource bound in D3D12, in which case writes to them are discarded
            return active_data;
        }

        // Don't apply effects to non-RGB buffers
        resource_desc desc = device->get_resource_desc(rs);
        resource_view_desc v_desc = device->get_resource_view_desc(rtvs[index]);

        if (!ValidFormat(deviceData.current_runtime, desc, group->getMatchSwapchainResolution())) {
            return active_data;
        }

        active_data.resource = rs;
        active_data.format = v_desc.format;
    } else if (action & (MATCH_EFFECT | MATCH_PREVIEW) && group->getRenderToResourceViews()) {
        if (group->getAutoRenderSRV()) {
            active_data = FindAutoRenderResourceView(cmd_list, deviceData, group);
            if (active_data.resource != 0)
                return active_data;
        }

        uint32_t stageIndex = std::min(static_cast<uint32_t>(2), group->getRenderSRVShaderStage());

        int32_t slot_size = static_cast<int32_t>(state.get_root_table_size_at(stageIndex));
        int32_t slot = std::min(static_cast<int32_t>(group->getRenderSRVSlotIndex()), slot_size - 1);

        if (slot_size <= 0)
            return active_data;

        int32_t desc_size = static_cast<uint32_t>(state.get_root_table_entry_size_at(stageIndex, slot));
        int32_t desc = std::min(static_cast<int32_t>(group->getRenderSRVDescriptorIndex()), desc_size - 1);

        if (desc_size <= 0)
            return active_data;

        const descriptor_tracking::descriptor_data* buf = state.get_descriptor_at(stageIndex, slot, desc);

        CycleDescriptors(group, state, buf, stageIndex, slot, desc, desc_size, [group](uint32_t idx) { group->setRenderSRVDescriptorIndex(idx); });

        if (buf != nullptr && buf->view != 0) {
            resource rs = device->get_resource_from_view(buf->view);

            if (rs == 0) {
                // Render targets may not have a resource bound in D3D12, in which case writes to them are discarded
                return active_data;
            }

            // Don't apply effects to non-RGB buffers
            resource_desc desc = device->get_resource_desc(rs);
            resource_view_desc v_desc = device->get_resource_view_desc(buf->view);

            if (!ValidFormat(deviceData.current_runtime, desc, group->getMatchSwapchainResolution())) {
                return active_data;
            }

            active_data.resource = rs;
            active_data.format = v_desc.format;
        }
    }

    return active_data;
}

void RenderingManager::QueueOrDequeue(command_list* cmd_list,
                                      DeviceDataContainer& deviceData,
                                      CommandListDataContainer& commandListData,
                                      effect_queue& queue,
                                      unordered_set<EffectData*>& immediateQueue,
                                      uint64_t callLocation,
                                      uint32_t layoutIndex,
                                      uint64_t action) {
    for (auto it = queue.begin(); it != queue.end();) {
        auto& [name, data] = *it;
        // Set views during draw call since we can be sure the correct ones are bound at that point
        if (!callLocation && data.resource == 0) {
            ResourceViewData active_data = GetCurrentResourceView(cmd_list, deviceData, data.group, commandListData, layoutIndex, action);

            if (active_data.resource != 0) {
                data.resource = active_data.resource;
                data.format = active_data.format;
            } else if (data.group->getRequeueAfterRTMatchingFailure()) {
                // Leave loaded up in the effect/bind list and re-issue command on RT change
                it++;
                continue;
            } else {
                it = queue.erase(it);
                continue;
            }
        }

        // Queue updates depending on the place their supposed to be called at
        if (data.resource != 0 && (!callLocation && !data.invocationLocation || callLocation & data.invocationLocation)) {
            immediateQueue.insert(name);
        }

        it++;
    }
}