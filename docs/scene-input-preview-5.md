# REST Enhanced Scene input preview 5

Preview 5 addresses the flicker reported with Preview 4. The reporter confirmed full-strength scene effects, motion-blur coverage and correct technique colours. During flickering frames, REST reported **waiting for private GPU resources**.

## Change

At the reported 3818×1600 resolution, each full-precision scene/alpha pair uses about 117 MiB. The 512 MiB pool holds four pairs. Preview 4 waited for the CPU to observe GPU completion before reusing an entire pair and its descriptors, so an otherwise valid frame could run out of private resources and use the original unprocessed draw.

Preview 5 manages the large images separately from each recording's immutable descriptor tables. Once a command recording retires, another recording can reuse its images with a GPU queue wait on the prior completion fences. Every recording retains its own descriptors and pipeline references until completion. Sequential draws in the same recording can also share their scratch images. Only queues that submitted the affected recording participate in its retirement tracking.

The memory limit remains bounded. Copied preview diagnostics now include image-pair and descriptor counts, memory usage, reuse and GPU handoff counts, and pool-pressure skips.

## Install and test

1. Replace the add-on with the matching architecture from **REST-Scene-Input-Preview-5-Modern.zip**. Warframe uses `.addon64`. Keep the same pinned DXC pair installed in `REST-DXC`.
2. Keep Preview 4's native HDR setup and **Warframe HDR compositor split (experimental)** enabled after restart, with descriptor tracking active. Use **Pixel / Slot 2 / Binding 0**, Auto Scene Colour off, clamping off and flip off. Keep RenoDX off for this comparison. The [Preview 4 setup guide](scene-input-preview-4.md) covers the compiler files and restart setting.
3. Test **REST_SceneBoundaryCheck** alone with motion blur enabled. End shader hunting, then open and close the ESC menu and the UI elements that previously caused constant flickering. Check TAA, DLSS or FSR using the existing shader selection.
4. After a successful tint test, test the desired Marty’s Mods technique alone. Report whether tint/effect strength stays stable while moving and opening the UI.

If flickering persists, copy the preview diagnostics from this build. The new pool section will distinguish active recordings, retained descriptor sets and allocation pressure. Include `ReShade.log` for a crash or rendering error.

## Verification

A native D3D12 regression holds the GPU behind a fence while recording 96 submissions across two queues. All submissions share one scratch allocation and retain separate descriptor tables. After releasing the gate, every output must match its recording's input. It also checks active ownership, sequential reuse, retirement without submission and failed retirement. The existing rendering, pinned SDK conversion and shader-policy checks run alongside it.

The compositor shaders, full-precision scene format and saved alpha follow the Preview 4 implementation. Live menu, TAA/DLSS/FSR and long-session results remain the purpose of this candidate.

The file version remains **1.7.3.680**. Use **Scene input preview 5 (pre-release)** in the overlay and `BUILD-INFO.txt` to identify the candidate.
