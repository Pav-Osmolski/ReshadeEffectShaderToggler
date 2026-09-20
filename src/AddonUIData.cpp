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

#include <format>
#include <functional>
#include "AddonUIData.h"
#include "RenderingManager.h"
#include <algorithm>
#include <cfloat>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <vector>
#include <windows.h>

using namespace AddonImGui;
using namespace reshade::api;
using namespace ShaderToggler;
using namespace Shim::Constants;
using namespace std;

AddonUIData::AddonUIData(ShaderManager* pixelShaderManager, ShaderManager* vertexShaderManager, ShaderManager* computeShaderManager, ConstantHandlerBase* cHandler, atomic_uint32_t* activeCollectorFrameCounter):
    _pixelShaderManager(pixelShaderManager), _vertexShaderManager(vertexShaderManager), _computeShaderManager(computeShaderManager), _activeCollectorFrameCounter(activeCollectorFrameCounter),
    _constantHandler(cHandler)
{
    _toggleGroupIdShaderEditing = -1;
    _overlayOpacity = 0.2f;

    _keyBindings[Keybind::PIXEL_SHADER_DOWN] = VK_NUMPAD1;
    _keyBindings[Keybind::PIXEL_SHADER_UP] = VK_NUMPAD2;
    _keyBindings[Keybind::PIXEL_SHADER_MARK] = VK_NUMPAD3;
    _keyBindings[Keybind::PIXEL_SHADER_MARKED_DOWN] = VK_NUMPAD1 | (VK_CONTROL << 8);
    _keyBindings[Keybind::PIXEL_SHADER_MARKED_UP] = VK_NUMPAD2 | (VK_CONTROL << 8);
    _keyBindings[Keybind::PIXEL_SHADER_MARK_PREV] = 0;
    _keyBindings[Keybind::PIXEL_SHADER_MARK_NEXT] = 0;
    _keyBindings[Keybind::VERTEX_SHADER_DOWN] = VK_NUMPAD4;
    _keyBindings[Keybind::VERTEX_SHADER_UP] = VK_NUMPAD5;
    _keyBindings[Keybind::VERTEX_SHADER_MARK] = VK_NUMPAD6;
    _keyBindings[Keybind::VERTEX_SHADER_MARKED_DOWN] = VK_NUMPAD4 | (VK_CONTROL << 8);
    _keyBindings[Keybind::VERTEX_SHADER_MARKED_UP] = VK_NUMPAD5 | (VK_CONTROL << 8);
    _keyBindings[Keybind::VERTEX_SHADER_MARK_PREV] = 0;
    _keyBindings[Keybind::VERTEX_SHADER_MARK_NEXT] = 0;
    _keyBindings[Keybind::COMPUTE_SHADER_DOWN] = 0;
    _keyBindings[Keybind::COMPUTE_SHADER_UP] = 0;
    _keyBindings[Keybind::COMPUTE_SHADER_MARK] = 0;
    _keyBindings[Keybind::COMPUTE_SHADER_MARKED_DOWN] = 0;
    _keyBindings[Keybind::COMPUTE_SHADER_MARKED_UP] = 0;
    _keyBindings[Keybind::COMPUTE_SHADER_MARK_PREV] = 0;
    _keyBindings[Keybind::COMPUTE_SHADER_MARK_NEXT] = 0;
    _keyBindings[Keybind::INVOCATION_DOWN] = VK_NUMPAD7;
    _keyBindings[Keybind::INVOCATION_UP] = VK_NUMPAD8;
    _keyBindings[Keybind::DESCRIPTOR_DOWN] = VK_SUBTRACT;
    _keyBindings[Keybind::DESCRIPTOR_UP] = VK_ADD;

    MarkConfigClean();
}


unordered_map<int, ToggleGroup>& AddonUIData::GetToggleGroups()
{
    return _toggleGroups;
}

void AddonUIData::AddToggleGroupRemovalCallback(std::function<void(reshade::api::effect_runtime*, ShaderToggler::ToggleGroup*)> callback)
{
    _removalCallbacks.push_back(callback);
}

void AddonUIData::SignalToggleGroupRemoved(reshade::api::effect_runtime* runtime, ShaderToggler::ToggleGroup* group)
{
    for (auto& func : _removalCallbacks)
    {
        func(runtime, group);
    }
}

