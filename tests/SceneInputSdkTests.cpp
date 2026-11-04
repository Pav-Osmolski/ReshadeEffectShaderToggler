#include "SceneInputPolicy.h"
#include "d3d12_impl_type_convert.hpp"
#include <cstdio>
#include <cstdlib>

static void require(bool value) {
    if (!value) {
        std::fputs("Scene input SDK conversion regression\n", stderr);
        std::exit(EXIT_FAILURE);
    }
}

int main() {
    using namespace reshade::api;
    D3D12_RESOURCE_DESC native = {};
    native.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    native.Width = 3824;
    native.Height = 1600;
    native.DepthOrArraySize = 1;
    native.MipLevels = 1;
    native.Format = DXGI_FORMAT_R11G11B10_FLOAT;
    native.SampleDesc.Count = 1;
    native.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    const auto resource = reshade::d3d12::convert_resource_desc(native, heap);
    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Format = native.Format;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels = 1;
    auto view = reshade::d3d12::convert_resource_view_desc(srv);
    std::printf("SDK 2D SRV: layers=%u, mips=%u, accepted=%d\n", view.texture.layers,
                view.texture.levels, Rendering::IsSceneInputShape(resource, view));
    std::fflush(stdout);
    require(view.type == resource_view_type::texture_2d && view.texture.layers == 0);
    require(Rendering::IsSceneInputShape(resource, view));
    for (auto format : {DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,
                        DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM}) {
        auto typedNative = native;
        typedNative.Format = format;
        auto typedSrv = srv;
        typedSrv.Format = format;
        require(Rendering::IsSceneInputShape(reshade::d3d12::convert_resource_desc(typedNative, heap),
                                             reshade::d3d12::convert_resource_view_desc(typedSrv)));
    }

    // Array metadata is meaningful for array views; they remain unsupported.
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    srv.Texture2DArray.MipLevels = 1;
    srv.Texture2DArray.ArraySize = 1;
    require(!Rendering::IsSceneInputShape(resource, reshade::d3d12::convert_resource_view_desc(srv)));
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
    require(!Rendering::IsSceneInputShape(resource, reshade::d3d12::convert_resource_view_desc(srv)));
    srv = {};
    srv.Format = native.Format;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MostDetailedMip = 1;
    srv.Texture2D.MipLevels = 1;
    require(!Rendering::IsSceneInputShape(resource, reshade::d3d12::convert_resource_view_desc(srv)));
    native.MipLevels = 2;
    srv.Texture2D.MostDetailedMip = 0;
    require(!Rendering::IsSceneInputShape(reshade::d3d12::convert_resource_desc(native, heap),
                                         reshade::d3d12::convert_resource_view_desc(srv)));
    native.MipLevels = 1;
    native.DepthOrArraySize = 2;
    require(!Rendering::IsSceneInputShape(reshade::d3d12::convert_resource_desc(native, heap), view));
    native.DepthOrArraySize = 1;
    native.SampleDesc.Count = 4;
    require(!Rendering::IsSceneInputShape(reshade::d3d12::convert_resource_desc(native, heap), view));
    std::puts("Production SDK scene-input conversion tests passed.");
}
