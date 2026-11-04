#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Rendering {
// Table-based SM 6.0-6.5 DXIL is eligible too. SM 6.6 introduced direct heap
// indexing, which this path cannot redirect, so newer models stay unsupported.
// https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_DynamicResources.html
inline bool SupportsSceneInputShader(const void* code, size_t size, bool allowDxil = true) {
    if (!code || size < 32)
        return false;
    const auto* bytes = static_cast<const uint8_t*>(code);
    auto read = [&](size_t offset) {
        uint32_t value;
        std::memcpy(&value, bytes + offset, 4);
        return value;
    };
    if (read(0) != 0x43425844 || read(24) != size)
        return false;
    const uint32_t chunks = read(28);
    if (chunks == 0 || chunks > (size - 32) / 4)
        return false;
    bool classic = false, dxil = false, features = false;
    for (uint32_t i = 0; i < chunks; ++i) {
        const uint32_t offset = read(32 + i * 4);
        if (offset < 32 + chunks * 4ull || offset > size - 8 || read(offset + 4) > size - offset - 8)
            return false;
        const uint32_t tag = read(offset);
        const uint32_t payload = read(offset + 4);
        if (tag == 0x30494653) { // SFI0: shader feature requirements.
            if (features || payload != 8) return false;
            features = true;
            // D3D_SHADER_FEATURE_{RESOURCE,SAMPLER}_DESCRIPTOR_HEAP_INDEXING.
            if ((read(offset + 8) & 0x06000000u) != 0) return false;
        }
        if (tag == 0x4c495844) { // DXIL program and bitcode headers.
            if (!allowDxil || dxil || payload < 28 || payload % 4 != 0) return false;
            const size_t data = static_cast<size_t>(offset) + 8;
            const uint32_t model = read(data) & 0xffff;
            if (model < 0x60 || model > 0x65 || read(data + 4) * 4ull != payload || read(data + 8) != 0x4c495844)
                return false;
            const uint64_t bitcodeOffset = 8ull + read(data + 16);
            const uint32_t bitcodeSize = read(data + 20);
            if (bitcodeOffset < 24 || bitcodeOffset > payload || bitcodeSize < 4 || bitcodeSize > payload - bitcodeOffset ||
                read(data + static_cast<size_t>(bitcodeOffset)) != 0xdec04342)
                return false;
            dxil = true;
        }
        if (tag == 0x52444853 || tag == 0x58454853) { // SHDR / SHEX.
            if (classic) return false;
            classic = true;
        }
    }
    return classic != dxil && (!dxil || features);
}
inline bool IsClassicDxbc(const void* code, size_t size) {
    return SupportsSceneInputShader(code, size, false);
}
}
