#pragma once
#include <reshade_api.hpp>

namespace Rendering {
// render_technique has a void return and can silently enqueue an effect variant
// instead of drawing it. Observe ReShade's post-technique event for this exact
// call, rather than counting every API call as a rendered technique.
struct SceneTechniqueObservation {
    reshade::api::effect_runtime* runtime;
    reshade::api::effect_technique technique;
    reshade::api::command_list* command;
    reshade::api::resource_view target;
    bool recorded = false;
    SceneTechniqueObservation* previous;
    inline static thread_local SceneTechniqueObservation* active = nullptr;
    SceneTechniqueObservation(reshade::api::effect_runtime* r, reshade::api::effect_technique t,
                              reshade::api::command_list* c, reshade::api::resource_view v)
      : runtime(r), technique(t), command(c), target(v), previous(active) { active = this; }
    ~SceneTechniqueObservation() { active = previous; }
    SceneTechniqueObservation(const SceneTechniqueObservation&) = delete;
    SceneTechniqueObservation& operator=(const SceneTechniqueObservation&) = delete;
    static void OnRendered(reshade::api::effect_runtime* r, reshade::api::effect_technique t,
                           reshade::api::command_list* c, reshade::api::resource_view v, reshade::api::resource_view) {
        if (active && active->runtime == r && active->technique == t && active->command == c && active->target == v)
            active->recorded = true;
    }
};
}
