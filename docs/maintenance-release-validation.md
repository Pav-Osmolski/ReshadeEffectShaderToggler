# Maintenance release validation

This release selects general compatibility and lifecycle improvements from the development work. It starts at the maintained stable branch rather than merging the experimental branch. Modern targets v1.7.4.680; Legacy targets v1.6.8.580. The game results below were confirmed on 16 November 2026.

## Scope

Included: D3D9 standalone render-target surfaces and last-runtime pre-Reset cleanup; repeat-safe view disposal; views released before backing resources; device-scoped typed caches; same-device runtime replacement; retained removed-group objects and retired-work guards; independent pixel/vertex/compute effect queue evaluation.

Removed groups retain their CPU objects until add-on teardown to protect outstanding pointers. They are marked retired before removal callbacks begin, and the retired flag is atomic. GPU buffers follow the synchronized removal path. This change does not promise that every other group setting is safe for arbitrary concurrent mutation.

Excluded: experimental scene-input copies and draw replacement, Warframe shader profiles/patching, DXC loading, descriptor/producer diagnostics and scratch-image pooling. This release does not claim to fix Warframe HDR issue #22. The research remains on its separate feature branch.

D3D9 surfaces are accepted as render targets. This does not make a standalone surface shader-sampleable or enable Automatic Scene Colour, modern HDR presentation or every preview operation on D3D9. Existing D3D10/D3D11/D3D12/Vulkan features are retained.

## Automated and native coverage

`scripts/test-rendering-safety.ps1` runs the existing backend/format checks plus `scripts/test-maintenance.ps1`.

- The maintenance test compiles the production `GlobalResourceView.cpp`, `ToggleGroup.cpp` and configuration implementation against the pinned SDK. It checks alias-safe repeated disposal, retained map-node addresses, terminal retirement, cloning and atomic retirement publication in x64 and x86. It requires VS2022; older local compilers explicitly skip this portion rather than substituting production code.
- The native D3D9 test compiles the same production view class with a small API host backed by real D3D9 COM references. A hidden window is used; no interactive window is shown. Each architecture checks 12 surface/texture cycles, exact pixel readback, an expected failed Reset while default-pool view references remain, and 24 successful resets after releasing those references. It verifies repeated disposal does not duplicate releases.
- The native test does not inject the add-on into a game or run the full ReShade D3D9 implementation. Its host models D3D9 view-reference behaviour; game hunting, effect placement and the complete runtime destruction callback still need game testing.
- `scripts/test-maintenance.ps1 -RequireNative` fails if native coverage is unavailable. CI without a D3D9 HAL device may explicitly skip that part with exit code 77; a skip is not a pass. Lifecycle tests still run with VS2022.

On the maintainer's local Windows machine, Modern/API20 and Legacy/API7 both passed native x64 and x86 D3D9 checks on 15 November 2026. The existing rendering-safety tests also passed. Record hosted build results and exact candidate commits in the release PRs; do not reuse a different branch's results to certify these candidates.

## RE4 crash regression

The earlier Modern candidate crashed in Resident Evil 4 Ultimate HD Edition, x86, with ReShade 6.8.0.2156 after selecting a pixel shader with Shader Resource View as the destination. The retained log and two Windows crash reports/dumps provide evidence even without a manually recorded shader hash. The inspected dump faults in `bio4.exe+0x54a1bd` while dereferencing a null surface for Release, immediately after a depth-surface binding call.

REST's old D3D9 copy-state restoration combined a native state block with event-tracked target/depth handles. That event history is cleared at presentation and need not reflect persistent native bindings. A native test reproduces the loss of a bound depth surface with this path. The replacement captures actual native render-target/depth bindings and restores them before applying the other state, preserving viewport/scissor as well. It handles an originally absent depth surface, rejects nested capture and releases snapshot references before Reset. A failed capture skips the copy before it alters native state.

Local x86/x64 tests pass for both SDK lines. The fixed Modern x86 build subsequently passed RE4 hunting, normal effects/group operations and resolution/fullscreen changes. The retained ReShade log records 440 hunting selections across 19 pixel shaders, no REST snapshot/copy errors and a clean exit. This RE4 result applies to Modern/ReShade 6.8.0.2156, not Legacy. Hunting selections now log `REST hunting selection: PS shader 0x...` (or VS/CS) before publication to rendering threads. Direct, keyboard and marked navigation and suppression of repeated-selection log flooding are tested against the production ShaderManager.

## Recorded game coverage

Use the candidate matching the installed ReShade line and the game's architecture. Back up the current add-on and INI; keep the same working configuration for comparison.

| Configuration | Required observation | Recorded result |
| --- | --- | --- |
| D3D9 x86 representative title | Hunt/mark shaders, place a visible effect, toggle and remove groups, change resolution, Alt-Tab and switch window/fullscreen repeatedly; no crash, failed reset or missing effects after recreation. | Modern v1.7.4.680 / ReShade 6.8.0.2156: RE4 Ultimate HD Edition passed hunting, effects/groups and resolution/fullscreen changes after the native state fix. Legacy RE4 was not tested. |
| D3D9 x64 title where available | Repeat the reset/hunting checks; report if no suitable title is available. | Not freshly game-tested |
| D3D10 representative title where available | Hunting, effect placement, group removal and reload retain their normal behaviour. | Not freshly game-tested |
| D3D11, BG3 (x64) | Existing configured effects and hunting work; remove/recreate groups, reload effects and change resolution. | Maintainer reports both Modern v1.7.4.680 and Legacy v1.6.8.580 work perfectly, including with `renodx-dlss5.addon64` active. Exact ReShade patch versions and individual matrix steps were not separately recorded. |
| D3D12 representative title | Existing stable effect-placement/hunting workflow works without experimental scene-input features. | Not freshly game-tested |
| Vulkan, BG3 (x64) | Existing injection, preview/hunting, group removal and resolution/reload behaviour remain stable. | Maintainer reports both Modern v1.7.4.680 and Legacy v1.6.8.580 work perfectly, including with `renodx-dlss5.addon64` active. Exact ReShade patch versions and individual matrix steps were not separately recorded. |

For each tested case, record game, graphics API, ReShade version, REST candidate commit and architecture. A pass on Modern does not automatically certify Legacy. There are no Automatic Scene Colour algorithm changes in this maintenance selection, but existing configured behaviour remains a smoke-test target. Do not claim every matrix row has passed if only some titles are available.

## Attribution

Reviewed BadassBaboon's fork at `372c4a83725df11f62adfc3ce41822774fb40855`.

- D3D9 surface/reset and destruction ordering: [f403b4778969808ed369a057d060c1e858d5e96a](https://github.com/BadassBaboon/ReshadeEffectShaderToggler/commit/f403b4778969808ed369a057d060c1e858d5e96a). The fork credits CrazyDaifu for the D3D9 contribution.
- Removed-group lifetime: [584e36b](https://github.com/BadassBaboon/ReshadeEffectShaderToggler/commit/584e36b). Adapted to Enhanced with retained map nodes and explicit retired-work guards.
- Enhanced's existing format handling, Automatic Scene Colour and backend safeguards are preserved. Broad 8-bit restrictions, disabled previews, controller UI, geometry filters and unrelated key migrations are not imported.

Original file notices remain in place. The fork's [MIT notice](fork-license.txt) is included alongside the repository [license](../LICENSE).
