#pragma once
#include "GlobalResourceView.h"
#include <memory>
#include <vector>

namespace Rendering {
struct SceneInputLease {
    reshade::api::device* device = nullptr;
    reshade::api::resource texture = { 0 };
    std::shared_ptr<GlobalResourceView> views;
    std::vector<reshade::api::descriptor_table> tables;
    std::vector<uint32_t> sizes;
    uint32_t width = 0, height = 0;
    reshade::api::format format = reshade::api::format::unknown;
    bool shaderReadable = false;
    void Dispose();
    ~SceneInputLease() { Dispose(); }
};
class SceneInputPool;
void RefreshSceneInputDescriptors(reshade::api::command_list*);

// These functions operate only on D3D12. They do not transition or write the
// game's input, and never update a descriptor in a game-owned heap.
std::shared_ptr<SceneInputLease> PrepareSceneInput(reshade::api::command_list*,
                                                   uint32_t slot,
                                                   uint32_t index,
                                                   reshade::api::resource_view& source,
                                                   const char*& status);
void BindSceneInput(reshade::api::command_list*);
void RestoreSceneInput(reshade::api::command_list*);
void RetireSceneInputs(reshade::api::command_list*);
void DisposeSceneInputs(reshade::api::device*);
void SceneInputSecondary(reshade::api::command_list*, reshade::api::command_list*);
}
