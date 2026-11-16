#pragma once
#include <d3d9.h>
#include <wrl/client.h>
#include <array>
#include <algorithm>

namespace Rendering {
// D3DSBT_ALL does not include render-target/depth-stencil bindings. Read these
// from the native device, not event history which may be cleared at Present.
class D3D9StateSnapshot {
    Microsoft::WRL::ComPtr<IDirect3DStateBlock9> states;
    std::array<Microsoft::WRL::ComPtr<IDirect3DSurface9>, 4> targets;
    Microsoft::WRL::ComPtr<IDirect3DSurface9> depth;
    DWORD count = 0;
  public:
    bool Capture(IDirect3DDevice9* device) {
        if (!device || states) return false; // Do not overwrite a pending outer snapshot.
        D3DCAPS9 caps{};
        if (FAILED(device->GetDeviceCaps(&caps))) return false;
        count = std::min<DWORD>(caps.NumSimultaneousRTs, static_cast<DWORD>(targets.size()));
        if (!count || FAILED(device->CreateStateBlock(D3DSBT_ALL, &states))) { Clear(); return false; }
        for (DWORD i = 0; i < count; ++i) {
            const auto hr = device->GetRenderTarget(i, &targets[i]);
            if (FAILED(hr) && (i == 0 || hr != D3DERR_NOTFOUND)) { Clear(); return false; }
        }
        const auto hr = device->GetDepthStencilSurface(&depth);
        if (FAILED(hr) && hr != D3DERR_NOTFOUND) { Clear(); return false; }
        if (FAILED(states->Capture())) { Clear(); return false; }
        return true;
    }
    bool Restore(IDirect3DDevice9* device) {
        if (!device || !states) return false;
        // Target changes may reset viewport state. Restore bindings first,
        // including explicitly absent MRT/depth slots, then the captured state.
        bool ok = SUCCEEDED(device->SetDepthStencilSurface(nullptr));
        for (DWORD i = 0; i < count; ++i)
            ok = SUCCEEDED(device->SetRenderTarget(i, targets[i].Get())) && ok;
        ok = SUCCEEDED(device->SetDepthStencilSurface(depth.Get())) && ok;
        ok = SUCCEEDED(states->Apply()) && ok;
        Clear(); // Release every default-pool reference before a later Reset.
        return ok;
    }
    void Clear() {
        states.Reset(); depth.Reset();
        for (auto& target : targets) target.Reset();
        count = 0;
    }
};
}