void AddonUIData::AssignPreferredGroupTechniques(std::unordered_map<std::string, EffectData>& allTechniques)
{
    for (auto& it : _toggleGroups)
    {
        it.second.AssignPreferredTechniqueData(allTechniques);
    }
}

vector<ToggleGroup*> AddonUIData::GetToggleGroupsForPixelShaderHash(uint32_t hash) const
{
    shared_lock lock(_shaderHashGroupsMutex);
    const auto it = _pixelShaderHashToToggleGroups.find(hash);
    return it != _pixelShaderHashToToggleGroups.end() ? it->second : vector<ToggleGroup*>{};
}

vector<ToggleGroup*> AddonUIData::GetToggleGroupsForVertexShaderHash(uint32_t hash) const
{
    shared_lock lock(_shaderHashGroupsMutex);
    const auto it = _vertexShaderHashToToggleGroups.find(hash);
    return it != _vertexShaderHashToToggleGroups.end() ? it->second : vector<ToggleGroup*>{};
}

vector<ToggleGroup*> AddonUIData::GetToggleGroupsForComputeShaderHash(uint32_t hash) const
{
    shared_lock lock(_shaderHashGroupsMutex);
    const auto it = _computeShaderHashToToggleGroups.find(hash);
    return it != _computeShaderHashToToggleGroups.end() ? it->second : vector<ToggleGroup*>{};
}

void AddonUIData::UpdateToggleGroupsForShaderHashes()
{
    unique_lock lock(_shaderHashGroupsMutex);

    _pixelShaderHashToToggleGroups.clear();
    _vertexShaderHashToToggleGroups.clear();
    _computeShaderHashToToggleGroups.clear();

    for (auto& [_,group] : _toggleGroups)
    {
        // Only consider the currently hunted hash for the group being edited
        if (group.getId() == _toggleGroupIdShaderEditing && (_pixelShaderManager->isInHuntingMode() || _vertexShaderManager->isInHuntingMode() || _computeShaderManager->isInHuntingMode()))
        {
            if (_pixelShaderManager->isInHuntingMode())
            {
                const uint32_t hash = _pixelShaderManager->getActiveHuntedShaderHash();
                if (hash != 0)
                    _pixelShaderHashToToggleGroups[hash].push_back(&group);
            }

            if (_vertexShaderManager->isInHuntingMode())
            {
                const uint32_t hash = _vertexShaderManager->getActiveHuntedShaderHash();
                if (hash != 0)
                    _vertexShaderHashToToggleGroups[hash].push_back(&group);
            }

            if (_computeShaderManager->isInHuntingMode())
            {
                const uint32_t hash = _computeShaderManager->getActiveHuntedShaderHash();
                if (hash != 0)
                    _computeShaderHashToToggleGroups[hash].push_back(&group);
            }

            continue;
        }

        for (const auto& h : group.getPixelShaderHashes())
            _pixelShaderHashToToggleGroups[h].push_back(&group);

        for (const auto& h : group.getVertexShaderHashes())
            _vertexShaderHashToToggleGroups[h].push_back(&group);

        for (const auto& h : group.getComputeShaderHashes())
            _computeShaderHashToToggleGroups[h].push_back(&group);
    }
}

const atomic_int& AddonUIData::GetToggleGroupIdShaderEditing() const
{
    return _toggleGroupIdShaderEditing;
}

void AddonUIData::StopHuntingMode()
{
    _pixelShaderManager->stopHuntingMode();
    _vertexShaderManager->stopHuntingMode();
    _computeShaderManager->stopHuntingMode();
}


/// <summary>
/// Adds a default group with VK_CAPITAL as toggle key. Only used if there aren't any groups defined in the ini file.
/// </summary>
void AddonUIData::AddDefaultGroup()
{
    ToggleGroup toAdd("Default", ToggleGroup::getNewGroupId());
    toAdd.setToggleKey(0);
    toAdd.SetConfigDirtyFlag(&_configDirty);
    _toggleGroups.emplace(toAdd.getId(), toAdd);
    MarkConfigDirty();
}

