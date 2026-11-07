#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
using Path = std::filesystem::path;
using Bytes = std::vector<char>;

static void Check(HRESULT hr, const char *operation) {
  if (FAILED(hr)) {
    char message[160];
    snprintf(message, sizeof(message), "%s failed: 0x%08lX", operation, hr);
    throw std::runtime_error(message);
  }
}

static Bytes Read(const Path &path) {
  std::ifstream file(path, std::ios::binary);
  Bytes data((std::istreambuf_iterator<char>(file)), {});
  if (data.empty())
    throw std::runtime_error("Could not read " + path.string());
  return data;
}

struct TextureInput {
  DXGI_FORMAT storage, view;
  UINT width, height, depth, mips, bytesPerPixel;
};

static TextureInput TextureFormat(const std::string &name, UINT w, UINT h, UINT d, UINT mips) {
  const struct { const char *name; DXGI_FORMAT storage, view; UINT size; } formats[] = {
      {"R11G11B10_FLOAT", DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT, 4},
      {"R16G16B16A16_FLOAT", DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, 8},
      {"R10G10B10A2_UNORM", DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM, 4},
      {"R8G8B8A8_UNORM", DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, 4},
      {"D24S8", DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, 4},
  };
  for (const auto &format : formats)
    if (name == format.name)
      return {format.storage, format.view, w, h, d, mips, format.size};
  throw std::runtime_error("Unsupported capture format " + name);
}

