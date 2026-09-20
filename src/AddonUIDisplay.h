///////////////////////////////////////////////////////////////////////
//
// Part of ShaderToggler, a shader toggler add on for Reshade 5+ which allows you
// to define groups of shaders to toggle them on/off with one key press
//
// (c) Frans 'Otis_Inf' Bouma.
//
// All rights reserved.
// https://github.com/FransBouma/ShaderToggler
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met :
//
//  * Redistributions of source code must retain the above copyright notice, this
//	  list of conditions and the following disclaimer.
//
//  * Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and / or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
/////////////////////////////////////////////////////////////////////////

#pragma once
#include "AddonUIAbout.h"
#include "AddonUIConstants.h"
#include "ConstantManager.h"
#include "KeyData.h"
#include "ResourceManager.h"
#include "version.h"
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <format>
#include <imgui.h>
#include <ranges>
#include <reshade.hpp>

#define MAX_DESCRIPTOR_INDEX 10

// From Reshade, see https://github.com/crosire/reshade/blob/main/source/imgui_widgets.cpp
static bool key_input_box(const char* name, uint32_t* keys, const reshade::api::effect_runtime* runtime) {
    char buf[48];
    buf[0] = '\0';
    if (*keys)
        buf[ShaderToggler::reshade_key_name(*keys).copy(buf, sizeof(buf) - 1)] = '\0';

    ImGui::InputTextWithHint(name,
                             "Click to set keyboard shortcut",
                             buf,
                             sizeof(buf),
                             ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoUndoRedo | ImGuiInputTextFlags_NoHorizontalScroll);

    if (ImGui::IsItemActive()) {
        const uint32_t last_key_pressed = ShaderToggler::reshade_last_key_pressed(runtime);
        if (last_key_pressed != 0) {
            if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
                *keys = 0;

            } else if (last_key_pressed < 0x10 || last_key_pressed > 0x12) // Exclude modifier keys
            {
                *keys = last_key_pressed;
                *keys |= static_cast<uint32_t>(runtime->is_key_down(0x11)) << 8;
                *keys |= static_cast<uint32_t>(runtime->is_key_down(0x10)) << 16;
                *keys |= static_cast<uint32_t>(runtime->is_key_down(0x12)) << 24;
            }

            return true;
        }
    } else if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Click in the field and press any key to change the shortcut to that key.");
    }

    return false;
}

static constexpr const char* invocationDescription[] = { "BEFORE DRAW", "AFTER DRAW", "ON RENDER TARGET CHANGE" };

static void DisplayIsPartOfToggleGroup() {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
    ImGui::SameLine();
    ImGui::Text(" Shader is part of this toggle group.");
    ImGui::PopStyleColor();
}

static void DisplayTechniqueSelection(reshade::api::effect_runtime* runtime,
                                      AddonImGui::AddonUIData& instance,
                                      ShaderToggler::ToggleGroup* group,
                                      float tblWidth = 0) {
    if (group == nullptr) {
        return;
    }

    RuntimeDataContainer& runtimeData = runtime->get_private_data<RuntimeDataContainer>();

    std::unordered_set<std::string> curTechniques = group->preferredTechniques();
    static char searchBuf[256] = "\0";

    // Take a stable snapshot of technique names. ReShade can rebuild allTechniques during
    // effect reload/reorder events; iterating that unordered_map directly while also
    // reconstructing the group's selection can otherwise silently drop selected entries.
    std::vector<std::pair<std::string, bool>> availableTechniques;
    {
        std::shared_lock<std::shared_mutex> techLock(runtimeData.technique_mutex);
        availableTechniques.reserve(runtimeData.allTechniques.size());
        for (const auto& [name, effectData] : runtimeData.allTechniques) {
            availableTechniques.emplace_back(name, effectData.enabled);
        }
    }

    // unordered_map iteration order changes whenever ReShade rebuilds its technique list.
    // Keep the UI deterministic so selections do not appear to jump around between reloads.
    std::sort(availableTechniques.begin(), availableTechniques.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

    bool allowAll = group->getAllowAllTechniques();
    bool exceptions = group->getHasTechniqueExceptions();

    if (ImGui::BeginTable("Technique selection##options", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody)) {
        ImGui::TableSetupColumn("##columnsetup", ImGuiTableColumnFlags_WidthFixed, tblWidth);

        ImGui::TableNextColumn();
        ImGui::Text("Apply all enabled techniques");
        ImGui::TableNextColumn();
        ImGui::Checkbox("##Catchalltechniques", &allowAll);

        ImGui::TableNextRow();

        if (allowAll) {
            ImGui::TableNextColumn();
            ImGui::Text("Except for selected techniques");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##Exceptfor", &exceptions);

            ImGui::TableNextRow();
        }

        ImGui::TableNextColumn();
        ImGui::Text("Mode");
        ImGui::TableNextColumn();
        if (!allowAll) {
            ImGui::TextUnformatted("Only ticked enabled techniques are applied");
        } else if (exceptions) {
            ImGui::TextUnformatted("Ticked techniques are EXCLUDED");
        } else {
            ImGui::TextUnformatted("All globally enabled techniques are applied");
        }

        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::Text("Search");
        ImGui::TableNextColumn();
        ImGui::InputText("##techniqueSearch", searchBuf, 256, ImGuiInputTextFlags_None);

        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        if (ImGui::Button("Untick all")) {
            curTechniques.clear();
        }
        ImGui::TableNextColumn();
        ImGui::Text("%zu selected / %zu available", curTechniques.size(), availableTechniques.size());

        ImGui::EndTable();
    }

    ImGui::Separator();

    // Start from the group's existing selection rather than rebuilding from an empty set.
    // This preserves selected names that are temporarily absent while ReShade reloads effects.
    std::unordered_set<std::string> newTechniques = curTechniques;

    if (allowAll && !exceptions) {
        ImGui::BeginDisabled();
    }

    if (ImGui::BeginTable("Technique selection##table", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoBordersInBody)) {
        ImGui::TableSetupColumn("##columnsetupSelection", ImGuiTableColumnFlags_WidthFixed, tblWidth);

        std::string searchString(searchBuf);

        for (const auto& [name, globallyEnabled] : availableTechniques) {
            bool enabled = newTechniques.contains(name);

            const bool visible =
              std::ranges::search(name,
                                  searchString,
                                  [](const wchar_t lhs, const wchar_t rhs) { return lhs == rhs; },
                                  std::towupper,
                                  std::towupper)
                .begin() != name.end();

            if (visible) {
                ImGui::TableNextColumn();
                if (ImGui::Checkbox(name.c_str(), &enabled)) {
                    if (enabled) {
                        newTechniques.insert(name);
                    } else {
                        newTechniques.erase(name);
                    }
                }
                if (!globallyEnabled) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(disabled in ReShade)");
                }
            }
        }

        ImGui::EndTable();
    }

    if (allowAll && !exceptions) {
        ImGui::EndDisabled();
    }

    group->setHasTechniqueExceptions(exceptions);
    group->setAllowAllTechniques(allowAll);
    group->setPreferredTechniques(newTechniques);

    // Rebind saved names to the current EffectData instances using a stable map.
    std::shared_lock<std::shared_mutex> techLock(runtimeData.technique_mutex);
    instance.AssignPreferredGroupTechniques(runtimeData.allTechniques);
}

