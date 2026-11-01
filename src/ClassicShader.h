#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Rendering {
// SM 6.6 direct heap indexing cannot be inferred from ReShade layout metadata.
// Restrict private-heap redirection to classic DXBC graphics pipelines.
inline bool IsClassicDxbc(const void* code, size_t size) {
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
    bool shader = false;
    for (uint32_t i = 0; i < chunks; ++i) {
        const uint32_t offset = read(32 + i * 4);
        if (offset > size - 8 || read(offset + 4) > size - offset - 8)
            return false;
        const uint32_t tag = read(offset);
        if (tag == 0x4c495844)
            return false;                                 // DXIL
        shader |= tag == 0x52444853 || tag == 0x58454853; // SHDR / SHEX
    }
    return shader;
}
}
