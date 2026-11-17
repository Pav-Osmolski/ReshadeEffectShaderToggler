# Advanced targets and bindings

Start with [Automatic Scene Colour](automatic-scene-colour.md) for ordinary scene-effect placement on a supported backend. Manual destination, texture-binding and constant-binding controls are for configurations that need a specific game resource or effect input.

## Manual render targets and SRVs

With Auto disabled, a group can use its configured destination and target/SRV selection. The correct index, shader stage, slot or descriptor depends on the game's bindings at the matched draw. A resource that looks like scene colour may instead be a history surface, partial buffer or unrelated pass.

Auto retains these settings but ignores them while active. Turning Auto off does not make an unsafe target legal: manual Vulkan rendering skips active render passes and targets without the required transfer-source support. D3D12 also retains native-render-pass guards.

## Texture bindings

The **Texture bindings** tab can expose or copy a selected target/binding for effects that are designed to consume it. Configure the expected resource and required effect semantics together. Copying a texture is separate from selecting Auto's live scene target; an Auto test does not certify every manually copied binding path.

Topology, typed format, resource state and lifetime still matter. On Vulkan, copied bindings need their own safe-boundary validation. Do not infer universal support from a successful hunting preview.

## Constant bindings

The **Constant bindings** controls select the shader stage and constant/push binding to extract. These are advanced game/effect-specific inputs, not automatic depth or camera detection. Choose a copy/hook method appropriate to the supported game path and verify it with the consuming effect.

The FFXIV constant-copy hook is x64-only. Legacy's older SDK has additional Vulkan buffer-descriptor limitations and retains conservative fallbacks where offsets cannot be restored reliably.

## Validate incrementally

Use a separate inactive cloned group, enable only the required effect chain, and change one destination/binding at a time. Check preview/diagnostic reasons, compare a moving scene, then test resizing and group toggles before saving. Keep a known-good config.

For source-level behaviour and test limits, see [technical reference](reference.md). For missing effects or crashes, use [troubleshooting](troubleshooting.md).
