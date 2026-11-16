#pragma once
#include "VulkanTraceBudget.h"
#include <reshade.hpp>
#include <cstdio>
#include <Windows.h>

class VulkanBoundaryTrace {
public:
    void arm() {
        static const bool enabled = [] {
            char value[2] = {};
            return GetEnvironmentVariableA("REST_VULKAN_BOUNDARY_TRACE", value, sizeof(value)) == 1 && value[0] == '1';
        }();
        if (enabled && _budget.start())
            reshade::log::message(reshade::log::level::info,
              "REST Vulkan trace: START; up to 512 events / 3 presents. begin a=load b=flags c=inside|endPending<<1; barrier a=old b=new c=state; queue a=width b=height c=usage; ps-draw a=hash. Events describe callbacks, not proven native pass boundaries.");
    }
    bool active() const { return _budget.active(); }
    void event(const char* name, const void* cmd, uint64_t target = 0,
               uint64_t a = 0, uint64_t b = 0, uint64_t c = 0) {
        bool finalLine = false;
        if (!_budget.take(&finalLine)) return;
        char text[384];
        std::snprintf(text, sizeof(text),
          "REST Vulkan trace: %s cmd=%p target=0x%llx a=0x%llx b=0x%llx c=0x%llx",
          name, cmd, static_cast<unsigned long long>(target),
          static_cast<unsigned long long>(a), static_cast<unsigned long long>(b), static_cast<unsigned long long>(c));
        reshade::log::message(reshade::log::level::info, text);
        if (finalLine)
            reshade::log::message(reshade::log::level::info, "REST Vulkan trace: event limit reached; remaining events omitted.");
    }
    void present(uint64_t pending) {
        event("present-clear", nullptr, 0, pending);
        if (_budget.finishFrame())
            reshade::log::message(reshade::log::level::info, "REST Vulkan trace: END after 3 presents; capture will not restart on this device.");
    }
private:
    VulkanTraceBudget _budget;
};
