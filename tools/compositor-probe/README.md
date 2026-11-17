# Captured compositor submission replay research

This standalone native D3D12 tool is carried on Legacy for the shared investigation. It is not linked into the Legacy add-on and does not enable Modern scene-input processing on ReShade 5.x.

Use externally exported TAA/no-TAA/FSR capture inputs and validated shader variants from the [Modern capture preparation guide](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/blob/feature/scene-input-and-backend-support/tools/compositor-probe/README.md). No captured game data or generated shaders are included in the repository.

Run `test-submission-replay.ps1 -CaptureRoot <capture-input-root> -ShaderRoot <generated-test-shader-directory> -OutputDirectory <independent-output>`. Windows x64, Visual C++ tools, a D3D12 device/debug layer and the external reference assets are required. Outputs must be separate from captured inputs.

The tool first establishes isolated original/identity/tint references in its output directory. It then retains 32 native recordings sharing one FP32 scene/processed/R32-alpha scratch set and final composed output, runs three reverse/rotated submissions through the restricted single-queue research bridge, and checks every output bit. The identity must match the original saved capture; the tint must match the prior validated split. Local TAA/no-TAA/FSR captures at 3824×1600 all passed with no D3D12 debug errors. Per-recording readbacks/descriptors still scale; this is not a constant-total-memory claim.

The queue/batch integration and complete REST/ReShade renderer remain unproven. This is not a public preview or a live Warframe fix. See [prototype status and limits](../../docs/scene-submission-prototype.md).
