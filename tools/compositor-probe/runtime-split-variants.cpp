#include "../../src/WarframeShaders.h"
#include <cstdio>
#include <fstream>

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: runtime-split-variants capture-directory pinned-dxc-directory\n");
        return 2;
    }
    const std::filesystem::path folder = argv[1];
    std::ifstream source(folder / "original.shader", std::ios::binary);
    const std::vector<char> bytes((std::istreambuf_iterator<char>(source)), {});
    Rendering::WarframeShaderPair pair;
    const char* status = nullptr;
    if (!Rendering::BuildWarframeShaderPair(argv[2], bytes.data(), bytes.size(), pair, status)) {
        fprintf(stderr, "%s\n", status);
        return 1;
    }
    std::ofstream scene(folder / "runtime-scene.dxil", std::ios::binary);
    std::ofstream ui(folder / "runtime-ui.dxil", std::ios::binary);
    scene.write(pair.scene.data(), pair.scene.size());
    ui.write(pair.ui.data(), pair.ui.size());
    return scene && ui ? 0 : 1;
}
