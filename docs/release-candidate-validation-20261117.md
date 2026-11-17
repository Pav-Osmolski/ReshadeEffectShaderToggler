# Stable-release candidate validation — 17 November 2026

Targets: REST Enhanced v1.7.5.680 (Modern) and REST Enhanced v1.6.9.580 (ReShade 5.x). Neither has been published. Modern is intended as Latest after both lines pass the final local checks. Optional Vulkan scene-copy support remains off by default.

| Check | Modern | Legacy |
|---|---|---|
| Native x64/x86 build and hosted regressions | Passed candidate be17040, run 44068434639 | Passed candidate bd5dd66, run 44068439484 |
| Enshrouded SDR, DLSS Quality, 100% output scale | Passed rendering code 08a497c | Pending |
| Enshrouded SDR 67%, HDR 100%/67% placement | Passed rendering code 08a497c | Pending |
| Enshrouded group/Auto toggles and shader hunting | Passed tested operations | Pending |
| Enshrouded windowed/fullscreen and normal exit | Passed one round-trip, code 0 | Pending |
| BG3 D3D11 | Passed, maintainer report | Passed, maintainer report |
| BG3 Vulkan | Passed, maintainer report | Passed, maintainer report |

Enshrouded used build 1287369 and ReShade 6.8.0.2155, existing starter world, 3840x2160 output, isolated CinematicDOF, and marked scene PS 0x46c3d744. SDR gameplay HUD stayed sharp while scene blur remained; group off blurred the compass/hotbar, on restored them. Reduced-scale/HDR comparisons used the Display menu over that loaded world. HDR at 67% reported a 1718x967 -> 3840x2160 Vulkan image blit with increasing counters; group off/on blurred/restored menu text. SDR hunting used Next/Prev; HDR hunting selected the marked FP16 target. Colour preview completeness and HDR colour accuracy were not certified. Testing did not traverse multiple world areas.

Required launch conditions: `--keep-vulkan-layers`, `REST_VULKAN_SCENE_TRANSFERS=1`, resource shim none, plus local shutdown workaround `RESHADE_DISABLE_GRAPHICS_HOOK=1`. This is not an add-on-only fix for every reported exit crash. No new crash dumps appeared; the existing depth-stencil-in-use warning persists. REST/ReShade configuration and game graphics preferences were restored after the run.

On 17 November 2026, the maintainer confirmed that the supplied Modern v1.7.5.680 and Legacy v1.6.9.580 candidates both pass BG3 on D3D11 and Vulkan. Exact ReShade patch versions, other add-ons, HDR settings and individual operations were not separately recorded for this report. The earlier stable tests included renodx-dlss5; that detail is not assumed for these candidates. Legacy Enshrouded SDR/HDR runtime checks remain pending. Warframe work remains separate. Update this record from actual evidence before publishing full stable releases.
