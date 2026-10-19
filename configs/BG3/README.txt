REST Enhanced - Baldur's Gate 3 Configuration Files
========================================================

These optional configuration files provide preconfigured REST Enhanced settings
for Baldur's Gate 3 using either Direct3D 11 or Vulkan.

CONTENTS
--------

D3D11\
  ReshadeEffectShaderToggler.ini

Vulkan\
  ReshadeEffectShaderToggler.ini


WHICH CONFIG SHOULD I USE?
--------------------------

Use the D3D11 configuration if you launch Baldur's Gate 3 using Direct3D 11
(bg3_dx11.exe).

Use the Vulkan configuration if you launch Baldur's Gate 3 using Vulkan.

Only use the configuration for the renderer you actually use.


INSTALLATION
------------

1. Make sure REST Enhanced and ReShade are already installed and working.

2. If you already have a ReshadeEffectShaderToggler.ini file, BACK IT UP FIRST.
   The supplied configuration will replace your existing REST configuration if
   you overwrite it.

3. Open either the D3D11 or Vulkan folder in this archive.

4. Copy:

   ReshadeEffectShaderToggler.ini

   to the same location as:

   ReshadeEffectShaderToggler.addon64

5. Allow Windows to replace the existing INI only if you have already backed up
   any REST configuration you want to keep.

6. Launch Baldur's Gate 3 and confirm REST Enhanced is loaded in the ReShade
   Add-ons tab.


IMPORTANT
---------

Shader hashes and suitable injection points can depend on the game version,
graphics API, graphics settings, resolution, upscaler and driver configuration.

These files are intended as useful starting configurations for the tested
Baldur's Gate 3 D3D11 and Vulkan paths. They are not guaranteed to behave
identically on every system.

If you already have your own REST groups, consider backing up your current INI
and importing or recreating only the settings you need rather than permanently
overwriting your existing configuration.


REST ENHANCED
-------------

GitHub:
https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler

ReShade forum:
https://reshade.me/forum/addons/10351-rest-enhanced---reshade-effect-shader-toggler-with-auto-scene-colour

Current enhanced development, documentation and testing by DeViLhoOD.
