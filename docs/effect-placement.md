# Place effects before UI or fog

Use a REST group to apply selected ReShade techniques at a suitable point in the game's frame. Later game passes can then draw over those effects, keeping interface elements sharp or preserving later fog composition.

## Set up a group

1. Create a **New group**. Use **Edit** to name it and optionally assign a hotkey.
2. Open **Settings**, start hunting and mark a shader associated with the desired scene boundary. See [shader hunting](shader-hunting.md).
3. For D3D10/11/12 or Vulkan, start with **Auto scene colour**. It uses the primary live colour target and handles eligible resolution differences.
4. Choose the effect-selection mode below. Keep required techniques enabled in ReShade's Home tab and in their required global order.
5. Click **Done hunting**, activate the group and check **Successful renders** and **Last successful techniques**.
6. Compare Active off/on, move the camera and open the game UI. Save the working configuration.

## Understand the technique checkboxes

| Mode shown in REST | Meaning of a tick |
|---|---|
| Only ticked enabled techniques are applied | Include this technique in the group's injection |
| Ticked techniques are EXCLUDED | Exclude this technique from the group's injection |
| All globally enabled techniques are applied | Apply every globally enabled technique; no selection filter |

**Apply all enabled techniques** together with **Except for selected techniques** enables exclusion mode. Untick an effect you want REST to inject in that mode. Excluding an effect from a REST group does not switch it off in ReShade; it can still render at the ordinary end of the frame and affect the HUD.

An effect disabled globally in ReShade will not run just because it is selected in REST. If multiple active groups assign the same technique, the first applicable group normally renders it. Temporarily disable competing groups while diagnosing placement.

## Multi-pass effects

Include every required technique in a chain and keep the ReShade ordering correct. For example, the tested Lumenite AO setup used `Lumenite_Kernel → Lumenite_LSAO`. REST preserves global technique order; it does not infer shader-package dependencies.

## Choose the right boundary

For direct D3D workflows, REST dispatches at the matched shader boundary, subject to backend guards. Vulkan queues work from the matched draw and waits for a legal later boundary. Selecting the final HUD shader can therefore be too late even when it works for HUD hiding.

On Vulkan, prefer an earlier scene shader whose target has a compatible later continuation or transition before the UI is drawn. REST never splits the game's active render pass to force an injection. See [Vulkan boundary rules](automatic-scene-colour.md#vulkan-boundaries).

## Manual destinations

Use manual render-target/SRV configuration only when you understand the required target and Auto is unsuitable. Auto preserves the manual settings but ignores them while active. Turning Auto off does not remove Vulkan's safety restrictions. See [advanced bindings](advanced-bindings.md).

## Verify the result

A rising **Successful renders** counter establishes that REST has rendered techniques. It does not alone prove a useful scene boundary or correct HDR colour. Compare the visible result and technique list. If the UI still receives the effect, check the selection mode, other active groups and the chosen boundary before hunting again.
