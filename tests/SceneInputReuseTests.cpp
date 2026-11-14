#include "SceneInputSync.h"
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace Rendering;
using Microsoft::WRL::ComPtr;

static void Check(bool ok, const char* text) {
    if (!ok)
        throw std::runtime_error(text);
}
static void Hr(HRESULT result) { Check(SUCCEEDED(result), "D3D12 operation failed"); }

int main() {
    try {
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
            debug->EnableDebugLayer();
        ComPtr<IDXGIFactory4> factory;
        Hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
        ComPtr<IDXGIAdapter> warp;
        Hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));
        ComPtr<ID3D12Device> device;
        Hr(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));

        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        std::array<ComPtr<ID3D12CommandQueue>, 2> queues;
        std::array<std::shared_ptr<SceneFence>, 2> completions;
        for (size_t i = 0; i < queues.size(); ++i) {
            Hr(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queues[i])));
            completions[i] = std::make_shared<SceneFence>();
            Hr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&completions[i]->fence)));
        }
        ComPtr<ID3D12Fence> gate;
        Hr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gate)));
        Hr(queues[0]->Wait(gate.Get(), 1));
        const char shader[] =
            "StructuredBuffer<uint> input : register(t0);"
            "RWByteAddressBuffer output : register(u0);"
            "[numthreads(1,1,1)] void main() { output.Store(0,input[0]); output.Store(4,0); }";
        ComPtr<ID3DBlob> shaderBlob, errors, rootBlob;
        Hr(D3DCompile(shader, sizeof(shader), nullptr, nullptr, nullptr, "main", "cs_5_0",
                      D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &shaderBlob, &errors));
        D3D12_DESCRIPTOR_RANGE ranges[]{
            {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0},
            {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 1}};
        D3D12_ROOT_PARAMETER parameter{};
        parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameter.DescriptorTable = {2, ranges};
        D3D12_ROOT_SIGNATURE_DESC signature{1, &parameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
        Hr(D3D12SerializeRootSignature(&signature, D3D_ROOT_SIGNATURE_VERSION_1, &rootBlob, &errors));
        ComPtr<ID3D12RootSignature> root;
        Hr(device->CreateRootSignature(0, rootBlob->GetBufferPointer(), rootBlob->GetBufferSize(), IID_PPV_ARGS(&root)));
        D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc{};
        pipelineDesc.pRootSignature = root.Get();
        pipelineDesc.CS = {shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize()};
        ComPtr<ID3D12PipelineState> pipeline;
        Hr(device->CreateComputePipelineState(&pipelineDesc, IID_PPV_ARGS(&pipeline)));

        auto buffer = [&](D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES state) {
            D3D12_HEAP_PROPERTIES heap{};
            heap.Type = heapType;
            heap.CreationNodeMask = heap.VisibleNodeMask = 1;
            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = 256;
            desc.Height = desc.DepthOrArraySize = desc.MipLevels = 1;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            if (heapType == D3D12_HEAP_TYPE_DEFAULT)
                desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            ComPtr<ID3D12Resource> resource;
            Hr(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&resource)));
            return resource;
        };
        auto shared = buffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        SceneInputOwnership lifetime;
        struct Recording {
            ComPtr<ID3D12CommandAllocator> allocator;
            ComPtr<ID3D12GraphicsCommandList> commands;
            ComPtr<ID3D12Resource> input, output;
        };
        constexpr size_t count = 96; // More than the old four-image budget.
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                                           static_cast<UINT>(2 * count), D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0};
        ComPtr<ID3D12DescriptorHeap> descriptors;
        Hr(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptors)));
        const auto descriptorSize = device->GetDescriptorHandleIncrementSize(heapDesc.Type);
        std::array<Recording, count> recordings;
        size_t queuedHandoffs = 0;
        for (size_t i = 0; i < count; ++i) {
            auto& r = recordings[i];
            const auto* next = &recordings[(i + 1) % count];
            Check(lifetime.CanAcquire(&r), "Retired scratch image cannot be reused");
            if (!SceneTicketsComplete(lifetime.dependencies))
                ++queuedHandoffs;
            lifetime.Acquire(&r, &r);
            Check(!lifetime.CanAcquire(next), "Image shared by two active recordings");
            Check(lifetime.CanAcquire(&r), "Sequential draws cannot share their own recording");
            lifetime.Retire(next, true, {});
            Check(lifetime.owner == &r, "An earlier draw retired the current owner");

            Hr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&r.allocator)));
            Hr(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, r.allocator.Get(), nullptr, IID_PPV_ARGS(&r.commands)));
            r.input = buffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
            r.output = buffer(D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
            void* mapped = nullptr;
            D3D12_RANGE unread{};
            Hr(r.input->Map(0, &unread, &mapped));
            *static_cast<UINT64*>(mapped) = i + 1;
            r.input->Unmap(0, nullptr);
            const auto cpu = descriptors->GetCPUDescriptorHandleForHeapStart().ptr + i * 2 * descriptorSize;
            D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
            srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv.Buffer.NumElements = 64;
            srv.Buffer.StructureByteStride = 4;
            device->CreateShaderResourceView(r.input.Get(), &srv, {cpu});
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
            uav.Format = DXGI_FORMAT_R32_TYPELESS;
            uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uav.Buffer.NumElements = 64;
            uav.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
            device->CreateUnorderedAccessView(shared.Get(), nullptr, &uav, {cpu + descriptorSize});
            ID3D12DescriptorHeap* heaps[]{descriptors.Get()};
            r.commands->SetDescriptorHeaps(1, heaps);
            r.commands->SetComputeRootSignature(root.Get());
            r.commands->SetComputeRootDescriptorTable(0, {descriptors->GetGPUDescriptorHandleForHeapStart().ptr + i * 2 * descriptorSize});
            r.commands->SetPipelineState(pipeline.Get());
            r.commands->Dispatch(1, 1, 1);
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition = {shared.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                                  D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE};
            r.commands->ResourceBarrier(1, &barrier);
            r.commands->CopyBufferRegion(r.output.Get(), 0, shared.Get(), 0, sizeof(UINT64));
            std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
            r.commands->ResourceBarrier(1, &barrier);
            Hr(r.commands->Close());

            const auto queue = i % queues.size();
            Hr(WaitSceneInputTickets(queues[queue].Get(), lifetime.dependencies));
            ID3D12CommandList* lists[]{r.commands.Get()};
            queues[queue]->ExecuteCommandLists(1, lists);
            // Model command-list retirement after submission. Each recording
            // keeps its own descriptor table, resources and allocator intact.
            auto& fence = completions[queue];
            const auto value = ++fence->value;
            Hr(queues[queue]->Signal(fence->fence.Get(), value));
            lifetime.Retire(&r, true, {{fence, value}});
            Check(lifetime.dependencies.size() <= queues.size(), "Dependency count grew per frame");
        }
        Check(queuedHandoffs == count - 1, "Backlog test did not hold the GPU");
        Hr(gate->Signal(1));
        for (const auto& fence : completions)
            Hr(fence->fence->SetEventOnCompletion(fence->value, nullptr));
        for (size_t i = 0; i < count; ++i) {
            void* mapped = nullptr;
            Hr(recordings[i].output->Map(0, nullptr, &mapped));
            Check(*static_cast<const UINT64*>(mapped) == i + 1, "Scratch reuse changed an earlier recording's output");
            D3D12_RANGE unmodified{};
            recordings[i].output->Unmap(0, &unmodified);
        }
        Check(SceneTicketsComplete(lifetime.dependencies), "Completion tickets did not finish");
        const auto saved = lifetime.dependencies;
        lifetime.Acquire(&recordings[0], &recordings[0]);
        lifetime.Retire(&recordings[0], true, {}); // Recording reset without submission.
        Check(lifetime.dependencies.size() == saved.size(), "Unsubmitted recording lost inherited dependencies");
        lifetime.Acquire(&recordings[0], &recordings[0]);
        lifetime.Retire(&recordings[0], false, {});
        Check(!lifetime.CanAcquire(&recordings[1]), "Failed retirement was reused");

        ComPtr<ID3D12InfoQueue> messages;
        if (SUCCEEDED(device.As(&messages))) {
            for (UINT64 i = 0; i < messages->GetNumStoredMessages(); ++i) {
                SIZE_T size = 0;
                Hr(messages->GetMessage(i, nullptr, &size));
                std::vector<char> bytes(size);
                auto* message = reinterpret_cast<D3D12_MESSAGE*>(bytes.data());
                Hr(messages->GetMessage(i, message, &size));
                Check(message->Severity > D3D12_MESSAGE_SEVERITY_ERROR, message->pDescription);
            }
        }
        std::puts("96 delayed GPU recordings reused one scratch allocation across two queues with immutable descriptors and exact outputs.");
        std::puts("Active, sequential, unsubmitted and failed-retirement ownership checks passed.");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
