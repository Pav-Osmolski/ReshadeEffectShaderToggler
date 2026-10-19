#include "RenderingSafety.h"
#include <stdexcept>

static void require(bool condition) {
    if (!condition) throw std::runtime_error("Rendering safety regression");
}

int main() {
    using namespace reshade::api;
    require(Rendering::SupportsShaderCopy(device_api::d3d9));
    require(Rendering::SupportsShaderCopy(device_api::d3d10));
    require(Rendering::SupportsShaderCopy(device_api::d3d11));
    require(Rendering::SupportsShaderCopy(device_api::d3d12));
    require(!Rendering::SupportsShaderCopy(device_api::vulkan));
    require(!Rendering::SupportsShaderCopy(device_api::opengl));
    require(!Rendering::SameColorViewFormat(format::r16g16b16a16_float, format::r16g16b16a16_unorm));
    require(!Rendering::SameColorViewFormat(format::r10g10b10a2_uint, format::r10g10b10a2_unorm));
    require(Rendering::SameColorViewFormat(format::r8g8b8a8_unorm, format::r8g8b8a8_unorm_srgb));
    require(!Rendering::SameColorViewFormat(format::unknown, format::unknown));
    for (const auto api : {device_api::d3d10, device_api::d3d11, device_api::d3d12, device_api::vulkan}) {
        const auto hdr = Rendering::GetColorViewFormats(api, format::r16g16b16a16_float);
        require(hdr.linear == format::r16g16b16a16_float && hdr.srgb == hdr.linear);
        const auto hdr10 = Rendering::GetColorViewFormats(api, format::r10g10b10a2_unorm);
        require(hdr10.linear == format::r10g10b10a2_unorm && hdr10.srgb == hdr10.linear);
        const auto srgb = Rendering::GetColorViewFormats(api, format::r8g8b8a8_unorm_srgb);
        if (api == device_api::vulkan)
            require(srgb.linear == format::r8g8b8a8_unorm_srgb && srgb.srgb == srgb.linear);
        else
            require(srgb.linear == format::r8g8b8a8_unorm && srgb.srgb == format::r8g8b8a8_unorm_srgb);
    }
    const auto typeless = Rendering::GetColorViewFormats(device_api::d3d11, format::r8g8b8a8_typeless);
    require(typeless.linear == format::r8g8b8a8_unorm && typeless.srgb == format::r8g8b8a8_unorm_srgb);

    resource_desc desc(1920, 1080, 1, 1, format::r16g16b16a16_float, 1,
                       memory_heap::default_, resource_usage::render_target);
    require(Rendering::SupportsWholePreviewCopy(desc));
    desc.texture.samples = 4;
    require(!Rendering::SupportsWholePreviewCopy(desc));
    desc.texture.samples = 1;
    desc.texture.levels = 2;
    require(!Rendering::SupportsWholePreviewCopy(desc));
    desc.texture.levels = 1;
    desc.texture.depth_or_layers = 6;
    require(!Rendering::SupportsWholePreviewCopy(desc));
    desc.texture.depth_or_layers = 1;
    desc.texture.width = 0;
    require(!Rendering::SupportsWholePreviewCopy(desc));
    desc.texture.width = 1920;
    desc.type = resource_type::texture_3d;
    require(!Rendering::SupportsWholePreviewCopy(desc));

    std::vector<uint32_t> set0, set1;
    uint32_t cursor = 0;
    const uint32_t initial[] = { 256, 512, 1024 };
    require(Rendering::SaveDynamicOffsets(set0, 1, cursor, 3, initial));
    require(Rendering::SaveDynamicOffsets(set1, 2, cursor, 3, initial));
    require(set0 == std::vector<uint32_t>{256});
    require(set1 == (std::vector<uint32_t>{512, 1024}));
    // Bind only set 1: set 0's offset must survive.
    const uint32_t changed[] = { 2048, 4096 };
    cursor = 0;
    require(Rendering::SaveDynamicOffsets(set1, 2, cursor, 2, changed));
    require(set0 == std::vector<uint32_t>{256});
    require(set1 == (std::vector<uint32_t>{2048, 4096}));
    cursor = 0;
    require(!Rendering::SaveDynamicOffsets(set1, 3, cursor, 2, changed));
    require(set1.empty());
    require(!Rendering::SaveDynamicOffsets(set1, 1, cursor, 1, nullptr));
    require(Rendering::SaveDynamicOffsets(set1, 0, cursor, 0, nullptr));
}
