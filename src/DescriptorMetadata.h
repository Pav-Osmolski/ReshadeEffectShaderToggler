#pragma once
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Rendering {
// Sparse indices avoid allocating a huge vector for a high heap offset. Unknown
// copies invalidate the destination; they must never preserve an old valid view.
template <typename Descriptor, size_t Capacity = 65536>
class DescriptorMetadata {
    mutable std::mutex mutex;
    std::unordered_map<uint32_t, Descriptor> entries;
public:
    static bool ValidSpan(uint32_t offset, uint32_t count) {
        return static_cast<uint64_t>(offset) + count <= static_cast<uint64_t>(UINT32_MAX) + 1;
    }
    Descriptor ReadOne(uint32_t offset) const {
        std::lock_guard lock(mutex);
        const auto it = entries.find(offset);
        return it != entries.end() ? it->second : Descriptor{};
    }
    std::vector<Descriptor> Snapshot(uint32_t offset, uint32_t count) const {
        const size_t size = std::min(static_cast<size_t>(count), Capacity);
        std::vector<Descriptor> result(size);
        if (!ValidSpan(offset, count)) return result;
        std::lock_guard lock(mutex);
        for (size_t i = 0; i < size; ++i) {
            const auto it = entries.find(static_cast<uint32_t>(static_cast<uint64_t>(offset) + i));
            if (it != entries.end()) result[i] = it->second;
        }
        return result;
    }
    template <typename Reader>
    bool Write(uint32_t offset, uint32_t count, Reader read) {
        if (!ValidSpan(offset, count)) return false;
        std::lock_guard lock(mutex);
        const uint64_t end = static_cast<uint64_t>(offset) + count;
        // Invalidate the whole destination, including a suffix outside the work
        // budget, so skipped/unobserved input cannot leave stale metadata behind.
        if (count <= Capacity) {
            for (uint32_t i = 0; i < count; ++i) entries.erase(static_cast<uint32_t>(static_cast<uint64_t>(offset) + i));
        } else {
            for (auto it = entries.begin(); it != entries.end();) {
                if (it->first >= offset && it->first < end) it = entries.erase(it);
                else ++it;
            }
        }
        const size_t size = std::min(static_cast<size_t>(count), Capacity);
        bool complete = size == count;
        for (size_t i = 0; i < size; ++i) {
            const auto value = read(static_cast<uint32_t>(i));
            if (static_cast<uint32_t>(value.type) == UINT32_MAX) { complete = false; continue; }
            if (entries.size() == Capacity) { complete = false; continue; }
            entries.emplace(static_cast<uint32_t>(static_cast<uint64_t>(offset) + i), value);
        }
        return complete;
    }
    size_t Size() const { std::lock_guard lock(mutex); return entries.size(); }
};
}
