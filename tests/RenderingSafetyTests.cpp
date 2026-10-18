#include "RenderingSafety.h"
#include <stdexcept>

static void require(bool condition) {
    if (!condition) throw std::runtime_error("Rendering safety regression");
}

int main() {
    using namespace reshade::api;
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
