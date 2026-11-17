#include "prototypes/SceneSubmissionBridge.h"
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <array>
#include <thread>
#include <atomic>
#include <cstdio>
#include <stdexcept>
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
using Microsoft::WRL::ComPtr;
using namespace SceneSubmissionResearch;
static void Check(bool v,const char* s){if(!v)throw std::runtime_error(s);}
static void Hr(HRESULT h){Check(SUCCEEDED(h),"Native prototype operation failed");}
static void WaitFence(ID3D12Fence* fence,uint64_t value){
    HANDLE event=CreateEvent(nullptr,FALSE,FALSE,nullptr);Check(event!=nullptr,"Fence event creation failed");
    const HRESULT hr=fence->SetEventOnCompletion(value,event);
    const DWORD result=SUCCEEDED(hr)?WaitForSingleObject(event,15000):WAIT_FAILED;
    CloseHandle(event);Hr(hr);Check(result==WAIT_OBJECT_0,"GPU completion timed out");
}
int main(){try{
    ComPtr<ID3D12Debug> debug;
    Hr(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))); debug->EnableDebugLayer();
    ComPtr<IDXGIFactory4> factory; Hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
    ComPtr<IDXGIAdapter> warp; Hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));
    ComPtr<ID3D12Device> device; Hr(D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
    ComPtr<ID3D12InfoQueue> info; Hr(device.As(&info));
    D3D12_COMMAND_QUEUE_DESC qdesc{};
    ComPtr<ID3D12CommandQueue> queue,other;
    Hr(device->CreateCommandQueue(&qdesc,IID_PPV_ARGS(&queue)));
    Hr(device->CreateCommandQueue(&qdesc,IID_PPV_ARGS(&other)));
    ComPtr<ID3D12Fence> done,gate;
    Hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&done)));
    Hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&gate)));
    QueueAffineBridge bridge(queue.Get(),done.Get());
    auto wait=[&](uint64_t t){Check(t!=0,"Missing completion ticket"); WaitFence(done.Get(),t);};
    auto buffer=[&](D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state){
        D3D12_HEAP_PROPERTIES hp{};hp.Type=type;hp.CreationNodeMask=hp.VisibleNodeMask=1;
        D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=256;
        d.Height=d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ComPtr<ID3D12Resource> r;Hr(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,state,nullptr,IID_PPV_ARGS(&r)));return r;
    };
    D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;hp.CreationNodeMask=hp.VisibleNodeMask=1;
    D3D12_RESOURCE_DESC td{};td.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;td.Width=td.Height=1;
    td.DepthOrArraySize=td.MipLevels=1;td.SampleDesc.Count=1;td.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
    td.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    ComPtr<ID3D12Resource> scratch;
    Hr(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&td,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&scratch)));
    const auto scratchBytes=device->GetResourceAllocationInfo(0,1,&td).SizeInBytes;
    const char shader[]="StructuredBuffer<float4> input:register(t0); RWTexture2D<float4> output:register(u0);"
        "[numthreads(1,1,1)] void main(){output[uint2(0,0)]=input[0]+float4(0.5,0,0,0);}";
    ComPtr<ID3DBlob> code,errors,serialized;
    Hr(D3DCompile(shader,sizeof(shader),nullptr,nullptr,nullptr,"main","cs_5_0",0,0,&code,&errors));
    D3D12_DESCRIPTOR_RANGE ranges[]={{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,1}};
    D3D12_ROOT_PARAMETER param{};param.ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;param.DescriptorTable={2,ranges};
    D3D12_ROOT_SIGNATURE_DESC rs{1,&param,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
    Hr(D3D12SerializeRootSignature(&rs,D3D_ROOT_SIGNATURE_VERSION_1,&serialized,&errors));
    ComPtr<ID3D12RootSignature> root;Hr(device->CreateRootSignature(0,serialized->GetBufferPointer(),serialized->GetBufferSize(),IID_PPV_ARGS(&root)));
    D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.CS={code->GetBufferPointer(),code->GetBufferSize()};
    ComPtr<ID3D12PipelineState> pipeline;Hr(device->CreateComputePipelineState(&pd,IID_PPV_ARGS(&pipeline)));
    struct Item{std::shared_ptr<Recording> recording;ComPtr<ID3D12Resource> readback;size_t index;};
    auto record=[&](size_t index){
        Item item{std::make_shared<Recording>(),{},index};auto& r=*item.recording;
        auto input=buffer(D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        item.readback=buffer(D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        void* data=nullptr;D3D12_RANGE empty{};Hr(input->Map(0,&empty,&data));
        const float value[]{static_cast<float>(index%32),2.f,-.5f,.25f};memcpy(data,value,sizeof(value));input->Unmap(0,nullptr);
        D3D12_DESCRIPTOR_HEAP_DESC hd{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,2,D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,0};
        ComPtr<ID3D12DescriptorHeap> heap;Hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)));
        auto cpu=heap->GetCPUDescriptorHandleForHeapStart();const auto stride=device->GetDescriptorHandleIncrementSize(hd.Type);
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Buffer.NumElements=1;srv.Buffer.StructureByteStride=16;device->CreateShaderResourceView(input.Get(),&srv,cpu);
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;uav.Format=td.Format;
        cpu.ptr+=stride;device->CreateUnorderedAccessView(scratch.Get(),nullptr,&uav,cpu);
        r.resources={input,item.readback,scratch};r.descriptors={heap};r.root=root;r.pipeline=pipeline;
        Hr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&r.allocator)));
        Hr(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,r.allocator.Get(),pipeline.Get(),IID_PPV_ARGS(&r.commands)));
        ID3D12DescriptorHeap* heaps[]{heap.Get()};r.commands->SetDescriptorHeaps(1,heaps);r.commands->SetComputeRootSignature(root.Get());
        r.commands->SetComputeRootDescriptorTable(0,heap->GetGPUDescriptorHandleForHeapStart());r.commands->Dispatch(1,1,1);
        D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition={scratch.Get(),0,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE};r.commands->ResourceBarrier(1,&b);
        D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=scratch.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=item.readback.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dst.PlacedFootprint.Footprint={td.Format,1,1,1,256};r.commands->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
        std::swap(b.Transition.StateBefore,b.Transition.StateAfter);r.commands->ResourceBarrier(1,&b);
        Hr(r.commands->Close());return item;
    };
    auto verify=[&](const Item& item){void* p=nullptr;Hr(item.readback->Map(0,nullptr,&p));const auto* bits=static_cast<const uint16_t*>(p);
        const float expected=static_cast<float>(item.index%32)+.5f;uint32_t raw=0;memcpy(&raw,&expected,sizeof(raw));
        const uint16_t red=static_cast<uint16_t>((((raw>>23)-112)<<10)|((raw>>13)&1023));
        Check(bits[0]==red&&bits[1]==0x4000&&bits[2]==0xb800&&bits[3]==0x3400,"FP16 HDR/alpha output mismatch");
        D3D12_RANGE empty{};item.readback->Unmap(0,&empty);};
    std::vector<Item> items;for(size_t i=0;i<32;++i)items.push_back(record(i));
    std::vector<std::shared_ptr<Recording>> batch;for(size_t i=items.size();i-->0;)batch.push_back(items[i].recording);
    uint64_t ticket=0;Hr(queue->Wait(gate.Get(),1));Hr(bridge.Submit(queue.Get(),batch,ticket));
    Check(done->GetCompletedValue()<ticket,"Completion ticket preceded gated batch");
    uint64_t rejected=99;Check(FAILED(bridge.Submit(queue.Get(),{items[0].recording},rejected))&&rejected==0,"In-flight recording accepted");
    Hr(gate->Signal(1));wait(ticket);for(const auto& i:items)verify(i);Hr(bridge.Collect());
    Check(bridge.Pending()==0,"Completed flight retained");
    for(size_t round=0;round<4;++round){
        std::rotate(batch.begin(),batch.begin()+7,batch.end());Hr(bridge.Submit(queue.Get(),batch,ticket));wait(ticket);
        for(const auto& i:items)verify(i);Hr(bridge.Collect());
    }
    std::puts("PASS: 32 simultaneously valid recordings, one FP16 scratch image, reordered batches and four resubmissions; exact HDR/alpha outputs.");
    Check(scratchBytes==device->GetResourceAllocationInfo(0,1,&td).SizeInBytes,"Scratch allocation changed");
    // Entire malformed batch is rejected, even when its first element is valid.
    const auto before=items[0].recording->completion;
    Check(FAILED(bridge.Submit(queue.Get(),{items[0].recording,nullptr},rejected)),"Partial invalid batch accepted");
    Check(items[0].recording->completion==before&&bridge.Pending()==0,"Invalid batch executed a prefix");
    Check(FAILED(bridge.Submit(queue.Get(),{items[0].recording,items[0].recording},rejected)),"Repeated native list accepted");
    Check(FAILED(bridge.Submit(other.Get(),batch,rejected))&&bridge.Pending()==0,"Second queue reached native execution");
    std::puts("PASS: second queue, malformed batches, repeated and in-flight recordings rejected before execution.");
    // Two CPU submitters share the same queue/bridge. Each batch retains its lock
    // through native execution and the post-submit completion signal.
    Hr(queue->Wait(gate.Get(),2));std::atomic<bool> success{true};std::array<uint64_t,2> tickets{};
    auto submit=[&](size_t n){std::vector<std::shared_ptr<Recording>> part;
        for(size_t i=n*16;i<(n+1)*16;++i)part.push_back(items[i].recording);
        if(FAILED(bridge.Submit(queue.Get(),part,tickets[n])))success=false;};
    std::thread a(submit,0),b(submit,1);a.join();b.join();Check(success,"Concurrent submission failed");
    Check(tickets[0]!=tickets[1]&&done->GetCompletedValue()<std::min(tickets[0],tickets[1]),"Early or duplicate concurrent ticket");
    // Retire/reset the CPU recording identity while GPU work is pending. A new
    // Recording has a fresh allocator; the old immutable version stays retained.
    std::weak_ptr<Recording> old=items[0].recording;
    batch.clear();items[0]=record(100);Check(!old.expired(),"Pending version destroyed at reset");
    Hr(bridge.Collect());Check(!old.expired(),"Pending version collected early");
    Hr(gate->Signal(2));wait(std::max(tickets[0],tickets[1]));Hr(bridge.Collect());Check(old.expired(),"Completed retired version leaked");
    for(size_t i=1;i<items.size();++i)verify(items[i]);
    Hr(bridge.Submit(queue.Get(),{items[0].recording},ticket));wait(ticket);verify(items[0]);Hr(bridge.Collect());
    auto abandoned=record(200);std::weak_ptr<Recording> unsubmitted=abandoned.recording;abandoned.recording.reset();
    Check(unsubmitted.expired()&&bridge.Pending()==0,"Abandoned recording leaked or submitted");
    std::puts("PASS: concurrent CPU batches, pending-version reset/retention, completed retirement and abandoned recording.");
    // Fault injection AFTER real native execution: a failed completion signal
    // poisons the bridge and quarantines every flight. No fabricated ticket.
    ComPtr<ID3D12Fence> failedFence,drain;
    Hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&failedFence)));
    Hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&drain)));
    auto failed=record(301);std::weak_ptr<Recording> quarantined=failed.recording;
    {
        QueueAffineBridge failing(other.Get(),failedFence.Get(),
            [](ID3D12CommandQueue*,ID3D12Fence*,uint64_t)->HRESULT{return E_FAIL;});
        Hr(other->Wait(gate.Get(),3));uint64_t noTicket=7;
        Check(FAILED(failing.Submit(other.Get(),{failed.recording},noTicket))&&noTicket==0,"Signal failure fabricated completion");
        failed.recording.reset();Check(!quarantined.expired(),"Signal failure released in-flight refs");
        Check(FAILED(failing.Collect())&&failing.Pending()==1,"Poisoned flight collected");
        Check(FAILED(failing.Submit(other.Get(),{},noTicket)),"Poisoned bridge accepted work");
        Hr(other->Signal(drain.Get(),1));Check(drain->GetCompletedValue()==0,"Fault test gate did not hold execution");
        Hr(gate->Signal(3));WaitFence(drain.Get(),1);verify(failed);
        // External drain is test-owned and occurs before the poisoned bridge
        // destroys its quarantine. The bridge itself claims no recovery.
    }
    Check(quarantined.expired(),"Drained quarantine leaked");
    std::puts("PASS: injected post-execution signal failure retains GPU refs, issues no ticket and rejects further work; external drain permits teardown.");
    for(UINT64 i=0;i<info->GetNumStoredMessages();++i){SIZE_T bytes=0;Hr(info->GetMessage(i,nullptr,&bytes));
        std::vector<char> storage(bytes);auto* message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());Hr(info->GetMessage(i,message,&bytes));
        Check(message->Severity>D3D12_MESSAGE_SEVERITY_ERROR,message->pDescription);}
    std::printf("Scratch physical bytes: %llu; recording count: 32; debug error/corruption messages: 0.\n",static_cast<unsigned long long>(scratchBytes));
    return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
