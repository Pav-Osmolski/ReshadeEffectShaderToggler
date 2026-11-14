#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <algorithm>
#include <memory>
#include <vector>

namespace Rendering {
struct SceneFence {
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    uint64_t value = 0;
};

struct SceneTicket {
    std::shared_ptr<SceneFence> fence;
    uint64_t value;
};

inline void MergeSceneTickets(std::vector<SceneTicket>& into, const std::vector<SceneTicket>& added) {
    for (const auto& ticket : added) {
        const auto found = std::find_if(into.begin(), into.end(), [&](const auto& current) {
            return current.fence == ticket.fence;
        });
        if (found == into.end())
            into.push_back(ticket);
        else
            found->value = std::max(found->value, ticket.value);
    }
}

inline bool SceneTicketsComplete(const std::vector<SceneTicket>& tickets) {
    for (const auto& ticket : tickets) {
        const auto completed = ticket.fence->fence->GetCompletedValue();
        if (completed == UINT64_MAX || completed < ticket.value)
            return false;
    }
    return true;
}

// Large scratch images can pass to a new recording as soon as the previous
// recording retires. The new submission waits on its predecessor on the GPU.
// Immutable descriptor tables stay with each recording until GPU completion.
struct SceneInputOwnership {
    const void* owner = nullptr;
    const void* recording = nullptr;
    bool reusable = true;
    std::vector<SceneTicket> dependencies;

    bool CanAcquire(const void* command) const {
        return reusable && (owner == nullptr || recording == command);
    }
    void Acquire(const void* lease, const void* command) {
        owner = lease;
        recording = command;
    }
    void Retire(const void* lease, bool canReuse, const std::vector<SceneTicket>& tickets) {
        if (owner != lease)
            return; // An earlier draw in this recording shared the same images.
        MergeSceneTickets(dependencies, tickets);
        reusable = reusable && canReuse;
        owner = recording = nullptr;
    }
};

inline HRESULT WaitSceneInputTickets(ID3D12CommandQueue* queue, const std::vector<SceneTicket>& tickets) {
    for (const auto& ticket : tickets) {
        const auto completed = ticket.fence->fence->GetCompletedValue();
        if (completed == UINT64_MAX)
            return DXGI_ERROR_DEVICE_REMOVED;
        if (completed < ticket.value) {
            const auto result = queue->Wait(ticket.fence->fence.Get(), ticket.value);
            if (FAILED(result))
                return result;
        }
    }
    return S_OK;
}
}
