# UI hiding and masking

REST offers two distinct ways to keep a game interface out of your screenshots or effects.

| Goal | Workflow |
|---|---|
| Remove interface elements from the image | Hunt the UI shaders and enable **Hide marked shaders** |
| Keep the interface visible but unaffected by an effect | Inject the effect at a suitable earlier scene boundary |

## Hide the HUD

Create a group for the interface, hunt the relevant pixel/vertex shaders and mark the draws that remove the intended elements. Enable **Hide marked shaders**, finish hunting and test the group's Active checkbox. Assign a group hotkey for a convenient photomode toggle, then save.

Games often draw text, icons, prompts and minimaps with different shaders, so one hash may not hide everything. Recheck the result in menus, gameplay and cutscenes. A shader shared by world rendering and UI may hide more than expected.

REST suppresses matching graphics draws. Compute dispatches are deliberately not hidden because they may drive lighting, simulation or upscaling.

## Keep the UI sharp

For depth of field, perspective correction or other scene effects, leave **Hide marked shaders** off and use [effect placement](effect-placement.md). Find a boundary before later UI composition and include the intended techniques.

On Vulkan, a final HUD shader can be too late: REST needs a compatible target and a safe later boundary. The tested [Enshrouded setup](games/enshrouded.md) uses an earlier scene shader instead of the HUD shaders.

## What “masking” means here

REST can avoid processing UI by choosing where an effect runs. It does not automatically identify every UI pixel or produce a universal per-pixel mask for all games and effects. Advanced copied texture/buffer bindings are separate, manually configured workflows.

## Keep both workflows

Use separate groups for HUD hiding and scene effects. Clone a working group to experiment without losing its hashes. Clones and imported groups start inactive with no hotkey. Keep competing effect groups disabled while testing, and save the intended Active states.
