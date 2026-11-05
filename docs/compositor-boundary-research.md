# Warframe compositor boundary investigation

Developer research, 5 November 2026. Related: [reporter's Preview 3 result](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/issues/22#issuecomment-6660706352), [current experimental implementation](experimental-backend-support.md), [reproduction tools](../tools/compositor-probe/README.md).

## Outcome

A later hook exists **inside** the two captured compositor shaders, after the main/secondary scene blend and game grading, before UI composition. This is not a separate game draw that can be selected with another ordinary REST shader boundary. Native GPU replay confirms that an effect inserted there can survive full secondary-colour replacement while retaining the game's UI composition code.

This is a shader-hook feasibility experiment, **not a live REST implementation** or a new pre-release. It does not run arbitrary ReShade techniques or validate a two-pass intermediate texture, state restoration or resource retirement in the game.

## Replay checks

Used the existing no-TAA, TAA/no-sharpening and FSR/TAAU captures on the local D3D12 GPU through headless RenderDoc 1.46. Shader inputs are restricted by full SHA-256 identity. Microsoft DXC 1.9.2609 disassembles, assembles and validates the replacement variants. An unsigned replacement initially produced unchanged images even for a deliberate tint; those early runs were rejected. The accepted runs include both identity and positive-change checks.

| Check | TAA | No TAA | FSR/TAAU |
|---|---:|---:|---:|
| Unmodified assembly round trip | Bit-exact | Bit-exact | Bit-exact |
| Identity hook before UI | Bit-exact | Bit-exact | Bit-exact |
| Main-input tint at forced full secondary weight: maximum RGB difference | 0 | 1 code value | 1 code value |
| Later tint at full secondary weight | Still changes scene | Still changes scene | Still changes scene |
| Fully opaque UI interior pixels checked | 22,290 | 19,803 | 19,331 |
| Changed opaque UI interior pixels with later tint | 0 | 0 | 0 |

Differences are measured in the final 10-bit RGB output. The one-code-value residual in the main-input experiment is consistent with floating-point cancellation/rounding, rather than a visible retained tint. The forced secondary weight is a controlled shader test, not a fresh capture of live motion blur. The late tint changes more than six million pixels per saved frame.

Opaque interiors use a five-by-five all-opaque neighbourhood to avoid misleading comparisons at bilinear alpha edges. Translucent UI reveals the changed scene underneath, so its final pixel values are expected to change. These tests preserve UI composition; they do not promise that every final pixel underneath translucent UI remains identical.

The saved DLSS recording fell back to TAA during capture. The reporter separately confirmed live Preview 3 placement with DLSS; **the new hook has no live DLSS validation**. Raw PNG previews are not colour-managed HDR reference images.

## Rejected shortcut

A separate local GPU prototype reconstructed UI blending in a hand-written HLSL pass using the captured UI texture and UI brightness constant. Although average error was small, identity output differed by up to 14–15 ten-bit code values; tint comparisons contained substantially larger outliers at some pixels. Alpha remained unchanged. That reconstruction is not accepted as a colour-faithful replacement.

The preferred direction retains the original game's UI shader operations and sampling. It must also preserve the original post-grading scene before applying effects; copying an earlier float scene or independently processing both main and secondary inputs is not equivalent.

## Requirements for a production implementation

1. **Explicit opt-in profile and strict matching.** Identify the exact compositor bytecode and validated root/target contract. Unknown revisions must skip this path. Do not infer scene/UI roles or colour space from format, size or shader CRC alone.
2. **Separate scene and UI variants from the original shader.** The first variant computes the complete graded scene without UI. After ReShade processes that private result, the second variant uses it at the verified hook and retains the game's UI composition. Do not redistribute captured game shaders or a compiled replacement containing their code. Runtime transformation needs a reviewed compiler/validator dependency and support for the relevant architecture.
3. **Preserve target semantics.** Clone and validate the graphics pipeline, draw geometry, root bindings, viewports/scissors and target interpretation. Reject unsupported side effects, depth/stencil writes, multiple outputs, bundles, indirect draws and active render passes. A single-draw shader-hook experiment does not establish safe draw replay.
4. **Protect alpha and resource lifetime.** Preserve the game's required output alpha across arbitrary techniques. Keep private descriptors, textures and derived pipelines alive until all referencing command recordings and GPU submissions are retired. Reuse the existing bounded resource/fence approach only after extending its ownership contract to both passes.
5. **Handle effect readiness and state restoration.** Observe actual ReShade technique completion. If a permutation is loading or preparation fails, restore the original draw behaviour without displaying partial processing or applying selected effects over the final UI.
6. **Validate the real two-pass identity.** Compare original and split/composed outputs, including translucent UI edges, highlights, negative/out-of-gamut intermediate values, alpha, grading and UI brightness changes. An inserted identity multiply inside one shader being bit-exact does not certify an intermediate texture or its precision.
7. **Measure live coverage and cost.** Test motion blur, all reported AA/upscaler paths including native DLSS, resize/reload, long sessions, other draw-intercepting add-ons and GPU validation messages. Measure extra draws, memory and GPU time. Applying effects after grading aligns their placement with final-frame usage, but does not make SDR-only techniques universally HDR-correct.

No extra reporter capture is required to establish this hook. Existing captures suffice for the implementation prototype; live verification will be needed once that prototype is integrated. No stable-release readiness follows from this research alone.
