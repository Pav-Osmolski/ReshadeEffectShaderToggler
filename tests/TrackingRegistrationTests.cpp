#include "StateTracking.h"
#include <map>
#include <set>
#include <stdexcept>

static std::map<reshade::addon_event, std::set<void*>> registered;
extern "C" __declspec(dllexport) bool ReShadeRegisterAddon(void*, uint32_t) { return true; }
extern "C" __declspec(dllexport) void ReShadeUnregisterAddon(void*) {}
extern "C" __declspec(dllexport) void ReShadeLogMessage(void*, int, const char*) {}
extern "C" __declspec(dllexport) void ReShadeRegisterEvent(reshade::addon_event event, void* callback) { registered[event].insert(callback); }
extern "C" __declspec(dllexport) void ReShadeUnregisterEvent(reshade::addon_event event, void* callback) { registered[event].erase(callback); }
extern "C" __declspec(dllexport) void ReShadeRegisterEventForAddon(void*, reshade::addon_event event, void* callback) { ReShadeRegisterEvent(event, callback); }
extern "C" __declspec(dllexport) void ReShadeUnregisterEventForAddon(void*, reshade::addon_event event, void* callback) { ReShadeUnregisterEvent(event, callback); }
static void require(bool value) { if (!value) throw std::runtime_error("Tracking registration regression"); }

int main() {
    using reshade::addon_event;
    state_tracking::register_events(true);
    require(state_tracking::descriptor_tracking_enabled());
    require(registered[addon_event::copy_descriptor_sets].size() == 1);
    require(registered[addon_event::update_descriptor_sets].size() == 1);
    require(registered[addon_event::bind_descriptor_sets].size() == 1);
    void* tracked = *registered[addon_event::bind_descriptor_sets].begin();
    state_tracking::unregister_events();
    require(!state_tracking::descriptor_tracking_enabled());
    for (const auto& entry : registered) require(entry.second.empty());
    state_tracking::register_events(false);
    require(!state_tracking::descriptor_tracking_enabled());
    require(registered[addon_event::copy_descriptor_sets].empty());
    require(registered[addon_event::update_descriptor_sets].empty());
    require(registered[addon_event::bind_descriptor_sets].size() == 1);
    require(*registered[addon_event::bind_descriptor_sets].begin() != tracked);
    state_tracking::unregister_events();
    for (const auto& entry : registered) require(entry.second.empty());
}
