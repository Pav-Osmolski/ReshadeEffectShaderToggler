#pragma once
#include "CDataFile.h"
#include <cstring>

// Configuration may change in the overlay; image-creation policy stays frozen
// until the next process launch, including when the configuration is reloaded.
class VulkanSceneTransferSettings {
public:
    void Load(CDataFile& ini) { _configured = ini.GetBoolOrDefault("VulkanSceneTransfers", "General", false); }
    void Save(CDataFile& ini) const { ini.SetBool("VulkanSceneTransfers", _configured, "", "General"); }
    bool SetConfigured(bool value) {
        if (_configured == value) return false;
        _configured = value;
        return true;
    }
    void Initialize(const char* environment) {
        if (_initialized) return;
        _initialized = true;
        if (environment != nullptr && std::strcmp(environment, "1") == 0) _override = 1;
        else if (environment != nullptr && std::strcmp(environment, "0") == 0) _override = 0;
        _active = _override == -1 ? _configured : _override == 1;
    }
    bool Configured() const { return _configured; }
    bool Active() const { return _active; }
    bool HasOverride() const { return _override != -1; }
private:
    bool _configured = false;
    bool _active = false;
    bool _initialized = false;
    int _override = -1;
};
