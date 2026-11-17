# Validation records

These records preserve what was tested, with versions, commits and limits. Use current user guides for setup; historical candidate language describes the work at the time.

| Record | Scope |
|---|---|
| [Saved Vulkan UI setting, 17 November 2026](vulkan-ui-setting-validation-20261117.md) | Modern/Legacy confirmation; detailed startup and menu-scene injection check |
| [Vulkan safety and scene matching, 17 November 2026](release-candidate-validation-20261117.md) | Enshrouded SDR/HDR, scale/mode changes, BG3 D3D11/Vulkan reports |
| [Maintenance validation](maintenance-release-validation.md) | D3D9 native reset/state, RE4 hunting crash regression and lifecycle work |
| [Vulkan allocation safety](vulkan-transfer-safety.md) | What the generic descriptor can establish and why optional transfers remain off by default |

## Read results at the right scope

- Automated x86/x64 builds verify compilation and metadata, not injected game behaviour.
- CPU tests verify targeted bookkeeping and decisions, not GPU execution.
- Native D3D9 API-host tests exercise real COM resources/reset behaviour, not the entire injected game runtime.
- A maintainer report establishes the configurations reported, not every operation in a broader matrix.
- Sharp UI with a scene effect establishes placement in that scene; it is not a measurement of HDR colour accuracy.

BG3 D3D11 and Vulkan, Modern and Legacy, have separate reported passes. Enshrouded tests used the documented launch setup and an earlier scene shader. RE4's detailed x86 game result belongs to Modern. Warframe HDR work remains separate.

Historical [release notes](releases/README.md) remain available without being promoted over current setup guidance in site search.
