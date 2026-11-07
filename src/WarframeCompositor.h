#pragma once

#include <reshade_api.hpp>
#include <filesystem>

namespace Rendering {
struct WarframeSplitPipeline {
    reshade::api::device* device = nullptr;
    reshade::api::pipeline_layout layout = {0};
    reshade::api::pipeline scene = {0};
    reshade::api::pipeline ui = {0};
    const char* status = "Warframe split: unsupported pipeline state";
    ~WarframeSplitPipeline();
};

void ConfigureWarframeSplit(const std::filesystem::path& executable, const std::filesystem::path& addon, bool enabled);
bool WarframeSplitAvailable();
bool WarframeSplitEnabled();
void InitWarframeSplitPipeline(reshade::api::device*, reshade::api::pipeline_layout, uint32_t count,
                              const reshade::api::pipeline_subobject*, reshade::api::pipeline);
}