ToggleGroup* AddonUIData::CloneToggleGroup(int sourceGroupId)
{
    const auto source = _toggleGroups.find(sourceGroupId);
    if (source == _toggleGroups.end())
        return nullptr;

    const int newId = ToggleGroup::getNewGroupId();
    ToggleGroup clone = source->second.cloneForNewId(newId);
    clone.SetConfigDirtyFlag(&_configDirty);
    _toggleGroups.emplace(newId, std::move(clone));
    UpdateToggleGroupsForShaderHashes();
    MarkConfigDirty();
    return &_toggleGroups.at(newId);
}


/// <summary>
/// Loads the defined hashes and groups from the shaderToggler.ini file.
/// </summary>
void AddonUIData::LoadShaderTogglerIniFile(const string& fileName)
{
    // Will assume it's started at the start of the application and therefore no groups are present.

    reshade::log::message(reshade::log::level::info, std::format("Loading config file from \"{}\"", (_basePath / fileName).string()).c_str());

    CDataFile iniFile;
    if (!iniFile.Load((_basePath / fileName).string()))
    {
        reshade::log::message(reshade::log::level::info, std::format("Could not find config file at \"{}\"", (_basePath / fileName).string()).c_str());
        MarkConfigClean();
        // not there
        return;
    }

    const int configVersion = iniFile.GetInt("ConfigVersion", "General");
    if (configVersion != INT_MIN && configVersion > REST_CONFIG_VERSION) {
        reshade::log::message(reshade::log::level::warning,
          std::format("Configuration version {} is newer than supported version {}.", configVersion, REST_CONFIG_VERSION).c_str());
    }

    _trackDescriptors = iniFile.GetBoolOrDefault("TrackDescriptors", "General", true);

    const float overlayOpacity = iniFile.GetFloat("OverlayOpacity", "General");
    if (overlayOpacity != FLT_MIN)
        _overlayOpacity = std::clamp(overlayOpacity, 0.0f, 1.0f);

    const int collectionFrames = iniFile.GetInt("ShaderCollectionFrames", "General");
    if (collectionFrames != INT_MIN)
        _startValueFramecountCollectionPhase = std::clamp(collectionFrames, 10, 1000);

    _resourceShim = iniFile.GetValue("ResourceShim", "General");
    if (_resourceShim.size() <= 0)
    {
        _resourceShim = Rendering::ResourceShimNames[0];
    }

    _constHookType = iniFile.GetValue("ConstantBufferHookType", "General");
    if (_constHookType.size() <= 0)
    {
        _constHookType = "default";
    }

    _constHookCopyType = iniFile.GetValue("ConstantBufferHookCopyType", "General");
    if (_constHookCopyType.size() <= 0)
    {
        _constHookCopyType = "gpu_readback";
    }

    _preventRuntimeReload = iniFile.GetBoolOrDefault("PreventRuntimeReload", "General", false);

    for (uint32_t i = 0; i < ARRAYSIZE(KeybindNames); i++)
    {
        uint32_t keybinding = iniFile.GetUInt(KeybindNames[i], "Keybindings");
        if (keybinding != UINT_MAX)
        {
            _keyBindings[i] = keybinding;
        }
    }

    int groupCounter = 0;
    const int numberOfGroups = iniFile.GetInt("AmountGroups", "General");
    if (numberOfGroups == INT_MIN)
    {
        // old format file?
        AddDefaultGroup();
        groupCounter = -1;	// enforce old format read for pre 1.0 ini file.
    }
    else
    {
        for (int i = 0; i < numberOfGroups; i++)
        {
            int nId = ToggleGroup::getNewGroupId();
            const auto& xx = _toggleGroups.emplace(nId, ToggleGroup{"", nId });
        }
    }
    for (auto& [_,group] : _toggleGroups)
    {
        group.loadState(iniFile, groupCounter);
        group.SetConfigDirtyFlag(&_configDirty);		// groupCounter is normally 0 or greater. For when the old format is detected, it's -1 (and there's 1 group).
        groupCounter++;

        for (const auto& h : group.getPixelShaderHashes())
        {
            _pixelShaderHashToToggleGroups[h].push_back(&group);
        }

        for (const auto& h : group.getVertexShaderHashes())
        {
            _vertexShaderHashToToggleGroups[h].push_back(&group);
        }

        for (const auto& h : group.getComputeShaderHashes())
        {
            _computeShaderHashToToggleGroups[h].push_back(&group);
        }
    }

    MarkConfigClean();
}

