#include "renderdoc_replay.h"
#include "split-inputs.h"
#include <cstdio>
#include <fstream>
#include <functional>
#include <string>
#include <vector>
REPLAY_PROGRAM_MARKER()
int main(int argc, char **argv) {
  if (argc < 4)
    return 2;
  RENDERDOC_SetDebugLogFile("replay-debug.log");
  GlobalEnvironment env;
  env.enumerateGPUs = false;
  RENDERDOC_InitialiseReplay(env, {});
  printf("LOG %s\n", RENDERDOC_GetLogFile());
  fflush(stdout);
  auto cap = RENDERDOC_OpenCaptureFile();
  auto open = cap->OpenFile(argv[1], "", nullptr);
  if (!open.OK()) {
    fprintf(stderr, "open %d\n", int(open.code));
    return 3;
  }
  auto result = cap->OpenCapture(ReplayOptions(), nullptr);
  if (!result.first.OK()) {
    fprintf(stderr, "replay %d\n", int(result.first.code));
    return 4;
  }
  auto ctrl = result.second;
  uint32_t event = uint32_t(strtoul(argv[2], nullptr, 10));
  ResourceId target;
  std::function<void(const ActionDescription &)> find =
      [&](const ActionDescription &a) {
        if (a.eventId == event)
          target = a.outputs[0];
        for (auto &c : a.children)
          find(c);
      };
  for (auto &a : ctrl->GetRootActions())
    find(a);
  if (target == ResourceId())
    return 5;
  ctrl->SetFrameEvent(event, true);
  auto original = ctrl->GetD3D12PipelineState()->pixelShader.resourceId;
  auto write = [&](const std::string &path, const bytebuf &data) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char *>(data.data()), data.size());
    if (!f) {
      fprintf(stderr, "Could not write %s\n", path.c_str());
      std::exit(8);
    }
  };
  auto refl = ctrl->GetD3D12PipelineState()->pixelShader.reflection;
  if (!refl)
    return 7;
  write(std::string(argv[3]) + "/original.shader", refl->rawBytes);
  for (auto &access : ctrl->GetDescriptorAccess()) {
    if (access.stage != ShaderStage::Pixel || access.staticallyUnused)
      continue;
    for (auto &descriptor : ctrl->GetDescriptors(access.descriptorStore,
                                                 {DescriptorRange(access)})) {
      if (descriptor.type == DescriptorType::ConstantBuffer)
        write(std::string(argv[3]) + "/constants.raw",
              ctrl->GetBufferData(descriptor.resource, descriptor.byteOffset,
                                  descriptor.byteSize));
      if (CategoryForDescriptorType(access.type) !=
              DescriptorCategory::ReadOnlyResource ||
          !refl || access.index >= refl->readOnlyResources.size())
        continue;
      auto reg = refl->readOnlyResources[access.index].fixedBindNumber;
      if (reg == 8)
        write(std::string(argv[3]) + "/ui.raw",
              ctrl->GetTextureData(descriptor.resource, Subresource()));
    }
  }
  auto save = [&](const std::string &name) {
    auto data = ctrl->GetTextureData(target, Subresource());
    write(name + ".raw", data);
    TextureSave image;
    image.resourceId = target;
    image.destType = FileType::PNG;
    auto s = ctrl->SaveTexture(image, (name + ".png").c_str());
    printf("OUTPUT %s %zu bytes png=%d\n", name.c_str(), data.size(),
           int(s.code));
    fflush(stdout);
  };
  save(std::string(argv[3]) + "/original");
  int firstVariant = 4;
  if (argc > 4 && std::string(argv[4]) == "--split-inputs") {
    try {
      ExportSplitInputs(*ctrl, argv[3]);
    } catch (const std::exception &error) {
      fprintf(stderr, "Input export failed: %s\n", error.what());
      return 9;
    }
    ++firstVariant;
  }
  for (int i = firstVariant; i < argc; ++i) {
    std::string name = argv[i];
    std::ifstream f(name + ".dxil", std::ios::binary);
    std::vector<char> input((std::istreambuf_iterator<char>(f)), {});
    bytebuf bytes;
    bytes.resize(input.size());
    memcpy(bytes.data(), input.data(), input.size());
    auto shader = ctrl->BuildTargetShader("main", ShaderEncoding::DXIL, bytes,
                                          {}, ShaderStage::Pixel);
    printf("BUILD %s %s\n", name.c_str(), shader.second.c_str());
    fflush(stdout);
    if (shader.first == ResourceId())
      return 6;
    ctrl->ReplaceResource(original, shader.first);
    ctrl->ClearReplayCache();
    ctrl->SetFrameEvent(event, true);
    save(name);
    ctrl->RemoveReplacement(original);
    ctrl->FreeTargetResource(shader.first);
  }
  for (auto &m : ctrl->GetDebugMessages())
    printf("DEBUG %s\n", m.description.c_str());
  ctrl->Shutdown();
  cap->Shutdown();
  RENDERDOC_ShutdownReplay();
  return 0;
}
