#pragma once

#include "renderdoc_replay.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <string>

// Export only the input state needed by the captured compositor. Files are
// deliberately kept outside the repository alongside the original capture.
inline void ExportSplitInputs(IReplayController &controller, const std::string &directory) {
  const auto &state = *controller.GetD3D12PipelineState();
  const auto &rasterizer = state.rasterizer;
  if (rasterizer.viewports.size() != 1 || rasterizer.scissors.size() != 1)
    throw std::runtime_error("Expected one viewport and scissor");
  const auto mesh = controller.GetPostVSData(0, 0, MeshDataStage::VSOut);
  if (mesh.vertexResourceId == ResourceId() || mesh.indexResourceId != ResourceId() ||
      mesh.numIndices > 16 || mesh.vertexByteStride > 128)
    throw std::runtime_error("Unexpected compositor mesh");

  auto write = [&](const std::string &name, const bytebuf &data) {
    std::ofstream file(directory + "/" + name, std::ios::binary);
    file.write(reinterpret_cast<const char *>(data.data()), data.size());
    if (!file || data.empty())
      throw std::runtime_error("Could not export " + name);
  };
  std::ofstream manifest(directory + "/split-inputs.tsv");
  manifest << std::setprecision(std::numeric_limits<float>::max_digits10);
  const auto &vp = rasterizer.viewports[0];
  const auto &sc = rasterizer.scissors[0];
  manifest << "viewport " << vp.x << ' ' << vp.y << ' ' << vp.width << ' ' << vp.height << ' '
           << vp.minDepth << ' ' << vp.maxDepth << '\n';
  manifest << "scissor " << sc.x << ' ' << sc.y << ' ' << sc.x + sc.width << ' ' << sc.y + sc.height << '\n';
  manifest << "mesh " << mesh.numIndices << ' ' << mesh.vertexByteStride << ' ' << unsigned(mesh.topology) << '\n';
  write("vertices.raw", controller.GetBufferData(mesh.vertexResourceId, mesh.vertexByteOffset,
                                                uint64_t(mesh.vertexByteStride) * mesh.numIndices));
  const auto &reflection = *state.pixelShader.reflection;
  const auto textures = controller.GetTextures();
  for (const auto &access : controller.GetDescriptorAccess()) {
    if (access.stage != ShaderStage::Pixel || access.staticallyUnused)
      continue;
    const DescriptorRange range(access);
    if (IsSamplerDescriptor(access.type)) {
      const auto samplers = controller.GetSamplerDescriptors(access.descriptorStore, {range});
      if (access.index >= reflection.samplers.size() || samplers.size() != 1)
        throw std::runtime_error("Unexpected sampler binding");
      const auto &s = samplers[0];
      manifest << "sampler " << reflection.samplers[access.index].fixedBindNumber << ' '
               << unsigned(s.addressU) << ' ' << unsigned(s.addressV) << ' ' << unsigned(s.addressW) << ' '
               << unsigned(s.filter.minify) << ' ' << unsigned(s.filter.magnify) << ' ' << unsigned(s.filter.mip)
               << ' ' << unsigned(s.filter.filter) << ' ' << s.maxAnisotropy << ' ' << s.minLOD << ' ' << s.maxLOD
               << ' ' << s.mipBias;
      for (float value : s.borderColorValue.floatValue)
        manifest << ' ' << value;
      manifest << '\n';
    } else if (CategoryForDescriptorType(access.type) == DescriptorCategory::ReadOnlyResource) {
      const auto descriptors = controller.GetDescriptors(access.descriptorStore, {range});
      if (access.index >= reflection.readOnlyResources.size() || descriptors.size() != 1)
        throw std::runtime_error("Unexpected texture binding");
      const auto &d = descriptors[0];
      const auto t = std::find_if(textures.begin(), textures.end(), [&](const TextureDescription &candidate) {
        return candidate.resourceId == d.resource;
      });
      if (t == textures.end() || t->arraysize != 1 || t->msSamp != 1 || d.firstMip != 0)
        throw std::runtime_error("Unexpected compositor texture");
      const auto reg = reflection.readOnlyResources[access.index].fixedBindNumber;
      const auto mips = std::min<uint32_t>(t->mips, d.numMips);
      manifest << "texture " << reg << ' ' << d.format.Name().c_str() << ' ' << t->width << ' '
               << t->height << ' ' << t->depth << ' ' << mips << ' '
               << unsigned(d.swizzle.red) << ' ' << unsigned(d.swizzle.green) << ' '
               << unsigned(d.swizzle.blue) << ' ' << unsigned(d.swizzle.alpha) << '\n';
      for (uint32_t mip = 0; mip < mips; ++mip)
        write("t" + std::to_string(reg) + "-" + std::to_string(mip) + ".raw",
              controller.GetTextureData(d.resource, Subresource(mip)));
    }
  }
  if (!manifest)
    throw std::runtime_error("Could not write split-inputs.tsv");
}
