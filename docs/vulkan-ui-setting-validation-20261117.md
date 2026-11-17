# Saved Vulkan scene-transfer setting validation — 17 November 2026

Tested development commits: Modern `961586715988a3e67d566ffd9d1c461463bb015d`, Legacy `ec86e3e2484217e7fb88654dcc570d95ae14c667`. Hosted runs 44079117837/44079125688 passed x64/x86 production builds, rendering/lifecycle/hunting tests, new configuration-policy regressions and binary/package checks. Release preparation changes version stamps and documentation only; no rendering or preference logic changed after these tests.

The maintainer confirmed testing both Modern and Legacy successfully before authorizing release. Exact individual operations and Legacy host version were not separately recorded.

Detailed Modern Enshrouded inspection used the normal game launcher with `RESHADE_DISABLE_GRAPHICS_HOOK=1` and `--keep-vulkan-layers`. Installed addon64 hash `a04f2a0d2de2c01c9601485197c878ac2ee66c2f2bcee3462d1f28d08edfff1a` matched the test artifact. Startup log recorded configured=true, active=true, launcher override=false and eligible FP16 transfer additions. The UI showed Active this launch: Enabled. The saved setting was working; the original group still selected three late HUD shaders and excluded CinematicDOF.

Preserving that original group, a copy was configured with the earlier scene PS `0x46c3d744` and CinematicDOF removed from the exception list. Direct 3840x2160 -> 3840x2160 injection succeeded with seven techniques including CinematicDOF; the counter increased from 39 to 8732. Group off blurred menu text; on restored sharp text over the blurred scene. The corrected copy and enabled preference were saved through REST. A colour preview of the selected FP16 scene was captured at a safe boundary during hunting.

That detailed check used the title-screen scene, not a fresh loaded-world HDR or exit matrix. Prior v1.7.5/v1.6.9 coverage remains documented separately. No universal allocation-safety, HDR-colour or Warframe-resolution claim is made.
