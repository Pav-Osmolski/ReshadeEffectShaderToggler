#pragma once
#include <cstdint>
#include <cstddef>
#include <unordered_map>

namespace Rendering {
// Recording-order hints only: a bound RTV does not prove that pixels were written.
// Never combine histories from different command lists or recording generations.
class ProducerTrace {
public:
    struct Draw { uint32_t ps = 0, vs = 0, slot = 0; uint64_t ordinal = 0; };
    static constexpr std::size_t capacity = 512;
    static constexpr uint64_t drawLimit = 32768;
    void Reset(const char* why = "command-list reset") {
        draws.clear(); ordinal = 0; clipped = false; reason = why;
    }
    void Sync(uint64_t value) {
        if (generation != value) { Reset("tracking toggle or resource lifetime changed"); generation = value; }
    }
    void Forget(uint64_t resource, const char* why) { draws.erase(resource); reason = why; }
    bool BeginDraw() {
        if (ordinal >= drawLimit) { draws.clear(); clipped = true; return false; }
        ++ordinal; return true;
    }
    void Record(uint64_t resource, uint32_t ps, uint32_t vs, uint32_t slot) {
        if (!resource || clipped) return;
        auto it = draws.find(resource);
        if (it == draws.end() && draws.size() >= capacity) { clipped = true; draws.clear(); return; }
        draws[resource] = Draw{ps, vs, slot, ordinal};
    }
    const Draw* Find(uint64_t resource) const {
        const auto it = draws.find(resource); return it == draws.end() ? nullptr : &it->second;
    }
    uint64_t ordinal = 0;
    bool clipped = false;
    const char* reason = "no earlier observed draw";
private:
    uint64_t generation = 0;
    std::unordered_map<uint64_t, Draw> draws;
};
}
