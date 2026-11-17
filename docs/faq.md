# Frequently asked questions

## Is REST a shader pack?

REST is a ReShade add-on that controls shader matching, effect placement and UI draw suppression. You supply suitable ReShade effects; the included REST shaders are helpers used by the add-on.

## Which ZIP should I download?

Choose Modern for ReShade 6.8+, or Legacy for the supported ReShade 5.x line. Future packages include the line and version in the archive name. Older releases use `release.zip`. Choose the release asset, not GitHub's source-code download. [Installation](getting-started.md).

## Why does my effect still blur the UI?

Check successful renders and technique mode. In exclusion mode, ticking an effect excludes it from REST's injection; it may still render normally at the end of the frame. A selected boundary may also occur too late. [Effect placement](effect-placement.md).

## Does Hide marked shaders create an effect mask?

It suppresses matching graphics draws. Keeping visible UI out of an effect instead requires a suitable earlier injection boundary. REST does not automatically generate a universal per-pixel mask. [UI control](ui-control.md).

## Why does Vulkan keep waiting?

The matched image needs suitable copy usage and a legal later boundary. The final UI pass may be too late. Optional scene transfers can help eligible FP16 images, but do not bypass boundary requirements. [Auto Scene Colour](automatic-scene-colour.md#vulkan-boundaries).

## Must Vulkan scene transfers be a launcher variable?

No. Current stable versions save it under Options > Advanced Vulkan options. Save and restart. Remove the old variable from the launcher so it does not override the preference. [Setting guide](vulkan-scene-transfer-setting.md).

## Does the Enshrouded graphics-hook workaround belong in REST's UI?

No. ReShade reads `RESHADE_DISABLE_GRAPHICS_HOOK` during its own startup, before REST can control it. Keep that process-only workaround and `--keep-vulkan-layers` in the tested Enshrouded launcher. [Game guide](games/enshrouded.md).

## Does REST make effects HDR-compatible?

No. The effect must handle the encoding and precision at the chosen scene target. Tone-map clamping is not automatic PQ/scRGB conversion. [HDR guide](HDR.md).

## Will my config survive an upgrade?

Replace the binary and helpers while preserving your INI. Back it up first, particularly before installing example configs. Use Save changes for edits and group import when you want to retain unrelated groups. [Configuration](groups-and-configuration.md).
