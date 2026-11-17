#include "DeviceStub.h"
#include "GlobalResourceView.h"
#include "D3D9StateSnapshot.h"
#include <d3d9.h>
#include <wrl/client.h>
#include <cstdio>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "user32.lib")
using namespace reshade::api;
using Microsoft::WRL::ComPtr;
static void Check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
static void Hr(HRESULT hr) { Check(SUCCEEDED(hr), "Native D3D9 operation failed"); }

// Small ReShade API host backed by real D3D9 COM references. This exercises
// REST's production GlobalResourceView; it is not the full ReShade runtime.
struct SurfaceHost : TestHost::DeviceStub {
    device_api get_api() const override { return device_api::d3d9; }
    unsigned created = 0, destroyed = 0;
    resource_desc get_resource_desc(resource handle) const override {
        auto* object = reinterpret_cast<IDirect3DResource9*>(static_cast<uintptr_t>(handle.handle));
        D3DSURFACE_DESC desc{};
        const bool surface = object->GetType() == D3DRTYPE_SURFACE;
        if (surface) Hr(static_cast<IDirect3DSurface9*>(object)->GetDesc(&desc));
        else Hr(static_cast<IDirect3DTexture9*>(object)->GetLevelDesc(0, &desc));
        resource_desc result(desc.Width, desc.Height, 1, 1, format::b8g8r8a8_unorm, 1, memory_heap::default_,
            surface ? resource_usage::render_target : resource_usage::render_target | resource_usage::shader_resource);
        if (surface) result.type = resource_type::surface;
        return result;
    }
    bool create_resource_view(resource handle, resource_usage usage, const resource_view_desc& desc, resource_view* out) override {
        auto* object = reinterpret_cast<IDirect3DResource9*>(static_cast<uintptr_t>(handle.handle));
        *out = {0};
        const bool srgb = desc.format == format::b8g8r8a8_unorm_srgb;
        IUnknown* view = nullptr;
        if (object->GetType() == D3DRTYPE_SURFACE) {
            if (usage != resource_usage::render_target) return false;
            object->AddRef(); view = object;
        } else if (usage == resource_usage::shader_resource) {
            object->AddRef(); view = object;
        } else {
            IDirect3DSurface9* surface = nullptr;
            if (FAILED(static_cast<IDirect3DTexture9*>(object)->GetSurfaceLevel(0, &surface))) return false;
            view = surface;
        }
        ++created;
        *out = {reinterpret_cast<uintptr_t>(view) | (srgb ? 1ull : 0ull)};
        return true;
    }
    void destroy_resource_view(resource_view view) override {
        reinterpret_cast<IUnknown*>(static_cast<uintptr_t>(view.handle & ~1ull))->Release();
        ++destroyed;
    }
};

