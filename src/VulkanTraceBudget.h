#pragma once
#include <atomic>
#include <cstdint>

// One bounded capture per device. Logging never changes render eligibility.
class VulkanTraceBudget {
public:
    bool start(uint32_t lines = 512, uint32_t frames = 3) {
        bool expected = false;
        if (!_started.compare_exchange_strong(expected, true)) return false;
        _frames.store(frames);
        _lines.store(frames != 0 ? lines : 0);
        return true;
    }
    bool active() const { return _lines.load() != 0; }
    bool take(bool* finalLine = nullptr) {
        auto remaining = _lines.load();
        while (remaining != 0)
            if (_lines.compare_exchange_weak(remaining, remaining - 1)) {
                if (finalLine != nullptr) *finalLine = remaining == 1;
                return true;
            }
        return false;
    }
    bool finishFrame() {
        auto frames = _frames.load();
        while (frames != 0) {
            if (_frames.compare_exchange_weak(frames, frames - 1)) {
                if (frames == 1) { _lines.store(0); return true; }
                return false;
            }
        }
        return false;
    }
private:
    std::atomic_bool _started{false};
    std::atomic_uint32_t _lines{0}, _frames{0};
};
