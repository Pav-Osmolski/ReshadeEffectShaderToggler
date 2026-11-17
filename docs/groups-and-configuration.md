# Groups, saving and configuration

A REST group stores shader hashes, effect-selection rules, target/binding settings, a name, an optional hotkey and its Active state. The configuration is game-local: `ReshadeEffectShaderToggler.ini` sits beside the REST add-on.

## Group controls

| Control | What it does |
|---|---|
| New group | Create a group to configure |
| Edit | Change its name and optional hotkey |
| Settings | Open hunting, effects and advanced target/binding controls |
| Active | Enable the group's behaviour; this state is saved |
| Clone | Copy hashes/effects/settings into a new inactive group with no hotkey |
| Copy group / Import group | Share one self-contained group without replacing the whole configuration |
| Delete | Remove a group after confirmation; save to persist the change |

Imported groups also start inactive with no hotkey. Check technique names, target assumptions and game/API compatibility before activating them. REST warns about conflicting group/REST shortcuts.

## Saving is explicit

Use **Save changes** when REST shows **Unsaved changes**. Done hunting commits the current marks to the group; it does not by itself write the INI. Closing the overlay or game is not a substitute for saving.

REST writes a verified temporary file, refreshes `ReshadeEffectShaderToggler.ini.bak` from the previous configuration, then atomically replaces the main INI. This backup does not protect a file overwritten manually by another installer or configuration download.

## General preferences

Collection frame count, overlay opacity, descriptor tracking, hunting shortcuts and other general options are saved alongside groups. **Vulkan scene transfers** is a per-config startup preference: save and fully restart the game to apply it. The UI separates its saved checkbox from **Active this launch**. [Read the restart and override rules](vulkan-scene-transfer-setting.md).

Auto Scene Colour preserves manual target/SRV and alpha-preservation settings, but ignores them while Auto is active. They take effect again when Auto is disabled, subject to backend safety guards.

## Upgrades and shared configs

Keep a separate INI backup before upgrading, changing API or trying shared configurations. Preserve your existing INI when replacing a binary. Matching game versions and renderer/settings matter because hashes can change.

The optional `BG3-Configs-D3D11+Vulkan.zip` provides separate renderer configs. Copying an entire INI replaces the current configuration; use group import to retain unrelated groups. See the [BG3 guide](games/baldurs-gate-3.md).

## Recover a known-good setup

Close the game, preserve the current INI for diagnosis, then copy a known-good backup to `ReshadeEffectShaderToggler.ini`. Restart and check group states, effect modes and hotkeys. If saving fails, check folder write access and whether the loaded add-on/config path matches the one you edited.
