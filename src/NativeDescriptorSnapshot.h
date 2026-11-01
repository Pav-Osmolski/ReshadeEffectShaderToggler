#pragma once
#include <d3d12.h>
#include <memory>
#include <mutex>
#include <reshade_api.hpp>
#include <vector>
#include <wrl/client.h>

namespace Rendering {
// CPU-only pages. Descriptors are copied at creation, before a game can reuse
// its staging slot. Copying FROM a shader-visible heap is forbidden by D3D12.
class NativeDescriptorPage {
  public:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
    std::mutex mutex;
    std::vector<uint32_t> free;
    uint32_t stride = 0;
};
struct NativeDescriptorSnapshot {
    std::shared_ptr<NativeDescriptorPage> page;
    uint32_t slot = 0;
    reshade::api::resource_view view = { 0 };
    reshade::api::resource resource = { 0 };
    reshade::api::resource_view_desc description = {};
    ~NativeDescriptorSnapshot() {
        if (!page)
            return;
        std::lock_guard lock(page->mutex);
        page->free.push_back(slot);
    }
};
class NativeDescriptorSnapshots {
    std::mutex mutex;
    std::vector<std::shared_ptr<NativeDescriptorPage>> pages;

  public:
    std::shared_ptr<NativeDescriptorSnapshot> Capture(reshade::api::device* dev, reshade::api::descriptor_heap sourceHeap, reshade::api::resource_view source) {
        if (dev->get_api() != reshade::api::device_api::d3d12 || sourceHeap == 0 || source == 0)
            return {};
        auto* original = reinterpret_cast<ID3D12DescriptorHeap*>(sourceHeap.handle);
        const auto description = original->GetDesc();
        if (description.Type != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || (description.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) != 0)
            return {};
        auto* native = reinterpret_cast<ID3D12Device*>(dev->get_native());
        std::lock_guard lock(mutex);
        std::shared_ptr<NativeDescriptorPage> selected;
        uint32_t slot = 0;
        for (const auto& page : pages) {
            std::lock_guard pageLock(page->mutex);
            if (page->free.empty())
                continue;
            selected = page;
            slot = page->free.back();
            page->free.pop_back();
            break;
        }
        if (!selected) {
            if (pages.size() >= 256)
                return {}; // 65,536 live descriptor snapshots.
            selected = std::make_shared<NativeDescriptorPage>();
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            desc.NumDescriptors = 256;
            if (FAILED(native->CreateDescriptorHeap(&desc, IID_PPV_ARGS(selected->heap.GetAddressOf()))))
                return {};
            selected->stride = native->GetDescriptorHandleIncrementSize(desc.Type);
            for (uint32_t i = 1; i < 256; ++i)
                selected->free.push_back(i);
            pages.push_back(selected);
        }
        const auto handle = selected->heap->GetCPUDescriptorHandleForHeapStart().ptr + static_cast<SIZE_T>(slot) * selected->stride;
        native->CopyDescriptorsSimple(
          1, D3D12_CPU_DESCRIPTOR_HANDLE{ handle }, D3D12_CPU_DESCRIPTOR_HANDLE{ static_cast<SIZE_T>(source.handle) }, description.Type);
        auto result = std::make_shared<NativeDescriptorSnapshot>();
        result->page = selected;
        result->slot = slot;
        result->view = { static_cast<uint64_t>(handle) };
        result->resource = dev->get_resource_from_view(source);
        result->description = dev->get_resource_view_desc(source);
        return result;
    }
};
}
