#include "VulkanSceneTransferSettings.h"
#include <stdexcept>

static void require(bool condition) {
    if (!condition) throw std::runtime_error("Vulkan scene-transfer settings regression");
}

int main() {
    CDataFile oldConfig;
    require(oldConfig.LoadFromString("[General]\nTrackDescriptors=True\n"));
    VulkanSceneTransferSettings settings;
    settings.Load(oldConfig);
    settings.Initialize(nullptr);
    require(!settings.Configured() && !settings.Active() && !settings.HasOverride());
    require(settings.SetConfigured(true));
    require(!settings.SetConfigured(true));
    require(!settings.Active()); // UI edits cannot mutate image-creation policy.
    CDataFile saved;
    settings.Save(saved);
    require(saved.GetBoolOrDefault("VulkanSceneTransfers", "General", false));
    VulkanSceneTransferSettings restarted;
    restarted.Load(saved);
    restarted.Initialize(nullptr);
    require(restarted.Configured() && restarted.Active());
    restarted.SetConfigured(false);
    restarted.Save(saved);
    require(restarted.Active()); // Disabling also waits for restart.
    settings.Load(saved);
    settings.Initialize("1");
    require(!settings.Active()); // Reload/reinitialization cannot change startup policy.

    for (const char* value : { "0", "1", "invalid", "", "11" }) {
        VulkanSceneTransferSettings overridden;
        overridden.SetConfigured(true);
        overridden.Initialize(value);
        require(overridden.Active() == (std::strcmp(value, "0") != 0));
        require(overridden.HasOverride() == (std::strcmp(value, "0") == 0 || std::strcmp(value, "1") == 0));
        overridden.Save(saved);
        require(saved.GetBoolOrDefault("VulkanSceneTransfers", "General", false)); // Override never overwrites preference.
    }
    VulkanSceneTransferSettings forcedOn;
    forcedOn.Initialize("1");
    require(!forcedOn.Configured() && forcedOn.Active() && forcedOn.HasOverride());
}