int main() {
    try {
        const HINSTANCE module = GetModuleHandleW(nullptr);
        WNDCLASSW wc{};
        wc.lpfnWndProc = DefWindowProcW; wc.hInstance = module; wc.lpszClassName = L"RESTHeadlessD3D9Test";
        Check(RegisterClassW(&wc) != 0, "Could not register hidden D3D9 test window");
        const HWND window = CreateWindowExW(0, wc.lpszClassName, L"REST D3D9 regression", WS_OVERLAPPED,
                                            0, 0, 128, 128, nullptr, nullptr, module, nullptr);
        Check(window != nullptr, "Could not create hidden D3D9 test window");
        ComPtr<IDirect3D9> api;
        api.Attach(Direct3DCreate9(D3D_SDK_VERSION));
        Check(api != nullptr, "D3D9 unavailable");
        D3DPRESENT_PARAMETERS present{};
        present.BackBufferWidth = present.BackBufferHeight = 128;
        present.BackBufferFormat = D3DFMT_UNKNOWN;
        present.BackBufferCount = 1;
        present.SwapEffect = D3DSWAPEFFECT_DISCARD;
        present.hDeviceWindow = window; present.Windowed = TRUE;
        ComPtr<IDirect3DDevice9> native;
        HRESULT hr = api->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &present, &native);
        if (FAILED(hr)) {
            std::printf("SKIP: native D3D9 HAL unavailable (0x%08lx); no runtime coverage claimed.\n", static_cast<unsigned long>(hr));
            DestroyWindow(window); UnregisterClassW(wc.lpszClassName, module);
            return 77;
        }
        SurfaceHost host;
        {
            ComPtr<IDirect3DSurface9> gameTarget, gameDepth, privateTarget, secondTarget;
            Hr(native->GetRenderTarget(0, &gameTarget));
            D3DCAPS9 caps{}; Hr(native->GetDeviceCaps(&caps));
            if (caps.NumSimultaneousRTs > 1) {
                D3DSURFACE_DESC targetDesc{}; Hr(gameTarget->GetDesc(&targetDesc));
                Hr(native->CreateRenderTarget(128, 128, targetDesc.Format, D3DMULTISAMPLE_NONE, 0, FALSE, &secondTarget, nullptr));
            }
            Hr(native->CreateDepthStencilSurface(128, 128, D3DFMT_D16, D3DMULTISAMPLE_NONE, 0, TRUE, &gameDepth, nullptr));
            Hr(native->CreateRenderTarget(128, 128, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &privateTarget, nullptr));
            Hr(native->SetDepthStencilSurface(gameDepth.Get()));
            ComPtr<IDirect3DStateBlock9> oldRestore;
            Hr(native->CreateStateBlock(D3DSBT_ALL, &oldRestore));
            Hr(oldRestore->Capture());
            Hr(native->SetDepthStencilSurface(nullptr));
            Hr(native->SetRenderTarget(0, privateTarget.Get()));
            // Reproduce the old path after event history lost the depth binding.
            Hr(native->SetRenderTarget(0, gameTarget.Get()));
            Hr(oldRestore->Apply());
            ComPtr<IDirect3DSurface9> observed;
            Check(native->GetDepthStencilSurface(&observed) == D3DERR_NOTFOUND && !observed,
                  "Control case did not reproduce depth loss with a state block alone");
            oldRestore.Reset();
            for (unsigned mode = 0; mode < 2; ++mode) {
                Hr(native->SetDepthStencilSurface(mode == 0 ? gameDepth.Get() : nullptr));
                if (secondTarget) Hr(native->SetRenderTarget(1, secondTarget.Get()));
                D3DVIEWPORT9 expected{4, 5, 96, 80, 0.1f, 0.9f};
                RECT scissor{2, 3, 100, 99};
                Hr(native->SetViewport(&expected)); Hr(native->SetScissorRect(&scissor));
                Rendering::D3D9StateSnapshot snapshot;
                Check(snapshot.Capture(native.Get()), "Native state snapshot failed");
                Check(!snapshot.Capture(native.Get()), "Nested capture overwrote an outer snapshot");
                Hr(native->SetDepthStencilSurface(nullptr));
                Hr(native->SetRenderTarget(0, privateTarget.Get()));
                if (secondTarget) Hr(native->SetRenderTarget(1, nullptr));
                D3DVIEWPORT9 changed{0, 0, 64, 64, 0.0f, 1.0f};
                Hr(native->SetViewport(&changed));
                Check(snapshot.Restore(native.Get()), "Native state restore failed");
                ComPtr<IDirect3DSurface9> target, depth;
                Hr(native->GetRenderTarget(0, &target));
                Check(target.Get() == gameTarget.Get(), "Original render target was not restored");
                if (secondTarget) {
                    ComPtr<IDirect3DSurface9> second;
                    Hr(native->GetRenderTarget(1, &second));
                    Check(second.Get() == secondTarget.Get(), "Additional render target was not restored");
                }
                const auto depthResult = native->GetDepthStencilSurface(&depth);
                Check(mode == 0 ? SUCCEEDED(depthResult) && depth.Get() == gameDepth.Get() :
                      depthResult == D3DERR_NOTFOUND && !depth, "Original depth presence/absence was not restored");
                D3DVIEWPORT9 actual{}; RECT actualScissor{};
                Hr(native->GetViewport(&actual)); Hr(native->GetScissorRect(&actualScissor));
                Check(actual.X == expected.X && actual.Y == expected.Y && actual.Width == expected.Width &&
                      actual.Height == expected.Height && actual.MinZ == expected.MinZ && actual.MaxZ == expected.MaxZ,
                      "Viewport changed during target restoration");
                Check(EqualRect(&actualScissor, &scissor), "Scissor state was not restored");
                Check(!snapshot.Restore(native.Get()), "Consumed snapshot was reused");
            }
            Hr(native->SetDepthStencilSurface(nullptr));
            if (secondTarget) Hr(native->SetRenderTarget(1, nullptr));
            std::puts("REPRODUCED depth loss with old restoration; native snapshot restores target, depth/null, viewport and scissor.");
        }
        Hr(native->Reset(&present)); // Snapshot must not retain default-pool references.
        for (unsigned iteration = 0; iteration < 12; ++iteration) {
            ComPtr<IDirect3DSurface9> surface, readback;
            Hr(native->CreateRenderTarget(64, 64, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &surface, nullptr));
            Rendering::GlobalResourceView view(&host, {reinterpret_cast<uintptr_t>(surface.Get())}, format::b8g8r8a8_unorm);
            Check(view.rtv != 0 && view.rtv_srgb != 0 && view.srv == 0, "Standalone surface views were not accepted correctly");
            Hr(native->ColorFill(surface.Get(), nullptr, 0xff336699));
            Hr(native->CreateOffscreenPlainSurface(64, 64, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &readback, nullptr));
            Hr(native->GetRenderTargetData(surface.Get(), readback.Get()));
            D3DLOCKED_RECT pixels{};
            Hr(readback->LockRect(&pixels, nullptr, D3DLOCK_READONLY));
            Check(*static_cast<const DWORD*>(pixels.pBits) == 0xff336699, "D3D9 target output mismatch");
            Hr(readback->UnlockRect()); readback.Reset();
            surface.Reset(); // REST's view references are now the only owners.
            present.BackBufferWidth = 128 + (iteration % 3) * 16;
            present.BackBufferHeight = 128 + (iteration % 2) * 16;
            Check(FAILED(native->Reset(&present)), "Control case: live default-pool surface did not block Reset");
            view.Dispose(); view.Dispose();
            Check(host.created == host.destroyed, "Repeated surface disposal leaked or duplicated COM releases");
            Hr(native->Reset(&present));
            ComPtr<IDirect3DTexture9> texture;
            Hr(native->CreateTexture(64, 64, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture, nullptr));
            Rendering::GlobalResourceView textureViews(&host, {reinterpret_cast<uintptr_t>(texture.Get())}, format::b8g8r8a8_unorm);
            Check(textureViews.rtv != 0 && textureViews.srv != 0, "Texture target views unavailable");
            texture.Reset();
            textureViews.Dispose(); textureViews.Dispose();
            Check(host.created == host.destroyed, "Texture/surface views were not all released");
            Hr(native->Reset(&present));
        }
        native.Reset(); api.Reset();
        DestroyWindow(window); UnregisterClassW(wc.lpszClassName, module);
        std::printf("Native D3D9 %zu-bit: 12 surface/texture cycles, exact pixels and 24 successful resets after production view disposal.\n", sizeof(void*) * 8);
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
