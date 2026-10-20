# REST Enhanced HDR diagnostics 1 (pre-release)

This is a diagnostic build for [issue #22](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/issues/22), based on the current stable Modern or Legacy release. It is **not a confirmed HDR fix**. The four-part file version remains the stable baseline; identify this build by the **HDR diagnostics 1 (pre-release)** label above the hunting preview and the source commit in `BUILD-INFO.txt`.

## Install

Back up your current REST add-on and `ReshadeEffectShaderToggler.ini`. Close the game, then replace the existing add-on with the matching diagnostic build: `.addon64` for a 64-bit game such as Warframe, `.addon32` for a 32-bit game. Use the Modern ZIP for ReShade 6.8.0+; use the explicitly named Legacy ZIP for ReShade 5.8.0+. Install only one REST add-on. Keep your existing configuration and helper shaders.

## What changed

- The preview reports the actual matched shader hash/stage on Direct3D, including vertex/compute hunting, rather than displaying an unpopulated zero hash.
- Storage format and selected typed view format are shown separately.
- **Copy preview diagnostics** works in both automatic and manual target modes. It copies the selected shader, target/view formats, all colour targets tracked at that draw, capture timing/path, status and relevant settings.
- Missing targets, resource/view failures and unknown resource states have explicit status messages. A recorded capture is labelled as recorded, rather than implying that GPU pixels have been verified.
- The target-selection policy, HDR conversion and rendering/injection behavior are unchanged. Automatic selection still follows primary RTV 0; it does not discover compute UAV outputs.

## Warframe test

1. Start with **native HDR**, without an HDR add-on, and untick all individual ReShade techniques, including SOLARIS. Leave REST and ReShade's global effects switch enabled: disabling the global switch also stops REST's hunting/preview updates. Rescan shaders after changing HDR mode.
2. Select `0x34e751c3`, then `0xf0f9bb23`. For each, copy **preview diagnostics** and take a screenshot. Record whether HDR is on or off alongside the copied text; REST cannot infer the scene texture's encoding from its format.
3. Repeat in SDR with the same settings. Keep **Apply tone map clamping** and **Flip render target** off for this comparison. First use **Clear alpha channel** on, as in the original screenshots; then compare it off. Off uses a direct resource copy, so alpha can legitimately make the displayed preview transparent. That alone is not proof of a copy bug.
4. In HDR, compare **Auto scene colour** on with manual target index 0, **BEFORE DRAW**. If the copied report lists other non-null RTV slots, try those in manual mode. Compare **AFTER DRAW** and **ON RENDER TARGET CHANGE** invocation settings where available, recording the actual capture timing/status. These defer capture to a later pipeline/target bind, can show later image contents or be unavailable, and are diagnostic comparisons rather than guaranteed valid injection points.
5. If those targets contain only UI, hunt for a different scene pass in HDR rather than relying on SDR hashes. Share copied reports from the original hashes and any candidate that contains the scene.

Start with the first two hashes in native HDR and SDR; the additional comparisons are useful only if needed. Once a scene target works with techniques disabled, enable SOLARIS separately and report whether its effect appears at the intended point.

No GPU frame capture is required for the initial report. A D3D12 frame capture, if available, could subsequently show whether the source itself is UI-only or whether REST's copy is incorrect. Logs already attached to issue #22 do not need to be resent unless a new failure occurs.

## Restore stable

Close the game and restore the backed-up stable add-on. Restore the INI too if you saved experimental target/invocation settings. This diagnostic build does not change the configuration format.

## Validation limits

Build/CPU checks do not establish Warframe HDR correctness. Native HDR target selection, preview pixels, effect placement and RenoDX combinations still require runtime testing. Neither a universal HDR fix nor automatic compute-output injection is claimed.
