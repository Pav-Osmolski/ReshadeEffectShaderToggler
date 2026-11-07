#pragma once

#include <cstddef>
#include <filesystem>
#include <vector>

namespace Rendering {
enum class WarframeShader { unknown, normal, taa };
WarframeShader IdentifyWarframePixelShader(const void* code, size_t size);
bool IsWarframeVertexShader(const void* code, size_t size);

struct WarframeShaderPair {
    std::vector<char> scene;
    std::vector<char> ui;
};

// Uses the caller's live, exact-match DXIL. The generated pair is kept in
// memory and checked by the pinned Microsoft validator before PSO creation.
bool BuildWarframeShaderPair(const std::filesystem::path& compilerDirectory, const void* code, size_t size,
                            WarframeShaderPair& pair, const char*& status);
}