class Gpu {
public:
  ComPtr<ID3D12Device> device;
  ComPtr<ID3D12GraphicsCommandList> list;
  std::vector<ComPtr<ID3D12Resource>> resources;
  Gpu() {
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
      debug->EnableDebugLayer();
      printf("D3D12 debug layer enabled\n");
    }
    Check(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)), "Create device");
    D3D12_COMMAND_QUEUE_DESC desc{};
    Check(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue)), "Create queue");
    Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)), "Create allocator");
    Check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&list)), "Create list");
    Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "Create fence");
  }

  ID3D12Resource *Resource(D3D12_RESOURCE_DESC desc, D3D12_HEAP_TYPE type, D3D12_RESOURCE_STATES state) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = type;
    heap.CreationNodeMask = heap.VisibleNodeMask = 1;
    ComPtr<ID3D12Resource> resource;
    Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&resource)), "Create resource");
    resources.push_back(resource);
    return resource.Get();
  }

  ID3D12Resource *Buffer(UINT64 size, D3D12_HEAP_TYPE type, const char *data = nullptr) {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = desc.SampleDesc.Count = 1;
    desc.DepthOrArraySize = desc.MipLevels = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    auto *resource = Resource(desc, type, type == D3D12_HEAP_TYPE_UPLOAD
                                           ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST);
    if (data) {
      void *mapped = nullptr;
      const D3D12_RANGE unread{};
      Check(resource->Map(0, &unread, &mapped), "Map upload");
      memcpy(mapped, data, static_cast<size_t>(size));
      resource->Unmap(0, nullptr);
    }
    return resource;
  }

  ID3D12Resource *Texture(const TextureInput &input, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state) {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = input.depth > 1 ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = input.width;
    desc.Height = input.height;
    desc.DepthOrArraySize = static_cast<UINT16>(input.depth);
    desc.MipLevels = static_cast<UINT16>(input.mips);
    desc.Format = input.storage;
    desc.SampleDesc.Count = 1;
    desc.Flags = flags;
    return Resource(desc, D3D12_HEAP_TYPE_DEFAULT, state);
  }

  void Transition(ID3D12Resource *resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition = {resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after};
    list->ResourceBarrier(1, &barrier);
  }

  ID3D12Resource *LoadTexture(const Path &directory, UINT binding, const TextureInput &input) {
    auto *texture = Texture(input, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);
    auto desc = texture->GetDesc();
    for (UINT mip = 0; mip < input.mips; ++mip) {
      auto bytes = Read(directory / ("t" + std::to_string(binding) + "-" + std::to_string(mip) + ".raw"));
      D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
      UINT rows = 0;
      UINT64 rowSize = 0, totalSize = 0;
      device->GetCopyableFootprints(&desc, mip, 1, 0, &footprint, &rows, &rowSize, &totalSize);
      const auto &layout = footprint.Footprint;
      const UINT expectedRow = std::max(1u, input.width >> mip) * input.bytesPerPixel;
      if (rowSize != expectedRow || bytes.size() != rowSize * rows * layout.Depth)
        throw std::runtime_error("Capture texture size mismatch at t" + std::to_string(binding));
      auto *upload = Buffer(totalSize, D3D12_HEAP_TYPE_UPLOAD);
      void *mapped = nullptr;
      const D3D12_RANGE unread{};
      Check(upload->Map(0, &unread, &mapped), "Map texture upload");
      for (UINT row = 0; row < rows * layout.Depth; ++row)
        memcpy(static_cast<char *>(mapped) + size_t(row) * layout.RowPitch,
               bytes.data() + size_t(row) * expectedRow, expectedRow);
      upload->Unmap(0, nullptr);
      D3D12_TEXTURE_COPY_LOCATION source{upload, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
      source.PlacedFootprint = footprint;
      D3D12_TEXTURE_COPY_LOCATION dest{texture, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
      dest.SubresourceIndex = mip;
      list->CopyTextureRegion(&dest, 0, 0, 0, &source, nullptr);
    }
    Transition(texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    return texture;
  }

  struct Readback {
    Path path;
    ID3D12Resource *buffer;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    UINT rowSize;
  };

  void SaveTarget(ID3D12Resource *target, const Path &path) {
    const auto desc = target->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT64 totalSize = 0, rowSize = 0;
    device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, &rowSize, &totalSize);
    auto *buffer = Buffer(totalSize, D3D12_HEAP_TYPE_READBACK);
    Transition(target, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION source{target, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
    D3D12_TEXTURE_COPY_LOCATION dest{buffer, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
    dest.PlacedFootprint = footprint;
    list->CopyTextureRegion(&dest, 0, 0, 0, &source, nullptr);
    Transition(target, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    outputs.push_back({path, buffer, footprint, static_cast<UINT>(rowSize)});
  }

  void Finish() {
    Check(list->Close(), "Close list");
    ID3D12CommandList *commands[]{list.Get()};
    queue->ExecuteCommandLists(1, commands);
    Check(queue->Signal(fence.Get(), 1), "Signal fence");
    Check(fence->SetEventOnCompletion(1, nullptr), "Wait for GPU");
    Check(device->GetDeviceRemovedReason(), "GPU submission");
    ComPtr<ID3D12InfoQueue> messages;
    if (SUCCEEDED(device.As(&messages))) {
      bool hasErrors = false;
      for (UINT64 i = 0; i < messages->GetNumStoredMessages(); ++i) {
        SIZE_T size = 0;
        Check(messages->GetMessage(i, nullptr, &size), "Read debug message size");
        Bytes storage(size);
        auto *message = reinterpret_cast<D3D12_MESSAGE *>(storage.data());
        Check(messages->GetMessage(i, message, &size), "Read debug message");
        if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) {
          fprintf(stderr, "%s\n", message->pDescription);
          hasErrors = true;
        }
      }
      if (hasErrors)
        throw std::runtime_error("D3D12 validation errors");
      printf("D3D12 debug queue contains no errors\n");
    }
    for (const auto &output : outputs) {
      void *mapped = nullptr;
      Check(output.buffer->Map(0, nullptr, &mapped), "Map readback");
      std::ofstream file(output.path, std::ios::binary);
      const auto &layout = output.footprint.Footprint;
      for (UINT row = 0; row < layout.Height; ++row)
        file.write(static_cast<char *>(mapped) + size_t(row) * layout.RowPitch, output.rowSize);
      const D3D12_RANGE unmodified{};
      output.buffer->Unmap(0, &unmodified);
      if (!file)
        throw std::runtime_error("Could not write " + output.path.string());
    }
  }

private:
  ComPtr<ID3D12CommandQueue> queue;
  ComPtr<ID3D12CommandAllocator> allocator;
  ComPtr<ID3D12Fence> fence;
  std::vector<Readback> outputs;
};

static ComPtr<ID3D12DescriptorHeap> Heap(Gpu &gpu, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT count,
                                       D3D12_DESCRIPTOR_HEAP_FLAGS flags) {
  D3D12_DESCRIPTOR_HEAP_DESC desc{type, count, flags, 0};
  ComPtr<ID3D12DescriptorHeap> heap;
  Check(gpu.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)), "Create descriptor heap");
  return heap;
}

static ComPtr<ID3D12RootSignature> Root(Gpu &gpu) {
  D3D12_DESCRIPTOR_RANGE ranges[]{
      {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 10, 0, 0, 0},
      {D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 10, 0, 0, 0}};
  D3D12_ROOT_PARAMETER params[3]{};
  for (UINT i = 0; i < 2; ++i) {
    params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[i].DescriptorTable = {1, &ranges[i]};
    params[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  }
  params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  D3D12_ROOT_SIGNATURE_DESC desc{3, params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
  ComPtr<ID3DBlob> blob, error;
  Check(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), "Serialize root signature");
  ComPtr<ID3D12RootSignature> root;
  Check(gpu.device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root)), "Create root signature");
  return root;
}

static ComPtr<ID3D12PipelineState> Pipeline(Gpu &gpu, ID3D12RootSignature *root,
                                           const Bytes &vs, const Path &ps, DXGI_FORMAT format, bool splitAlpha = false) {
  const auto pixel = Read(ps);
  const D3D12_INPUT_ELEMENT_DESC input[]{
      {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = root;
  desc.VS = {vs.data(), vs.size()};
  desc.PS = {pixel.data(), pixel.size()};
  desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  desc.SampleMask = UINT_MAX;
  desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  desc.RasterizerState.DepthClipEnable = TRUE;
  desc.InputLayout = {input, static_cast<UINT>(std::size(input))};
  desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  desc.NumRenderTargets = 1;
  desc.RTVFormats[0] = format;
  if (splitAlpha) {
    desc.NumRenderTargets = 2;
    desc.RTVFormats[1] = DXGI_FORMAT_R32_FLOAT;
    desc.BlendState.IndependentBlendEnable = TRUE;
    desc.BlendState.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED;
  }
  desc.SampleDesc.Count = 1;
  ComPtr<ID3D12PipelineState> pipeline;
  Check(gpu.device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline)), ps.filename().string().c_str());
  return pipeline;
}

int wmain(int argc, wchar_t **argv) {
  if (argc != 4 && argc != 5) {
    fprintf(stderr, "Usage: split-test mode-directory replay-vertex.dxil tint-scene.dxil [overwrite-alpha.dxil]\n");
    return 2;
  }
  try {
    const Path folder = argv[1];
    Gpu gpu;
    const auto root = Root(gpu);
    const auto vertexShader = Read(argv[2]);
    const auto vertices = Read(folder / "vertices.raw");
    auto constants = Read(folder / "constants.raw");
    if (vertices.size() != 144 || constants.size() < 260)
      throw std::runtime_error("Unexpected captured vertex or constant buffer");
    constants.resize(512, 0);
    auto *vertexBuffer = gpu.Buffer(vertices.size(), D3D12_HEAP_TYPE_UPLOAD, vertices.data());
    auto *constantBuffer = gpu.Buffer(constants.size(), D3D12_HEAP_TYPE_UPLOAD, constants.data());
    const auto srvs = Heap(gpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 90, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
    const auto samplers = Heap(gpu, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 10, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
    const auto rtvs = Heap(gpu, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 8, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    const UINT srvStep = gpu.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const UINT samplerStep = gpu.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    const UINT rtvStep = gpu.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_VIEWPORT viewport{};
    D3D12_RECT scissor{};
    std::array<ID3D12Resource *, 10> textures{};
    std::array<TextureInput, 10> inputs{};
    std::array<bool, 10> hasSampler{};
    std::ifstream manifest(folder / "split-inputs.tsv");
    std::string line;
    while (std::getline(manifest, line)) {
      std::istringstream values(line);
      std::string kind;
      values >> kind;
      if (kind == "viewport") {
        values >> viewport.TopLeftX >> viewport.TopLeftY >> viewport.Width >> viewport.Height >> viewport.MinDepth >> viewport.MaxDepth;
      } else if (kind == "scissor") {
        values >> scissor.left >> scissor.top >> scissor.right >> scissor.bottom;
      } else if (kind == "mesh") {
        UINT count, stride, topology;
        values >> count >> stride >> topology;
        if (!values || count != 3 || stride != 48 || topology != 5)
          throw std::runtime_error("Unexpected compositor geometry");
      } else if (kind == "texture") {
        UINT slot, width, height, depth, mips, r, g, b, a;
        std::string format;
        values >> slot >> format >> width >> height >> depth >> mips >> r >> g >> b >> a;
        if (!values || slot >= textures.size() || r != 0 || g != 1 || b != 2 || a != 3)
          throw std::runtime_error("Unsupported captured texture view");
        inputs[slot] = TextureFormat(format, width, height, depth, mips);
        textures[slot] = gpu.LoadTexture(folder, slot, inputs[slot]);
      } else if (kind == "sampler") {
        UINT slot, u, v, w, min, mag, mip, filter;
        float anisotropy;
        D3D12_SAMPLER_DESC desc{};
        values >> slot >> u >> v >> w >> min >> mag >> mip >> filter >> anisotropy
               >> desc.MinLOD >> desc.MaxLOD >> desc.MipLODBias;
        for (float &border : desc.BorderColor)
          values >> border;
        if (!values || slot >= hasSampler.size() || u != 3 || v != 3 || w != 3 || min < 1 || min > 2 ||
            mag < 1 || mag > 2 || mip < 1 || mip > 2 || filter != 0 || anisotropy != 0)
          throw std::runtime_error("Unsupported captured sampler");
        desc.AddressU = desc.AddressV = desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.Filter = static_cast<D3D12_FILTER>(((min - 1) << 4) | ((mag - 1) << 2) | (mip - 1));
        desc.MaxAnisotropy = 1;
        desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        gpu.device->CreateSampler(&desc, {samplers->GetCPUDescriptorHandleForHeapStart().ptr + slot * samplerStep});
        hasSampler[slot] = true;
      }
      if (!values)
        throw std::runtime_error("Incomplete capture manifest");
    }
    for (UINT slot : {0u, 1u, 2u, 3u, 4u, 8u, 9u})
      if (!textures[slot] || !hasSampler[slot])
        throw std::runtime_error("Missing captured binding");
    const UINT width = static_cast<UINT>(viewport.Width), height = static_cast<UINT>(viewport.Height);
    if (width != inputs[8].width || height != inputs[8].height || viewport.TopLeftX || viewport.TopLeftY)
      throw std::runtime_error("Unexpected output viewport");

    auto table = [&](UINT index, ID3D12Resource *scene, TextureInput input, ID3D12Resource *alpha = nullptr) {
      for (UINT slot = 0; slot < textures.size(); ++slot) {
        ID3D12Resource *resource = slot == 0 ? scene : textures[slot];
        TextureInput info = slot == 0 ? input : inputs[slot];
        if (slot == 1 && alpha) {
          resource = alpha;
          info = {DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_FLOAT, width, height, 1, 1, 4};
        }
        D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Format = resource ? info.view : DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        if (info.depth > 1) {
          desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
          desc.Texture3D.MipLevels = info.mips;
        } else {
          desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
          desc.Texture2D.MipLevels = resource ? info.mips : 1;
        }
        gpu.device->CreateShaderResourceView(resource, &desc, {srvs->GetCPUDescriptorHandleForHeapStart().ptr + (index * 10 + slot) * srvStep});
      }
    };
    table(0, textures[0], inputs[0]);
    auto createTarget = [&](DXGI_FORMAT format, UINT index) {
      TextureInput input{format, format, width, height, 1, 1, 0};
      auto *target = gpu.Texture(input, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET);
      gpu.device->CreateRenderTargetView(target, nullptr, {rtvs->GetCPUDescriptorHandleForHeapStart().ptr + index * rtvStep});
      return target;
    };
    const auto outputFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
    auto *output = createTarget(outputFormat, 0);
    const auto original = Pipeline(gpu, root.Get(), vertexShader, folder / "original.shader", outputFormat);
    const auto ui = Pipeline(gpu, root.Get(), vertexShader, folder / "split-ui.dxil", outputFormat);
    std::vector<ComPtr<ID3D12PipelineState>> passes;
    ID3D12DescriptorHeap *heaps[]{srvs.Get(), samplers.Get()};
    gpu.list->SetDescriptorHeaps(2, heaps);
    gpu.list->SetGraphicsRootSignature(root.Get());
    gpu.list->SetGraphicsRootDescriptorTable(1, samplers->GetGPUDescriptorHandleForHeapStart());
    gpu.list->SetGraphicsRootConstantBufferView(2, constantBuffer->GetGPUVirtualAddress());
    gpu.list->RSSetViewports(1, &viewport);
    gpu.list->RSSetScissorRects(1, &scissor);
    const D3D12_VERTEX_BUFFER_VIEW vertexView{vertexBuffer->GetGPUVirtualAddress(), 144, 48};
    gpu.list->IASetVertexBuffers(0, 1, &vertexView);
    gpu.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    auto draw = [&](ID3D12PipelineState *pipeline, UINT target, UINT descriptors, UINT alpha = 0) {
      const auto base = rtvs->GetCPUDescriptorHandleForHeapStart().ptr;
      const D3D12_CPU_DESCRIPTOR_HANDLE targets[]{{base + target * rtvStep}, {base + alpha * rtvStep}};
      gpu.list->OMSetRenderTargets(alpha ? 2 : 1, targets, FALSE, nullptr);
      gpu.list->SetGraphicsRootDescriptorTable(0, {srvs->GetGPUDescriptorHandleForHeapStart().ptr + descriptors * 10 * srvStep});
      gpu.list->SetPipelineState(pipeline);
      gpu.list->DrawInstanced(3, 1, 0, 0);
    };
    draw(original.Get(), 0, 0);
    gpu.SaveTarget(output, folder / "native-original.raw");
    const std::pair<DXGI_FORMAT, const char *> formats[]{
        {DXGI_FORMAT_R32G32B32A32_FLOAT, "fp32"}, {DXGI_FORMAT_R16G16B16A16_FLOAT, "fp16"}, {outputFormat, "rgb10"}};
    UINT slot = 1;
    for (const auto &[format, name] : formats) {
      auto *scene = createTarget(format, slot);
      auto *processed = createTarget(format, slot + 1);
      TextureInput input{format, format, width, height, 1, 1, 0};
      table(slot, scene, input);
      table(slot + 1, processed, input);
      passes.push_back(Pipeline(gpu, root.Get(), vertexShader, folder / "scene-only.dxil", format));
      draw(passes.back().Get(), slot, 0);
      gpu.Transition(scene, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      draw(ui.Get(), 0, slot);
      gpu.SaveTarget(output, folder / (std::string("split-") + name + "-identity.raw"));
      passes.push_back(Pipeline(gpu, root.Get(), vertexShader, argv[3], format));
      draw(passes.back().Get(), slot + 1, slot);
      gpu.Transition(processed, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      draw(ui.Get(), 0, slot + 1);
      gpu.SaveTarget(output, folder / (std::string("split-") + name + "-tint.raw"));
      if (format == DXGI_FORMAT_R32G32B32A32_FLOAT) {
        auto *tint = passes.back().Get();
        passes.push_back(Pipeline(gpu, root.Get(), vertexShader, folder / "secondary-scene-only.dxil", format));
        gpu.Transition(scene, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        draw(passes.back().Get(), slot, 0);
        gpu.Transition(scene, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        draw(ui.Get(), 0, slot);
        gpu.SaveTarget(output, folder / "split-secondary-identity.raw");
        gpu.Transition(processed, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        draw(tint, slot + 1, slot);
        gpu.Transition(processed, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        draw(ui.Get(), 0, slot + 1);
        gpu.SaveTarget(output, folder / "split-secondary-tint.raw");
        if (argc == 5) {
          auto *alpha = createTarget(DXGI_FORMAT_R32_FLOAT, 7);
          table(7, scene, input, alpha);
          table(8, processed, input, alpha);
          passes.push_back(Pipeline(gpu, root.Get(), vertexShader, folder / "runtime-scene.dxil", format, true));
          gpu.Transition(scene, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
          draw(passes.back().Get(), slot, 0, 7);
          gpu.Transition(scene, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
          gpu.Transition(alpha, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
          passes.push_back(Pipeline(gpu, root.Get(), vertexShader, folder / "runtime-ui.dxil", outputFormat));
          auto *runtimeUi = passes.back().Get();
          draw(runtimeUi, 0, 7);
          gpu.SaveTarget(output, folder / "runtime-identity.raw");
          passes.push_back(Pipeline(gpu, root.Get(), vertexShader, argv[4], format));
          gpu.Transition(processed, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
          draw(passes.back().Get(), slot + 1, slot);
          gpu.Transition(processed, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
          draw(runtimeUi, 0, 8);
          gpu.SaveTarget(output, folder / "runtime-tint-overwrite-alpha.raw");
        }
      }
      slot += 2;
    }
    gpu.Finish();
    return 0;
  } catch (const std::exception &error) {
    fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