/// <summary>
/// Saves the currently known toggle groups with their shader hashes to the shadertoggler.ini file
/// </summary>
void AddonUIData::SaveShaderTogglerIniFile(const string& fileName)
{
    CDataFile iniFile;

    iniFile.SetInt("ConfigVersion", REST_CONFIG_VERSION, "", "General");
    iniFile.SetValue("ResourceShim", _resourceShim, "", "General");
    iniFile.SetValue("ConstantBufferHookType", _constHookType, "", "General");
    iniFile.SetValue("ConstantBufferHookCopyType", _constHookCopyType, "", "General");
    iniFile.SetBool("TrackDescriptors", _trackDescriptors, "", "General");
    iniFile.SetFloat("OverlayOpacity", _overlayOpacity, "", "General");
    iniFile.SetInt("ShaderCollectionFrames", _startValueFramecountCollectionPhase, "", "General");
    iniFile.SetBool("PreventRuntimeReload", _preventRuntimeReload, "", "General");

    for (uint32_t i = 0; i < ARRAYSIZE(KeybindNames); i++)
        iniFile.SetUInt(KeybindNames[i], _keyBindings[i], "", "Keybindings");

    iniFile.SetInt("AmountGroups", static_cast<int>(_toggleGroups.size()), "", "General");

    int groupCounter = 0;
    for (const auto& [_, group] : _toggleGroups)
        group.saveState(iniFile, groupCounter++);

    const std::filesystem::path targetPath = _basePath / fileName;
    const std::filesystem::path tempPath = targetPath.string() + ".tmp";
    const std::filesystem::path backupPath = targetPath.string() + ".bak";
    std::error_code ec;
    std::filesystem::remove(tempPath, ec);

    iniFile.SetFileName(tempPath.string());
    if (!iniFile.Save() || !std::filesystem::exists(tempPath, ec) || std::filesystem::file_size(tempPath, ec) == 0) {
        reshade::log::message(reshade::log::level::error,
          std::format("Failed to write temporary configuration file \"{}\"", tempPath.string()).c_str());
        std::filesystem::remove(tempPath, ec);
        return;
    }

    CDataFile verifier;
    const int expectedGroupCount = static_cast<int>(_toggleGroups.size());
    if (!verifier.Load(tempPath.string()) ||
        verifier.GetInt("ConfigVersion", "General") != REST_CONFIG_VERSION ||
        verifier.GetInt("AmountGroups", "General") != expectedGroupCount) {
        reshade::log::message(reshade::log::level::error,
          std::format("Temporary configuration verification failed for \"{}\"", tempPath.string()).c_str());
        std::filesystem::remove(tempPath, ec);
        return;
    }

    if (std::filesystem::exists(targetPath, ec)) {
        ec.clear();
        std::filesystem::copy_file(targetPath, backupPath, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            reshade::log::message(reshade::log::level::warning,
              std::format("Could not refresh configuration backup \"{}\": {}", backupPath.string(), ec.message()).c_str());
        }
    }

    if (!MoveFileExW(tempPath.c_str(), targetPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        reshade::log::message(reshade::log::level::error,
          std::format("Failed to atomically replace configuration file \"{}\" (Win32 error {}).",
                      targetPath.string(), GetLastError()).c_str());
        std::filesystem::remove(tempPath, ec);
        return;
    }

    reshade::log::message(reshade::log::level::info,
      std::format("Saved configuration to \"{}\" (backup: \"{}\")", targetPath.string(), backupPath.string()).c_str());
    MarkConfigClean();
}

std::string AddonUIData::ExportToggleGroup(const ToggleGroup& group) const
{
    CDataFile data;
    data.SetInt("FormatVersion", 1, "", "RESTGroupExport");
    group.saveState(data, 0);
    return data.Serialize();
}

ToggleGroup* AddonUIData::ImportToggleGroup(const std::string& serialized)
{
    CDataFile data;
    if (!data.LoadFromString(serialized) || data.GetValue("Name", "Group0").empty())
        return nullptr;

    const int newId = ToggleGroup::getNewGroupId();
    ToggleGroup imported("", newId);
    imported.loadState(data, 0);
    imported.SetConfigDirtyFlag(&_configDirty);
    imported.setEditing(false);

    _toggleGroups.emplace(newId, std::move(imported));
    UpdateToggleGroupsForShaderHashes();
    MarkConfigDirty();
    return &_toggleGroups.at(newId);
}

/// <summary>
/// Function which marks the end of a shader editing cycle for a given toggle group
/// </summary>
/// <param name="acceptCollectedShaderHashes"></param>
/// <param name="groupEditing"></param>
void AddonUIData::OpenGroupSettings(ToggleGroup& group)
{
    _toggleGroupIdSettingsOpen = group.getId();
}

void AddonUIData::CloseGroupSettings(bool acceptCollectedShaderHashes, ToggleGroup& group)
{
    if (_toggleGroupIdShaderEditing == group.getId())
        EndShaderEditing(acceptCollectedShaderHashes, group);

    _toggleGroupIdSettingsOpen = -1;
}

void AddonUIData::EndShaderEditing(bool acceptCollectedShaderHashes, ToggleGroup& groupEditing)
{
    if (acceptCollectedShaderHashes && _toggleGroupIdShaderEditing == groupEditing.getId())
    {
        groupEditing.storeCollectedHashes(_pixelShaderManager->getMarkedShaderHashes(),
                                          _vertexShaderManager->getMarkedShaderHashes(),
                                          _computeShaderManager->getMarkedShaderHashes());
        groupEditing.resetDebugAutoDiagnostics();
    }

    _pixelShaderManager->stopHuntingMode();
    _vertexShaderManager->stopHuntingMode();
    _computeShaderManager->stopHuntingMode();
    _toggleGroupIdShaderEditing = -1;

    UpdateToggleGroupsForShaderHashes();
}


/// <summary>
/// Function which marks the start of a shader editing cycle for a given toggle group.
/// </summary>
/// <param name="groupEditing"></param>
void AddonUIData::StartShaderEditing(ToggleGroup& groupEditing)
{
    _toggleGroupIdSettingsOpen = groupEditing.getId();

    if (_toggleGroupIdShaderEditing == groupEditing.getId())
    {
        return;
    }
    if (_toggleGroupIdShaderEditing >= 0)
    {
        EndShaderEditing(false, groupEditing);
    }
    _toggleGroupIdShaderEditing = groupEditing.getId();
    *_activeCollectorFrameCounter = _startValueFramecountCollectionPhase;
    _pixelShaderManager->startHuntingMode(groupEditing.getPixelShaderHashes());
    _vertexShaderManager->startHuntingMode(groupEditing.getVertexShaderHashes());
    _computeShaderManager->startHuntingMode(groupEditing.getComputeShaderHashes());

    // after copying them to the managers, we can now clear the group's shader.
    groupEditing.clearHashes();

    UpdateToggleGroupsForShaderHashes();
}


/// <summary>
/// Mark the given shader group for editing of it's effect whitelist
/// </summary>
/// <param name="groupEditing"></param>
void AddonUIData::StartEffectEditing(ToggleGroup& groupEditing)
{
    _toggleGroupIdEffectEditing = groupEditing.getId();
}

/// <summary>
/// End editing of the current shader group's effect whitelist
/// </summary>
/// <param name="groupEditing"></param>
void AddonUIData::EndEffectEditing()
{
    _toggleGroupIdEffectEditing = -1;
}

/// <summary>
/// Mark the given shader group for editing of it's constants
/// </summary>
/// <param name="groupEditing"></param>
void AddonUIData::StartConstantEditing(ToggleGroup& groupEditing)
{
    _toggleGroupIdConstantEditing = groupEditing.getId();
}

/// <summary>
/// End editing of the current shader group's constants
/// </summary>
/// <param name="groupEditing"></param>
void AddonUIData::EndConstantEditing()
{
    _toggleGroupIdConstantEditing = -1;
}

uint32_t AddonUIData::GetKeybinding(Keybind keybind) const
{
    return _keyBindings[keybind];
}

void AddonUIData::SetKeybinding(Keybind keybind, uint32_t keys)
{
    if (_keyBindings[keybind] != keys) {
        _keyBindings[keybind] = keys;
        MarkConfigDirty();
    }
}