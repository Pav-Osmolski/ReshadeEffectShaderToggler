Texture2D t0 : register(t0);
SamplerState s0 : register(s0);

// Preserve HDR range and alpha. No colour-space conversion is implicit here.
void main(float4 vpos : SV_POSITION, float2 uv : TEXCOORD0, out float4 col : SV_TARGET)
{
    col = t0.SampleLevel(s0, uv, 0);
}
