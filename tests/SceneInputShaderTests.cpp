#include "ClassicShader.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

static void require(bool value) {
    if (!value) { std::fputs("Scene-input shader eligibility regression\n", stderr); std::exit(EXIT_FAILURE); }
}
int main(int argc, char** argv) {
    uint32_t shader[] = {0x43425844,0,0,0,0,1,96,2,40,56, // DXBC container.
        0x30494653,8,0,0,                               // SFI0, no direct heap indexing.
        0x4c495844,32,0x60,8,0x4c495844,0x100,16,8,    // DXIL SM 6.0.
        0xdec04342,0};                                  // Bounded bitcode payload.
    require(Rendering::SupportsSceneInputShader(shader, sizeof(shader)));
    require(!Rendering::IsClassicDxbc(shader, sizeof(shader)));
    shader[16] = 0x65; require(Rendering::SupportsSceneInputShader(shader, sizeof(shader)));
    shader[16] = 0x66; require(!Rendering::SupportsSceneInputShader(shader, sizeof(shader)));
    shader[16] = 0x60;
    shader[12] = 0x02000000; require(!Rendering::SupportsSceneInputShader(shader, sizeof(shader)));
    shader[12] = 0x04000000; require(!Rendering::SupportsSceneInputShader(shader, sizeof(shader)));
    shader[12] = 0;
    shader[10] = 0; require(!Rendering::SupportsSceneInputShader(shader, sizeof(shader))); shader[10] = 0x30494653;
    shader[20] = 32; require(!Rendering::SupportsSceneInputShader(shader, sizeof(shader))); shader[20] = 16;
    shader[21] = 128; require(!Rendering::SupportsSceneInputShader(shader, sizeof(shader))); shader[21] = 8;
    shader[22] = 0; require(!Rendering::SupportsSceneInputShader(shader, sizeof(shader))); shader[22] = 0xdec04342;
    require(!Rendering::SupportsSceneInputShader(shader, sizeof(shader) - 1));
    // Optional local-only capture fixtures are never copied into the repository
    // or distributed with builds. CI covers the policy with synthetic containers.
    for (int i = 1; i < argc; ++i) {
        std::ifstream file(argv[i], std::ios::binary);
        require(file.good());
        const std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        const bool supported = Rendering::SupportsSceneInputShader(bytes.data(), bytes.size());
        std::printf("Captured shader: %s | supported=%d | old classic-only=%d\n", argv[i], supported,
                    Rendering::IsClassicDxbc(bytes.data(), bytes.size()));
        require(supported);
    }
    std::puts("Scene-input shader eligibility tests passed.");
}
