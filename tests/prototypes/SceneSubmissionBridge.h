#pragma once
// Standalone research only. Not included by the add-on or its project file.
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>
#include <mutex>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace SceneSubmissionResearch {
// A recording owns immutable native objects, NOT an exclusive scratch allocation.
// The caller must record the same canonical scratch state at entry and exit.
struct Recording {
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commands;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> resources;
    std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> descriptors;
    uint64_t completion = 0;
};

// Restricted proof: one known direct queue, a complete immutable batch, and all
// submissions through this object. No ReShade callback or native detour is used.
// Reject a second queue BEFORE executing anything. Global cross-queue chaining
// can create cycles with game waits and is not a generally valid extension.
class QueueAffineBridge {
    struct Flight { uint64_t ticket; std::vector<std::shared_ptr<Recording>> recordings; };
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    std::mutex mutex_;
    std::vector<Flight> flights_;
    uint64_t next_ = 0;
    bool poisoned_ = false;
    using SignalFunction = HRESULT (*)(ID3D12CommandQueue*, ID3D12Fence*, uint64_t);
    static HRESULT NativeSignal(ID3D12CommandQueue* q, ID3D12Fence* f, uint64_t v) { return q->Signal(f, v); }
    SignalFunction signal_;
public:
    QueueAffineBridge(ID3D12CommandQueue* queue, ID3D12Fence* fence, SignalFunction signal = NativeSignal)
        : queue_(queue), fence_(fence), signal_(signal) {}
    // The owner must drain on the CPU before destroying the bridge. This harness
    // has no safe automatic recovery for a post-execution Signal failure.
    HRESULT Submit(ID3D12CommandQueue* queue, const std::vector<std::shared_ptr<Recording>>& batch,
                   uint64_t& ticket) {
        std::lock_guard lock(mutex_);
        ticket = 0;
        if (poisoned_) return E_FAIL;
        if (queue != queue_.Get() || queue->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT) return E_INVALIDARG;
        const auto completed = fence_->GetCompletedValue();
        if (completed == UINT64_MAX) return DXGI_ERROR_DEVICE_REMOVED;
        // Validate the ENTIRE batch before the first native execution. Reject
        // repeated/in-flight native recordings; no partial accepted prefix.
        std::vector<ID3D12CommandList*> seen;
        for (const auto& r : batch) {
            if (!r || !r->commands || !r->allocator || r->completion > completed ||
                std::find(seen.begin(), seen.end(), r->commands.Get()) != seen.end()) return E_INVALIDARG;
            seen.push_back(r->commands.Get());
        }
        if (batch.empty()) return S_OK;
        if (next_ == UINT64_MAX - 1) return E_FAIL;
        // Allocate retention before executing, so bad_alloc cannot drop GPU refs.
        flights_.push_back({next_ + 1, batch});
        for (const auto& r : batch) {
            ID3D12CommandList* one[]{r->commands.Get()};
            // Separate calls guarantee completion of one list before the next.
            queue_->ExecuteCommandLists(1, one);
        }
        const auto value = ++next_;
        const HRESULT hr = signal_(queue_.Get(), fence_.Get(), value); // AFTER the whole batch.
        if (FAILED(hr)) { poisoned_ = true; return hr; } // Retain quarantined refs.
        for (const auto& r : batch) r->completion = value;
        ticket = value;
        return S_OK;
    }
    HRESULT Collect() {
        std::lock_guard lock(mutex_);
        if (poisoned_) return E_FAIL;
        const auto completed = fence_->GetCompletedValue();
        if (completed == UINT64_MAX) return DXGI_ERROR_DEVICE_REMOVED;
        flights_.erase(std::remove_if(flights_.begin(), flights_.end(),
            [completed](const Flight& f){return f.ticket <= completed;}), flights_.end());
        return S_OK;
    }
    size_t Pending() { std::lock_guard lock(mutex_); return flights_.size(); }
};
}
