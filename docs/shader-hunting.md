# Find and mark shaders

Shader hunting connects a visible game element or scene pass to the shader hashes REST can match. Hashes vary with game versions, graphics API and settings; a shared configuration is a starting point rather than a universal preset.

## Collect, browse and mark

1. Keep the relevant scene or HUD element visible. Open a group's **Settings** and click **Start shader hunting**.
2. REST collects active shaders for the configured frame count. Increase that count for elements that appear intermittently.
3. Start with pixel shaders. Browse with **Prev / Next**, or search a known hash. The **All / Marked / Unmarked** filter narrows the list.
4. Use **Mark / unmark**, **Mark + Prev** or **Mark + Next**. Marked-navigation buttons revisit your choices. Navigation buttons support held repeat.
5. Use **Rescan** when the scene changes. It recollects shader stages while preserving existing marks.
6. Click **Done hunting** to commit pending marks to this group, then test and **Save changes**.

Marks edited during hunting are pending until Done. Finishing hunting is separate from saving the INI. A saved hash shown as not seen was not observed in the current collection; it has not necessarily become invalid forever.

## What to look for

For **HUD hiding**, find shaders responsible for the interface draws. Several hashes may cover icons, text, minimaps and prompts.

For **effect placement**, find a suitable scene boundary and colour target. An interface shader that can be hidden is not automatically a safe effect-injection point. This distinction is especially important on Vulkan.

## Previews

The preview identifies the selected shader/stage, target dimensions and format. RGB/R/G/B controls help inspect channels. It is a diagnostic texture view, not a colour-managed HDR reference.

Vulkan captures at a safe render-pass boundary. It can show a waiting or unsupported reason rather than an image; it does not perform flip, alpha-clearing or tone-map preview processing there. Other previews also require a single-sample, single-layer, single-mip 2D target. A preview of a target does not guarantee that Auto injection has a suitable later boundary.

## Default shortcuts

| Action | Pixel shaders | Vertex shaders |
|---|---|---|
| Previous / next | Numpad 1 / 2 | Numpad 4 / 5 |
| Mark / unmark | Numpad 3 | Numpad 6 |
| Previous / next marked | Ctrl + Numpad 1 / 2 | Ctrl + Numpad 4 / 5 |

All hunting shortcuts are configurable under **Shader hunting keybindings**. Compute hunting and Mark + Previous/Next have configurable bindings without defaults. Exact modifiers matter: a plain shortcut does not also fire when a modified version is pressed.

Search text, filters, selected stage, preview channel and hunting-pane width are retained during the current REST session. They are not a promise of persistent cross-launch UI state.

## If a selection crashes

Preserve `ReShade.log` before restarting the game. REST logs the selected stage/hash before making the selection active, so a screenshot is not required to recover that context. Include the destination mode and exact action in a [crash report](troubleshooting.md#crashes-and-issue-reports).