static void DrawPreview(unsigned long long textureId, uint32_t srcWidth, uint32_t srcHeight) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    float height = ImGui::GetWindowHeight();
    float width = ImGui::GetWindowWidth();

    float new_width = static_cast<float>(srcWidth);
    float new_height = static_cast<float>(srcHeight);

    float ratio = std::min(width / new_width, height / new_height);
    new_width *= ratio;
    new_height *= ratio;

    auto initialCursorPos = ImGui::GetCursorPos();
    auto centralizedCursorpos = ImVec2((width - new_width) * 0.5f, (height - new_height) * 0.5f);
    ImGui::SetCursorPos(centralizedCursorpos);

    ImGui::Image(textureId, ImVec2(new_width, new_height));

    ImGui::PopStyleVar();
}

static void DisplayPreview(AddonImGui::AddonUIData& instance,
                           Rendering::ResourceManager& resManager,
                           reshade::api::effect_runtime* runtime,
                           ShaderToggler::ToggleGroup* group,
                           float width = 0) {
    if (ImGui::BeginChild("RTPreview##child", { width, 0 }, true, ImGuiWindowFlags_None)) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3, 3));

        DeviceDataContainer& deviceData = runtime->get_device()->get_private_data<DeviceDataContainer>();
        reshade::api::resource_view srv = reshade::api::resource_view{ 0 };
        resManager.SetPongPreviewHandles(runtime->get_device(), nullptr, nullptr, &srv);
        bool clearAlpha = group->getClearPreviewAlpha();

        ImGui::Text("Clear alpha channel");
        ImGui::SameLine();
        ImGui::Checkbox("##Clearalpha", &clearAlpha);

        if (srv != 0) {
            ImGui::SameLine();
            ImGui::Text(std::format(" Address: 0x{:x} ", deviceData.huntPreview.target.handle).c_str());
            ImGui::SameLine();
            ImGui::Text(std::format("Format: {} ", static_cast<uint32_t>(deviceData.huntPreview.format)).c_str());
            ImGui::SameLine();
            ImGui::Text(std::format("Width: {} ", deviceData.huntPreview.width).c_str());
            ImGui::SameLine();
            ImGui::Text(std::format("Height: {} ", deviceData.huntPreview.height).c_str());
            ImGui::Separator();

            if (ImGui::BeginChild("RTPreview##preview", { 0, 0 }, false, ImGuiWindowFlags_None)) {
                DrawPreview(srv.handle, deviceData.huntPreview.width, deviceData.huntPreview.height);
            }
            ImGui::EndChild();
        }

        group->setClearPreviewAlpha(clearAlpha);

        ImGui::PopStyleVar();
    }
    ImGui::EndChild();
}

static void DisplayBindingPreview(AddonImGui::AddonUIData& instance,
                                  Rendering::ResourceManager& resManager,
                                  reshade::api::effect_runtime* runtime,
                                  ShaderToggler::ToggleGroup* group) {
    if (ImGui::BeginChild("BindingPreview##child", { 0, 0 }, true, ImGuiWindowFlags_None)) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3, 3));

        DeviceDataContainer& deviceData = runtime->get_device()->get_private_data<DeviceDataContainer>();
        ShaderToggler::GroupResource& groupResource = group->GetGroupResource(ShaderToggler::GroupResourceType::RESOURCE_BINDING);

        reshade::api::resource_view res_view = { 0 };
        if (groupResource.owning) {
            res_view = groupResource.srv;
        } else if (groupResource.g_res != nullptr) {
            res_view = groupResource.g_res->srv;
        }

        if (res_view != 0) {
            ImGui::Text(std::format("Format: {} ", static_cast<uint32_t>(groupResource.target_description.texture.format)).c_str());
            ImGui::SameLine();
            ImGui::Text(std::format("Width: {} ", groupResource.target_description.texture.width).c_str());
            ImGui::SameLine();
            ImGui::Text(std::format("Height: {} ", groupResource.target_description.texture.height).c_str());
            ImGui::Separator();

            if (ImGui::BeginChild("BindingPreview##preview", { 0, 0 }, false, ImGuiWindowFlags_None)) {
                DrawPreview(res_view.handle, groupResource.target_description.texture.width, groupResource.target_description.texture.height);
            }
            ImGui::EndChild();
        }

        ImGui::PopStyleVar();
    }
    ImGui::EndChild();
}

