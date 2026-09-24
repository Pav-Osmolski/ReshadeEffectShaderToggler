#pragma once

// REST Enhanced Legacy targets the ReShade 5.8.0 add-on API (API 7).
// Keep legacy naming adaptations isolated here so the modern branch remains clean.

#include <imgui.h>
#include <reshade.hpp>

#if RESHADE_API_VERSION == 7

namespace reshade {
namespace log {
enum class level {
    error = 1,
    warning = 2,
    info = 3,
    debug = 4,
};

inline void message(level severity, const char* message) {
    reshade::log_message(static_cast<reshade::log_level>(severity), message);
}
}

inline bool get_config_value(api::effect_runtime* runtime,
                             const char* section,
                             const char* key,
                             char* value,
                             size_t* value_size) {
    return reshade::config_get_value(runtime, section, key, value, value_size);
}
}

#ifndef ImGuiChildFlags_AlwaysAutoResize
#define ImGuiChildFlags_AlwaysAutoResize ImGuiWindowFlags_AlwaysAutoResize
#endif

#endif