static void DisplayRenderTargets(AddonImGui::AddonUIData& instance,
                                 Rendering::ResourceManager& resManager,
                                 reshade::api::effect_runtime* runtime,
                                 ShaderToggler::ToggleGroup* group) {
    static float height = ImGui::GetWindowHeight();

    const char* typeSelectedItem = invocationDescription[group->getInvocationLocation()];
    uint32_t selectedIndex = group->getInvocationLocation();

    const char* typeDestItems[] = { "Render target", "Shader Resource View" };
    uint32_t selectedDestIndex = group->getRenderToResourceViews() ? 1 : 0;
    const char* typeSelectedDestItem = typeDestItems[selectedDestIndex];

    static const char* stageItems[] = { "PIXEL", "VERTEX", "COMPUTE" };
    uint32_t selectedStageIndex = group->getRenderSRVShaderStage();
    const char* selectedStage = stageItems[selectedStageIndex];

    bool retry = group->getRequeueAfterRTMatchingFailure();
    bool tonemap = group->getToneMap();
    bool preserveAlpha = group->getPreserveAlpha();
    bool flipbuffer = group->getFlipBuffer();
    bool autoSceneColour = group->getAutoRenderSRV();

    static const char* swapchainMatchOptions[] = { "RESOLUTION", "ASPECT RATIO", "EXTENDED ASPECT RATIO", "NONE" };
    uint32_t selectedSwapchainMatchMode = group->getMatchSwapchainResolution();
    const char* typesSelectedSwapchainMatchMode = swapchainMatchOptions[selectedSwapchainMatchMode];

    const reshade::api::device_api deviceApi = runtime->get_device()->get_api();
    const bool autoSceneColourSupported = ShaderToggler::IsAutoSceneColourSupported(deviceApi);
    const bool supportsSRVwrite = deviceApi < reshade::api::device_api::d3d12;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    if (ImGui::BeginChild("RenderTargets", { 0, height / 1.5f }, true, ImGuiChildFlags_AlwaysAutoResize)) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3, 3));

        if (ImGui::BeginTable("RenderTargetsSettings", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody)) {
            ImGui::TableSetupColumn("##RTcolumnsetup", ImGuiTableColumnFlags_WidthFixed, ImGui::GetWindowWidth() / 3);

            ImGui::TableNextColumn();
            ImGui::Text("Auto scene colour");
            ImGui::TableNextColumn();
            if (!autoSceneColourSupported)
                ImGui::BeginDisabled();
            ImGui::Checkbox("##AutoSceneColour", &autoSceneColour);
            if (!autoSceneColourSupported)
                ImGui::EndDisabled();
            if (!autoSceneColourSupported) {
                ImGui::SameLine();
                ImGui::TextDisabled("(D3D10/D3D11/D3D12/Vulkan only)");
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Auto Scene Colour supports D3D10, D3D11, D3D12 and Vulkan. Vulkan native staging uses ReShade\'s generic image-blit path.");
            }

            const bool autoSceneColourActive = autoSceneColour && autoSceneColourSupported;
            const bool pendingVulkanShaderEdits =
              deviceApi == reshade::api::device_api::vulkan &&
              instance.GetToggleGroupIdShaderEditing().load() == group->getId();

            ImGui::TableNextRow();

            if (autoSceneColourActive) {
                // Automatic mode overlays the saved manual target configuration without
                // modifying it. The core resolver gives Auto mode precedence while active.

                ImGui::TableNextColumn();
                ImGui::Text("Target");
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Live render target (matched draw)");

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Graphics API");
                ImGui::TableNextColumn();
                if (deviceApi == reshade::api::device_api::d3d10)
                    ImGui::TextUnformatted("D3D10");
                else if (deviceApi == reshade::api::device_api::d3d11)
                    ImGui::TextUnformatted("D3D11");
                else if (deviceApi == reshade::api::device_api::d3d12)
                    ImGui::TextUnformatted("D3D12");
                else
                    ImGui::TextUnformatted("Vulkan");

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Current attempt");
                ImGui::TableNextColumn();
                if (pendingVulkanShaderEdits)
                    ImGui::TextUnformatted("Finish shader hunting (Done) to apply marks");
                else if (!group->getDebugAutoStatus().empty())
                    ImGui::TextUnformatted(group->getDebugAutoStatus().c_str());
                else
                    ImGui::TextUnformatted("Waiting for matching render target...");

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Current target");
                ImGui::TableNextColumn();
                if (group->getDebugCurrentSceneWidth() > 0 && group->getDebugCurrentSceneHeight() > 0) {
                    ImGui::Text("%ux%u | %s | 0x%llx",
                                group->getDebugCurrentSceneWidth(),
                                group->getDebugCurrentSceneHeight(),
                                group->getDebugCurrentFormat().empty() ? "(format unknown)" : group->getDebugCurrentFormat().c_str(),
                                static_cast<unsigned long long>(group->getDebugCurrentTarget()));
                } else {
                    ImGui::TextUnformatted("(none)");
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Last successful injection");
                ImGui::TableNextColumn();
                if (group->getDebugEffectRenderCalls() > 0) {
                    ImGui::Text("%ux%u -> %ux%u",
                                group->getDebugSceneWidth(),
                                group->getDebugSceneHeight(),
                                group->getDebugEffectWidth(),
                                group->getDebugEffectHeight());
                } else {
                    ImGui::TextUnformatted("(none yet)");
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Last successful staging");
                ImGui::TableNextColumn();
                if (group->getDebugEffectRenderCalls() == 0) {
                    ImGui::TextUnformatted("(none)");
                } else if (group->getDebugNativeStaging() && deviceApi == reshade::api::device_api::vulkan) {
                    ImGui::TextUnformatted("Vulkan image blit");
                } else if (group->getDebugNativeStaging()) {
                    ImGui::TextUnformatted("Fullscreen shader copy");
                } else {
                    ImGui::TextUnformatted("Direct");
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Last successful techniques");
                ImGui::TableNextColumn();
                if (group->getDebugEffectRenderCalls() > 0)
                    ImGui::Text("%u | %s",
                                group->getDebugLastRenderedTechniqueCount(),
                                group->getDebugLastTechniqueOrder().empty() ? "(none)" : group->getDebugLastTechniqueOrder().c_str());
                else
                    ImGui::TextUnformatted("(none)");

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Successful renders");
                ImGui::TableNextColumn();
                ImGui::Text("%llu", static_cast<unsigned long long>(group->getDebugEffectRenderCalls()));

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Diagnostics");
                ImGui::TableNextColumn();
                if (ImGui::Button("Copy diagnostics")) {
                    const char* apiName = deviceApi == reshade::api::device_api::d3d10 ? "D3D10" :
                                          deviceApi == reshade::api::device_api::d3d11 ? "D3D11" :
                                          deviceApi == reshade::api::device_api::d3d12 ? "D3D12" : "Vulkan";
                    const std::string diagnostics = std::format(
                      "REST {}\n"
                      "Group: {}\n"
                      "API: {}\n"
                      "Auto Scene Colour: active\n"
                      "\nCurrent candidate / latest attempt\n"
                      "Status: {}\n"
                      "Target: {}x{} | {} | 0x{:x}\n"
                      "\nLast successful injection\n"
                      "Scene -> effect: {}x{} -> {}x{}\n"
                      "Staging path: {}\n"
                      "Vulkan boundary: {}\n"
                      "Techniques: {}\n"
                      "Technique order: {}\n"
                      "Successful renders: {}\n"
                      "Last successful target: 0x{:x}",
                      REST_VERSION_STRING,
                      group->getName(),
                      apiName,
                      group->getDebugAutoStatus().empty() ? "(none)" : group->getDebugAutoStatus(),
                      group->getDebugCurrentSceneWidth(),
                      group->getDebugCurrentSceneHeight(),
                      group->getDebugCurrentFormat().empty() ? "(unknown)" : group->getDebugCurrentFormat(),
                      static_cast<unsigned long long>(group->getDebugCurrentTarget()),
                      group->getDebugSceneWidth(),
                      group->getDebugSceneHeight(),
                      group->getDebugEffectWidth(),
                      group->getDebugEffectHeight(),
                      group->getDebugEffectRenderCalls() == 0 ? "(none)" :
                        (group->getDebugNativeStaging() ?
                          (deviceApi == reshade::api::device_api::vulkan ? "Vulkan image blit" : "fullscreen shader copy") :
                          "direct"),
                      deviceApi == reshade::api::device_api::vulkan ? "deferred to same-target LOAD pass" : "not applicable",
                      group->getDebugLastRenderedTechniqueCount(),
                      group->getDebugLastTechniqueOrder().empty() ? "(none)" : group->getDebugLastTechniqueOrder(),
                      static_cast<unsigned long long>(group->getDebugEffectRenderCalls()),
                      static_cast<unsigned long long>(group->getDebugLastRenderTarget()));
                    ImGui::SetClipboardText(diagnostics.c_str());
                }
            } else {
                if (supportsSRVwrite) {
                    ImGui::TableNextColumn();
                    ImGui::Text("Render destination");
                    ImGui::TableNextColumn();
                    if (ImGui::BeginCombo("##Renderdestination", typeSelectedDestItem, ImGuiComboFlags_None)) {
                        for (int n = 0; n < IM_ARRAYSIZE(typeDestItems); n++) {
                            const bool is_selected = (typeSelectedDestItem == typeDestItems[n]);
                            if (ImGui::Selectable(typeDestItems[n], is_selected)) {
                                typeSelectedDestItem = typeDestItems[n];
                                selectedDestIndex = n;
                            }
                            if (is_selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::TableNextRow();
                } else {
                    selectedDestIndex = 0;
                }

                if (supportsSRVwrite && selectedDestIndex == 1) {
                    if (!instance.GetTrackDescriptors()) {
                        ImGui::BeginDisabled();
                        group->setRenderToResourceViews(false);
                    } else {
                        group->setRenderToResourceViews(true);
                    }

                    ImGui::TableNextColumn();
                    ImGui::Text("Shader Stage");
                    ImGui::TableNextColumn();
                    if (ImGui::BeginCombo("##RenderShaderStage", selectedStage, ImGuiComboFlags_None)) {
                        for (int n = 0; n < IM_ARRAYSIZE(stageItems); n++) {
                            const bool is_selected = (selectedStage == stageItems[n]);
                            if (ImGui::Selectable(stageItems[n], is_selected)) {
                                selectedStageIndex = n;
                                selectedStage = stageItems[n];
                            }
                            if (is_selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    group->setRenderSRVShaderStage(selectedStageIndex);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("Slot");
                    ImGui::TableNextColumn();
                    ImGui::Text("%u", group->getRenderSRVSlotIndex());
                    ImGui::SameLine();
                    ImGui::PushID(0);
                    if (ImGui::SmallButton("+"))
                        group->setRenderSRVSlotIndex(group->getRenderSRVSlotIndex() + 1);
                    ImGui::PopID();
                    if (group->getRenderSRVSlotIndex() != 0) {
                        ImGui::SameLine();
                        if (ImGui::SmallButton("-"))
                            group->setRenderSRVSlotIndex(group->getRenderSRVSlotIndex() - 1);
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("Binding");
                    ImGui::TableNextColumn();
                    ImGui::Text("%u", group->getRenderSRVDescriptorIndex());
                    ImGui::SameLine();
                    ImGui::PushID(2);
                    if (ImGui::SmallButton("+"))
                        group->setRenderSRVDescriptorIndex(group->getRenderSRVDescriptorIndex() + 1);
                    ImGui::PopID();
                    if (group->getRenderSRVDescriptorIndex() != 0) {
                        ImGui::SameLine();
                        ImGui::PushID(1);
                        if (ImGui::SmallButton("-"))
                            group->setRenderSRVDescriptorIndex(group->getRenderSRVDescriptorIndex() - 1);
                        ImGui::PopID();
                    }

                    if (!instance.GetTrackDescriptors())
                        ImGui::EndDisabled();
                } else {
                    group->setRenderToResourceViews(false);

                    ImGui::TableNextColumn();
                    ImGui::Text("Render target index");
                    ImGui::TableNextColumn();
                    ImGui::Text("%u", group->getRenderTargetIndex());
                    ImGui::SameLine();
                    if (ImGui::SmallButton("+"))
                        group->setRenderTargetIndex(group->getRenderTargetIndex() + 1);
                    if (group->getRenderTargetIndex() != 0) {
                        ImGui::SameLine();
                        if (ImGui::SmallButton("-"))
                            group->setRenderTargetIndex(group->getRenderTargetIndex() - 1);
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("Invocation location");
                    ImGui::TableNextColumn();
                    if (ImGui::BeginCombo("##Invocationlocation", typeSelectedItem, ImGuiComboFlags_None)) {
                        for (int n = 0; n < IM_ARRAYSIZE(invocationDescription); n++) {
                            const bool is_selected = (typeSelectedItem == invocationDescription[n]);
                            if (ImGui::Selectable(invocationDescription[n], is_selected)) {
                                typeSelectedItem = invocationDescription[n];
                                selectedIndex = n;
                            }
                            if (is_selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Retry RT assignment");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##RetryRTassignment", &retry);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Apply tone map clamping");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##tonemap", &tonemap);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Flip render target");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##flipbuffer", &flipbuffer);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Preserve target alpha channel");
            ImGui::TableNextColumn();
            if (autoSceneColourActive)
                ImGui::BeginDisabled();
            ImGui::Checkbox("##preserveAlpha", &preserveAlpha);
            if (autoSceneColourActive) {
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::TextDisabled("ignored while Auto scene colour is active");
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Match swapchain");
            ImGui::TableNextColumn();
            if (autoSceneColourActive) {
                ImGui::TextUnformatted("ASPECT RATIO (automatic)");
            } else if (ImGui::BeginCombo("##effSwapChainMatchMode", typesSelectedSwapchainMatchMode, ImGuiComboFlags_None)) {
                for (int n = 0; n < IM_ARRAYSIZE(swapchainMatchOptions); n++) {
                    const bool is_selected = (typesSelectedSwapchainMatchMode == swapchainMatchOptions[n]);
                    if (ImGui::Selectable(swapchainMatchOptions[n], is_selected)) {
                        typesSelectedSwapchainMatchMode = swapchainMatchOptions[n];
                        selectedSwapchainMatchMode = n;
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::EndTable();
        }

        group->setRequeueAfterRTMatchingFailure(retry);
        group->setMatchSwapchainResolution(selectedSwapchainMatchMode);
        group->setAutoRenderSRV(autoSceneColour);
        group->setInvocationLocation(selectedIndex);
        group->setToneMap(tonemap);
        group->setPreserveAlpha(preserveAlpha);
        group->setFlipBuffer(flipbuffer);

        ImGui::Separator();
        DisplayTechniqueSelection(runtime, instance, group, ImGui::GetWindowWidth() / 3);

        ImGui::PopStyleVar();
    }
    ImGui::EndChild();

    ImGui::PushID(4);
    ImGui::Button("", ImVec2(-1, 8.0f));
    ImGui::PopID();
    if (ImGui::IsItemActive())
        height += ImGui::GetIO().MouseDelta.y;

    DisplayPreview(instance, resManager, runtime, group);

    ImGui::PopStyleVar();
}

static void DisplayGroupView(AddonImGui::AddonUIData& instance,
                             Rendering::ResourceManager& resManager,
                             reshade::api::effect_runtime* runtime,
                             ShaderToggler::ToggleGroup* group,
                             ShaderToggler::ShaderManager* shaderManager) {
    float height = ImGui::GetWindowHeight();

    if (!shaderManager->isInHuntingMode()) {
        ImGui::TextDisabled("Shader hunting is not active.");
        ImGui::TextDisabled("The group's committed shader hashes remain active while you inspect Auto Scene Colour and other settings.");
        return;
    }

    if (*instance.ActiveCollectorFrameCounter() > 0) {
        ImGui::Text("Collecting active shaders... %u frames remaining", instance.ActiveCollectorFrameCounter()->load());
        ImGui::TextDisabled("Keep the relevant scene visible until collection finishes.");
        return;
    }

    static char shaderSearch[64] = "";
    static int filterMode = 0;
    const char* filterItems[] = { "All", "Marked", "Unmarked" };

    ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.45f);
    ImGui::InputTextWithHint("##shaderSearch", "Search shader hash...", shaderSearch, IM_ARRAYSIZE(shaderSearch));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::Combo("##shaderFilter", &filterMode, filterItems, IM_ARRAYSIZE(filterItems));
    ImGui::SameLine();
    if (ImGui::Button("Recollect")) {
        auto* pixelManager = instance.GetPixelShaderManager();
        auto* vertexManager = instance.GetVertexShaderManager();
        auto* computeManager = instance.GetComputeShaderManager();
        pixelManager->startHuntingMode(pixelManager->getMarkedShaderHashes());
        vertexManager->startHuntingMode(vertexManager->getMarkedShaderHashes());
        computeManager->startHuntingMode(computeManager->getMarkedShaderHashes());
        *instance.ActiveCollectorFrameCounter() = *instance.StartValueFramecountCollectionPhase();
        instance.UpdateToggleGroupsForShaderHashes();
        return;
    }

    bool navigationChanged = false;
    if (ImGui::Button("Prev")) {
        shaderManager->huntPreviousShader(false);
        navigationChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Next")) {
        shaderManager->huntNextShader(false);
        navigationChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Prev marked")) {
        shaderManager->huntPreviousShader(true);
        navigationChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Mark / unmark")) {
        shaderManager->toggleMarkOnHuntedShader();
        navigationChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Next marked")) {
        shaderManager->huntNextShader(true);
        navigationChanged = true;
    }

    if (navigationChanged)
        instance.UpdateToggleGroupsForShaderHashes();

    const size_t markedCount = shaderManager->getMarkedShaderCount();
    ImGui::TextDisabled("%zu collected | %zu marked", shaderManager->getAmountShaderHashesCollected(), markedCount);
    ImGui::SameLine();
    if (markedCount == 0)
        ImGui::BeginDisabled();
    if (ImGui::Button("Clear marked")) {
        shaderManager->clearMarkedShaderHashes();
        instance.UpdateToggleGroupsForShaderHashes();
    }
    if (markedCount == 0)
        ImGui::EndDisabled();

    ImGui::TextDisabled("Pending shader marks are applied to the group when you click Done.");
    ImGui::Separator();

    const std::unordered_set<uint32_t>& hashes = shaderManager->getCollectedShaderHashes();
    const int32_t selected = shaderManager->getActiveHuntedShaderIndex();
    uint32_t index = 0;

    std::string needle(shaderSearch);
    std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    if (ImGui::BeginTable("ShaderHashView",
                          1,
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoBordersInBody |
                            ImGuiTableColumnFlags_NoHeaderLabel,
                          ImVec2(0, height - 115))) {
        for (const uint32_t hash : hashes) {
            const bool marked = shaderManager->isHuntedShaderMarked(hash);
            const std::string hashText = std::format("{:#08x}", hash);

            bool visible = filterMode == 0 || (filterMode == 1 && marked) || (filterMode == 2 && !marked);
            if (visible && !needle.empty()) {
                std::string haystack = hashText;
                std::transform(haystack.begin(), haystack.end(), haystack.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                visible = haystack.find(needle) != std::string::npos;
            }

            if (!visible) {
                ++index;
                continue;
            }

            ImGui::TableNextColumn();

            if (marked)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));

            const bool clicked = ImGui::Selectable(hashText.c_str(), selected == static_cast<int32_t>(index), ImGuiSelectableFlags_AllowDoubleClick);
            if (clicked) {
                shaderManager->setActivedHuntedShaderIndex(index);
                if (ImGui::IsMouseDoubleClicked(0))
                    shaderManager->toggleMarkOnHuntedShader();
                instance.UpdateToggleGroupsForShaderHashes();
            }

            if (marked)
                ImGui::PopStyleColor();

            ++index;
        }

        ImGui::EndTable();
    }
}

static void DisplayTextureBindings(AddonImGui::AddonUIData& instance,
                                   ShaderToggler::ToggleGroup* group,
                                   reshade::api::effect_runtime* runtime,
                                   Rendering::ResourceManager& resManager) {
    static float height = ImGui::GetWindowHeight();
    float width = ImGui::GetWindowWidth();

    const char* typeItems[] = { "Render target", "Shader Resource View" };
    uint32_t selectedIndex = group->getExtractResourceViews() ? 1 : 0;
    const char* typeSelectedItem = typeItems[selectedIndex];
    DeviceDataContainer& deviceData = runtime->get_device()->get_private_data<DeviceDataContainer>();

    static const char* swapchainMatchOptions[] = { "RESOLUTION", "ASPECT RATIO", "EXTENDED ASPECT RATIO", "NONE" };
    uint32_t selectedSwapchainMatchMode = group->getBindingMatchSwapchainResolution();
    const char* typesSelectedSwapchainMatchMode = swapchainMatchOptions[selectedSwapchainMatchMode];

    static const char* stageItems[] = { "PIXEL", "VERTEX", "COMPUTE" };
    uint32_t selectedStageIndex = group->getSRVShaderStage();
    const char* selectedStage = stageItems[selectedStageIndex];

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    if (ImGui::BeginChild("Texture bindings viewer", { 0, height / 2.0f }, true, ImGuiChildFlags_AlwaysAutoResize)) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3, 3));

        // Name of group
        char tmpBuffer[150];

        // Name of Binding
        bool isBindingEnabled = group->isProvidingTextureBinding();

        const std::string& bindingName = group->getTextureBindingName();
        strncpy_s(tmpBuffer, 150, bindingName.c_str(), bindingName.size());

        bool copyBinding = group->getCopyTextureBinding();
        bool clearBinding = group->getClearBindings();
        bool flipBinding = group->getFlipBufferBinding();

        if (ImGui::BeginTable("Bindingsettings", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody)) {
            ImGui::TableSetupColumn("##BindingColumnSetup", ImGuiTableColumnFlags_WidthFixed, ImGui::GetWindowWidth() / 3);

            ImGui::TableNextColumn();
            ImGui::Text("Texture binding enabled");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##Texturebindingenabled", &isBindingEnabled);

            if (!isBindingEnabled) {
                ImGui::BeginDisabled();
                group->setProvidingTextureBinding(false);
            } else {
                group->setProvidingTextureBinding(true);
            }

            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("Texture semantic");
            ImGui::TableNextColumn();
            ImGui::InputText("##BindingName", tmpBuffer, 149);

            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("Texture source");
            ImGui::TableNextColumn();
            if (ImGui::BeginCombo("##Bindingsource", typeSelectedItem, ImGuiComboFlags_None)) {
                for (int n = 0; n < IM_ARRAYSIZE(typeItems); n++) {
                    bool is_selected = (typeSelectedItem == typeItems[n]);
                    if (ImGui::Selectable(typeItems[n], is_selected)) {
                        typeSelectedItem = typeItems[n];
                        selectedIndex = n;
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("Create texture copy for binding");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##Copybinding", &copyBinding);

            ImGui::TableNextRow();

            ImGui::BeginDisabled(!copyBinding);
            ImGui::TableNextColumn();
            ImGui::Text("Flip binding texture");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##flipbinding", &flipBinding);
            ImGui::EndDisabled();

            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("Clear binding on hash miss");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##Clearbinding", &clearBinding);

            ImGui::TableNextRow();

            ImGui::TableNextColumn();

            ImGui::EndTable();
        }

        group->setTextureBindingName(tmpBuffer);
        group->setCopyTextureBinding(copyBinding);
        group->setClearBindings(clearBinding);
        group->setFlipBufferBinding(flipBinding);

        ImGui::Separator();

        if (ImGui::BeginTable("BindingSourcesettings", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody)) {
            ImGui::TableSetupColumn("##BindingSourceColumnSetup", ImGuiTableColumnFlags_WidthFixed, ImGui::GetWindowWidth() / 3);

            if (selectedIndex == 1) {
                if (!instance.GetTrackDescriptors()) {
                    ImGui::BeginDisabled();
                    group->setExtractResourceViews(false);
                } else {
                    group->setExtractResourceViews(true);
                }

                ImGui::TableNextColumn();
                ImGui::Text("Shader Stage");
                ImGui::TableNextColumn();
                if (ImGui::BeginCombo("##ShaderStage", selectedStage, ImGuiComboFlags_None)) {
                    for (int n = 0; n < IM_ARRAYSIZE(stageItems); n++) {
                        bool is_selected = (selectedStage == stageItems[n]);
                        if (ImGui::Selectable(stageItems[n], is_selected)) {
                            selectedStageIndex = n;
                            selectedStage = stageItems[n];
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                group->setSRVShaderStage(selectedStageIndex);

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::Text("Slot");
                ImGui::TableNextColumn();
                ImGui::Text("%u", group->getBindingSRVSlotIndex());
                ImGui::SameLine();
                ImGui::PushID(0);
                if (ImGui::SmallButton("+")) {
                    group->setBindingSRVSlotIndex(group->getBindingSRVSlotIndex() + 1);
                }
                ImGui::PopID();

                if (group->getBindingSRVSlotIndex() != 0) {
                    ImGui::SameLine();

                    if (ImGui::SmallButton("-")) {
                        group->setBindingSRVSlotIndex(group->getBindingSRVSlotIndex() - 1);
                    }
                }

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::Text("Binding");
                ImGui::TableNextColumn();
                ImGui::Text("%u", group->getBindingSRVDescriptorIndex());
                ImGui::SameLine();
                ImGui::PushID(2);
                if (ImGui::SmallButton("+")) {
                    group->dispatchSRVCycle(ShaderToggler::CYCLE_UP);
                }
                ImGui::PopID();

                if (group->getBindingSRVDescriptorIndex() != 0) {
                    ImGui::SameLine();

                    ImGui::PushID(1);
                    if (ImGui::SmallButton("-")) {
                        group->dispatchSRVCycle(ShaderToggler::CYCLE_DOWN);
                    }
                    ImGui::PopID();
                }

                if (!instance.GetTrackDescriptors()) {
                    ImGui::EndDisabled();
                }
            } else {
                group->setExtractResourceViews(false);

                const char* rtTypeSelectedItem = invocationDescription[group->getBindingInvocationLocation()];
                uint32_t rtSelectedIndex = group->getBindingInvocationLocation();

                ImGui::TableNextColumn();
                ImGui::Text("Render target index");
                ImGui::TableNextColumn();
                ImGui::Text("%u", group->getBindingRenderTargetIndex());
                ImGui::SameLine();

                ImGui::PushID(0);
                if (ImGui::SmallButton("+")) {
                    group->setBindingRenderTargetIndex(group->getBindingRenderTargetIndex() + 1);
                }
                ImGui::PopID();

                if (group->getBindingRenderTargetIndex() != 0) {
                    ImGui::SameLine();

                    if (ImGui::SmallButton("-")) {
                        group->setBindingRenderTargetIndex(group->getBindingRenderTargetIndex() - 1);
                    }
                }

                ImGui::TableNextRow();

                if (!copyBinding) {
                    ImGui::BeginDisabled();
                }

                ImGui::TableNextColumn();
                ImGui::Text("Invocation location");
                ImGui::TableNextColumn();
                if (ImGui::BeginCombo("##Invocationlocation", rtTypeSelectedItem, ImGuiComboFlags_None)) {
                    for (int n = 0; n < IM_ARRAYSIZE(invocationDescription); n++) {
                        bool is_selected = (rtTypeSelectedItem == invocationDescription[n]);
                        if (ImGui::Selectable(invocationDescription[n], is_selected)) {
                            rtTypeSelectedItem = invocationDescription[n];
                            rtSelectedIndex = n;
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if (!copyBinding) {
                    ImGui::EndDisabled();
                }

                ImGui::TableNextColumn();
                ImGui::Text("Match swapchain");
                ImGui::TableNextColumn();
                if (ImGui::BeginCombo("##swapChainMatchMode", typesSelectedSwapchainMatchMode, ImGuiComboFlags_None)) {
                    for (int n = 0; n < IM_ARRAYSIZE(swapchainMatchOptions); n++) {
                        bool is_selected = (typesSelectedSwapchainMatchMode == swapchainMatchOptions[n]);
                        if (ImGui::Selectable(swapchainMatchOptions[n], is_selected)) {
                            typesSelectedSwapchainMatchMode = swapchainMatchOptions[n];
                            selectedSwapchainMatchMode = n;
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                group->setBindingInvocationLocation(rtSelectedIndex);
                group->setBindingMatchSwapchainResolution(selectedSwapchainMatchMode);
            }

            ImGui::EndTable();
        }

        if (!isBindingEnabled) {
            ImGui::EndDisabled();
        }

        ImGui::PopStyleVar();
    }
    ImGui::EndChild();

    ImGui::PushID(3);
    ImGui::Button("", ImVec2(-1, 8.0f));
    ImGui::PopID();
    if (ImGui::IsItemActive())
        height += ImGui::GetIO().MouseDelta.y;

    DisplayBindingPreview(instance, resManager, runtime, group);

    ImGui::PopStyleVar();
}

static void DisplayOverlay(AddonImGui::AddonUIData& instance, Rendering::ResourceManager& resManager, reshade::api::effect_runtime* runtime) {
    if (instance.GetToggleGroupIdSettingsOpen() >= 0) {
        std::string editingGroupName = "";
        const int idx = instance.GetToggleGroupIdSettingsOpen();
        ShaderToggler::ToggleGroup* group = nullptr;
        if (instance.GetToggleGroups().find(idx) != instance.GetToggleGroups().end()) {
            editingGroupName = instance.GetToggleGroups()[idx].getName();
            group = &instance.GetToggleGroups()[idx];
        }

        if (group == nullptr)
            return;

        ImGui::SetNextWindowBgAlpha(1.0);
        ImGui::SetNextWindowSize({ 1024, 768 }, ImGuiCond_Once);
        bool wndOpen = true;

        static float height = ImGui::GetWindowHeight();
        static float width = ImGui::GetWindowWidth();

        const char* typeItems[] = { "Pixel shader", "Vertex shader", "Compute Shader" };
        static const char* typeSelectedItem = typeItems[0];
        static uint32_t selectedIndex = 0;

        ShaderToggler::ShaderManager* selectedShaderManager =
          selectedIndex == 0 ? instance.GetPixelShaderManager() : (selectedIndex == 1 ? instance.GetVertexShaderManager() : instance.GetComputeShaderManager());

        if (ImGui::Begin(std::format("Group settings ({})", editingGroupName).c_str(), &wndOpen)) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            if (ImGui::BeginChild("GroupView", { width / 3.0f + 20.0f, 0 }, true, ImGuiWindowFlags_NoScrollbar)) {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3, 3));

                const bool huntingThisGroup = instance.GetToggleGroupIdShaderEditing().load() == group->getId();
                if (huntingThisGroup) {
                    if (ImGui::Button("Done hunting")) {
                        instance.EndShaderEditing(true, *group);
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("Marks will be committed to this group.");
                } else {
                    if (ImGui::Button("Start shader hunting"))
                        instance.StartShaderEditing(*group);
                    ImGui::SameLine();
                    ImGui::TextDisabled("Settings remain open when hunting finishes.");
                }

                DisplayGroupView(instance, resManager, runtime, group, selectedShaderManager);

                ImGui::PushItemWidth(ImGui::GetWindowWidth() - ImGui::GetStyle().FramePadding.x * 2 - ImGui::GetStyle().ItemSpacing.x * 2);
                if (ImGui::BeginCombo("##shaderType", typeSelectedItem, ImGuiComboFlags_None)) {
                    for (int n = 0; n < IM_ARRAYSIZE(typeItems); n++) {
                        bool is_selected = (typeSelectedItem == typeItems[n]);
                        if (ImGui::Selectable(typeItems[n], is_selected)) {
                            if (n != selectedIndex) {
                                // Reset hunting selections in other managers on switch
                                switch (n) {
                                    case 0: {
                                        instance.GetVertexShaderManager()->resetActiveHuntedShader();
                                        instance.GetComputeShaderManager()->resetActiveHuntedShader();
                                    } break;
                                    case 1: {
                                        instance.GetPixelShaderManager()->resetActiveHuntedShader();
                                        instance.GetComputeShaderManager()->resetActiveHuntedShader();
                                    } break;
                                    case 2: {
                                        instance.GetPixelShaderManager()->resetActiveHuntedShader();
                                        instance.GetVertexShaderManager()->resetActiveHuntedShader();
                                    } break;
                                    default:
                                        break;
                                }

                                instance.UpdateToggleGroupsForShaderHashes();
                            }

                            typeSelectedItem = typeItems[n];
                            selectedIndex = n;
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();

                ImGui::PopStyleVar();
            }
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::PushID(0);
            ImGui::Button("", ImVec2(8.0f, -1));
            ImGui::PopID();
            if (ImGui::IsItemActive())
                width += ImGui::GetIO().MouseDelta.x;

            ImGui::SameLine();

            if (ImGui::BeginChild("GroupSettings", { 0, 0 }, true, ImGuiChildFlags_AlwaysAutoResize)) {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3, 3));

                ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
                if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags)) {
                    if (ImGui::BeginTabItem("Effects")) {
                        instance.SetCurrentTabType(AddonImGui::TAB_RENDER_TARGET);
                        DisplayRenderTargets(instance, resManager, runtime, group);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Constant bindings")) {
                        instance.SetCurrentTabType(AddonImGui::TAB_CONSTANT_BUFFER);
                        DisplayConstantTab(instance, group, runtime->get_device());
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Texture bindings")) {
                        instance.SetCurrentTabType(AddonImGui::TAB_TEXTURE_BINDING);
                        DisplayTextureBindings(instance, group, runtime, resManager);
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }

                ImGui::PopStyleVar();
            }
            ImGui::EndChild();

            ImGui::PopStyleVar();
        }
        ImGui::End();

        if (!wndOpen) {
            instance.SetCurrentTabType(AddonImGui::TAB_NONE);
            instance.CloseGroupSettings(true, *group);
        }
    } else {
        instance.SetCurrentTabType(AddonImGui::TAB_NONE);
    }
}

static void CheckHotkeys(AddonImGui::AddonUIData& instance, reshade::api::effect_runtime* runtime) {
    if (*instance.ActiveCollectorFrameCounter() > 0)
        --(*instance.ActiveCollectorFrameCounter());

    auto pressedKeys = [&](uint32_t keys) {
        if (keys == 0 || !ShaderToggler::areKeysPressed(keys, runtime))
            return false;

        const bool wantsCtrl = ((keys >> 8) & 0xFF) != 0;
        const bool wantsShift = ((keys >> 16) & 0xFF) != 0;
        const bool wantsAlt = ((keys >> 24) & 0xFF) != 0;

        return wantsCtrl == runtime->is_key_down(VK_CONTROL) &&
               wantsShift == runtime->is_key_down(VK_SHIFT) &&
               wantsAlt == runtime->is_key_down(VK_MENU);
    };

    auto pressed = [&](AddonImGui::Keybind binding) {
        return pressedKeys(instance.GetKeybinding(binding));
    };

    auto handleHunting = [&](ShaderToggler::ShaderManager* manager,
                             AddonImGui::Keybind previous,
                             AddonImGui::Keybind next,
                             AddonImGui::Keybind mark,
                             AddonImGui::Keybind previousMarked,
                             AddonImGui::Keybind nextMarked) {
        bool changed = false;

        if (pressed(previousMarked)) {
            manager->huntPreviousShader(true);
            changed = true;
        } else if (pressed(nextMarked)) {
            manager->huntNextShader(true);
            changed = true;
        } else if (pressed(previous)) {
            manager->huntPreviousShader(false);
            changed = true;
        } else if (pressed(next)) {
            manager->huntNextShader(false);
            changed = true;
        }

        if (pressed(mark)) {
            manager->toggleMarkOnHuntedShader();
            changed = true;
        }

        return changed;
    };

    if (instance.GetToggleGroupIdShaderEditing() >= 0) {
        bool changed = false;
        changed |= handleHunting(instance.GetPixelShaderManager(),
                                 AddonImGui::PIXEL_SHADER_DOWN,
                                 AddonImGui::PIXEL_SHADER_UP,
                                 AddonImGui::PIXEL_SHADER_MARK,
                                 AddonImGui::PIXEL_SHADER_MARKED_DOWN,
                                 AddonImGui::PIXEL_SHADER_MARKED_UP);
        changed |= handleHunting(instance.GetVertexShaderManager(),
                                 AddonImGui::VERTEX_SHADER_DOWN,
                                 AddonImGui::VERTEX_SHADER_UP,
                                 AddonImGui::VERTEX_SHADER_MARK,
                                 AddonImGui::VERTEX_SHADER_MARKED_DOWN,
                                 AddonImGui::VERTEX_SHADER_MARKED_UP);
        changed |= handleHunting(instance.GetComputeShaderManager(),
                                 AddonImGui::COMPUTE_SHADER_DOWN,
                                 AddonImGui::COMPUTE_SHADER_UP,
                                 AddonImGui::COMPUTE_SHADER_MARK,
                                 AddonImGui::COMPUTE_SHADER_MARKED_DOWN,
                                 AddonImGui::COMPUTE_SHADER_MARKED_UP);

        if (changed)
            instance.UpdateToggleGroupsForShaderHashes();

        return;
    }

    for (auto& [_, group] : instance.GetToggleGroups()) {
        const uint32_t toggleKey = group.getToggleKey();
        if (!pressedKeys(toggleKey))
            continue;

        group.toggleActive();
        if (!group.isActive() && instance.GetConstantHandler() != nullptr)
            instance.GetConstantHandler()->RemoveGroup(&group, runtime->get_device());
    }
}

static void ShowHelpMarker(const char* desc) {
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(450.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static void DisplaySettings(AddonImGui::AddonUIData& instance, reshade::api::effect_runtime* runtime) {
    DisplayAbout();

    if (ImGui::CollapsingHeader("General info and help")) {
        ImGui::PushTextWrapPos();
        ImGui::TextUnformatted(
          "The Shader Toggler allows you to create one or more groups with shaders to toggle on/off. You can assign a keyboard shortcut (including using keys "
          "like Shift, Alt and Control) to each group, including a handy name. Each group can have one or more vertex or pixel shaders assigned to it. When "
          "you press the assigned keyboard shortcut, any draw calls using these shaders will be disabled, effectively hiding the elements in the 3D scene.");
        ImGui::TextUnformatted(
          "\nShader hunting can be controlled with the buttons in Group settings or with the shortcuts under Shader hunting keybindings. Pixel and vertex hunting keep the "
          "traditional numpad defaults; compute hunting is unassigned by default so it does not steal an existing shortcut. All hunting shortcuts can be changed "
          "for laptops, compact keyboards or personal preference.");
        ImGui::TextUnformatted(
          "\nWhen you step through shaders, the currently selected shader is disabled in the scene so you can identify it. Double-click a hash, use Mark / unmark, "
          "or use the configured shortcut to add or remove it from the group.");
        ImGui::TextUnformatted(
          "Configuration changes are tracked automatically. Use Save changes when the Unsaved changes indicator appears.");
        ImGui::PopTextWrapPos();
    }

    ImGui::AlignTextToFramePadding();
    if (ImGui::CollapsingHeader("Shader selection parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::AlignTextToFramePadding();
        ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.5f);
        ImGui::SliderFloat("Overlay opacity", instance.OverlayOpacity(), 0.0f, 1.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::SliderInt("# of frames to collect", instance.StartValueFramecountCollectionPhase(), 10, 1000);
        ImGui::SameLine();
        ShowHelpMarker("This is the number of frames the addon will collect active shaders. Set this to a high number if the shader you want to mark is only "
                       "used occasionally. Only shaders that are used in the frames collected can be marked.");
        ImGui::PopItemWidth();
    }
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Options", ImGuiTreeNodeFlags_None)) {
        ImGui::AlignTextToFramePadding();
        std::string varSelectedItem = instance.GetResourceShim();
        if (ImGui::BeginCombo("Resource Shim", varSelectedItem.c_str(), ImGuiComboFlags_None)) {
            for (auto& v : Rendering::ResourceShimNames) {
                bool is_selected = (varSelectedItem == v);
                if (ImGui::Selectable(v.c_str(), is_selected)) {
                    varSelectedItem = v;
                }
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        instance.SetResourceShim(varSelectedItem);

        ImGui::AlignTextToFramePadding();
        std::string varSelectedCopyMethod = instance.GetConstHookCopyType();
        if (ImGui::BeginCombo("Constant buffer copy method", varSelectedCopyMethod.c_str(), ImGuiComboFlags_None)) {
            for (auto& v : Shim::Constants::ConstantCopyTypeNames) {
                bool is_selected = (varSelectedCopyMethod == v);
                if (ImGui::Selectable(v.c_str(), is_selected)) {
                    varSelectedCopyMethod = v;
                }
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        instance.SetConstHookCopyType(varSelectedCopyMethod);

        ImGui::AlignTextToFramePadding();
        bool trackDescriptors = instance.GetTrackDescriptors();
        ImGui::Checkbox("Track descriptors", &trackDescriptors);
        instance.SetTrackDescriptors(trackDescriptors);

        bool runtimeReload = instance.GetPreventRuntimeReload();
        ImGui::Checkbox("Prevent runtime reload", &runtimeReload);
        instance.SetPreventRuntimeReload(runtimeReload);
    }

    if (ImGui::CollapsingHeader("Shader hunting keybindings", ImGuiTreeNodeFlags_None)) {
        constexpr uint32_t activeKeybindCount = static_cast<uint32_t>(AddonImGui::INVOCATION_DOWN);
        bool duplicateBinding = false;
        for (uint32_t i = 0; i < activeKeybindCount; i++) {
            uint32_t keys = instance.GetKeybinding(static_cast<AddonImGui::Keybind>(i));
            ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.35f);
            if (key_input_box(AddonImGui::KeybindDisplayNames[i], &keys, runtime))
                instance.SetKeybinding(static_cast<AddonImGui::Keybind>(i), keys);
            ImGui::PopItemWidth();

            if (keys != 0) {
                for (uint32_t j = 0; j < i; ++j) {
                    if (instance.GetKeybinding(static_cast<AddonImGui::Keybind>(j)) == keys) {
                        duplicateBinding = true;
                        break;
                    }
                }
            }
        }

        if (duplicateBinding)
            ImGui::TextDisabled("Warning: two or more REST actions use the same shortcut.");
    }

    if (ImGui::CollapsingHeader("List of Toggle Groups", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("New group"))
            instance.AddDefaultGroup();

        ImGui::SameLine();
        if (instance.IsConfigDirty())
            ImGui::TextDisabled("Unsaved changes");
        else
            ImGui::TextDisabled("Saved");

        ImGui::Separator();

        std::vector<ShaderToggler::ToggleGroup*> toRemove;
        std::vector<int> toClone;

        for (auto& [_, group] : instance.GetToggleGroups()) {
            ImGui::PushID(group.getId());
            ImGui::AlignTextToFramePadding();

            bool groupActive = group.isActive();
            ImGui::Checkbox("Active", &groupActive);
            if (groupActive != group.isActive()) {
                group.toggleActive();
                if (!groupActive && instance.GetConstantHandler() != nullptr)
                    instance.GetConstantHandler()->RemoveGroup(&group, runtime->get_device());
            }

            ImGui::SameLine();
            if (ImGui::Button("Edit"))
                group.setEditing(true);

            ImGui::SameLine();
            if (instance.GetToggleGroupIdSettingsOpen() >= 0) {
                if (instance.GetToggleGroupIdSettingsOpen() == group.getId()) {
                    if (ImGui::Button("Close"))
                        instance.CloseGroupSettings(true, group);
                } else {
                    ImGui::BeginDisabled(true);
                    ImGui::Button("Settings");
                    ImGui::EndDisabled();
                }
            } else if (ImGui::Button("Settings")) {
                instance.OpenGroupSettings(group);
            }

            ImGui::SameLine();
            if (ImGui::Button("Clone"))
                toClone.push_back(group.getId());

            ImGui::SameLine();
            if (ImGui::Button("Delete"))
                ImGui::OpenPopup("Delete group?");

            if (ImGui::BeginPopupModal("Delete group?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Delete '%s'?", group.getName().c_str());
                ImGui::TextDisabled("This takes effect immediately but is not written to disk until Save changes.");
                if (ImGui::Button("Delete", ImVec2(120, 0))) {
                    toRemove.push_back(&group);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0)))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            if (group.getToggleKey() > 0)
                ImGui::Text("%s (%s)", group.getName().c_str(), ShaderToggler::reshade_key_name(group.getToggleKey()).c_str());
            else
                ImGui::Text("%s", group.getName().c_str());

            const bool shaderEditingThisGroup = instance.GetToggleGroupIdShaderEditing().load() == group.getId();
            const size_t psCount = shaderEditingThisGroup ? instance.GetPixelShaderManager()->getMarkedShaderCount() : group.getPixelShaderHashCount();
            const size_t vsCount = shaderEditingThisGroup ? instance.GetVertexShaderManager()->getMarkedShaderCount() : group.getVertexShaderHashCount();
            const size_t csCount = shaderEditingThisGroup ? instance.GetComputeShaderManager()->getMarkedShaderCount() : group.getComputeShaderHashCount();
            const size_t fxCount = group.preferredTechniques().size();

            ImGui::TextDisabled("PS: %zu | VS: %zu | CS: %zu | FX: %zu%s%s",
                                psCount,
                                vsCount,
                                csCount,
                                fxCount,
                                group.getAutoRenderSRV() ? " | Auto Scene Colour" : "",
                                shaderEditingThisGroup ? " | pending" : "");

            if (group.getToggleKey() != 0) {
                bool conflictShown = false;
                for (const auto& [otherId, otherGroup] : instance.GetToggleGroups()) {
                    if (otherId != group.getId() && otherGroup.getToggleKey() == group.getToggleKey()) {
                        ImGui::TextDisabled("Warning: group shortcut conflicts with '%s'.", otherGroup.getName().c_str());
                        conflictShown = true;
                        break;
                    }
                }

                if (!conflictShown) {
                    constexpr uint32_t activeKeybindCount = static_cast<uint32_t>(AddonImGui::INVOCATION_DOWN);
                    for (uint32_t i = 0; i < activeKeybindCount; ++i) {
                        if (instance.GetKeybinding(static_cast<AddonImGui::Keybind>(i)) == group.getToggleKey()) {
                            ImGui::TextDisabled("Warning: shortcut conflicts with REST action '%s'.", AddonImGui::KeybindDisplayNames[i]);
                            break;
                        }
                    }
                }
            }

            if (group.isEditing()) {
                ImGui::Separator();
                ImGui::Text("Edit group %d", group.getId());

                char tmpBuffer[150] = {};
                const std::string name = group.getName();
                strncpy_s(tmpBuffer, 150, name.c_str(), name.size());
                ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.7f);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Name");
                ImGui::SameLine(ImGui::GetWindowWidth() * 0.2f);
                ImGui::InputText("##Name", tmpBuffer, 149);
                group.setName(tmpBuffer);
                ImGui::PopItemWidth();

                ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.7f);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Key shortcut");
                ImGui::SameLine(ImGui::GetWindowWidth() * 0.2f);

                uint32_t keys = group.getToggleKey();
                if (key_input_box(ShaderToggler::reshade_key_name(keys).c_str(), &keys, runtime))
                    group.setToggleKey(keys);
                ImGui::PopItemWidth();

                if (ImGui::Button("OK"))
                    group.setEditing(false);
                ImGui::Separator();
            }

            ImGui::PopID();
        }

        for (const int sourceId : toClone)
            instance.CloneToggleGroup(sourceId);

        if (!toRemove.empty()) {
            instance.GetToggleGroupIdEffectEditing() = -1;
            instance.GetToggleGroupIdSettingsOpen() = -1;
            instance.GetToggleGroupIdShaderEditing() = -1;
            instance.GetToggleGroupIdConstantEditing() = -1;
            instance.StopHuntingMode();
        }

        for (const auto* group : toRemove) {
            instance.SignalToggleGroupRemoved(runtime, const_cast<ShaderToggler::ToggleGroup*>(group));
            const int id = group->getId();
            std::erase_if(instance.GetToggleGroups(), [id](const auto& item) { return item.first == id; });
        }

        if (!toRemove.empty())
            instance.UpdateToggleGroupsForShaderHashes();

        ImGui::Separator();

        const bool dirty = instance.IsConfigDirty();
        if (!dirty)
            ImGui::BeginDisabled();
        if (ImGui::Button("Save changes"))
            instance.SaveShaderTogglerIniFile();
        if (!dirty)
            ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::TextDisabled(dirty ? "Changes have not been written to ReshadeEffectShaderToggler.ini." : "Configuration is up to date.");
    }
}
