// Temporary placement check for REST issue #22.
// Multiplication preserves HDR values without clamping or tone-map conversion.
// Select this technique in the REST group and keep its ReShade checkbox enabled.
#include "ReShade.fxh"

float4 RESTSceneBoundaryCheckPS(float4 position : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    float4 scene = tex2D(ReShade::BackBuffer, uv);
    return float4(scene.rgb * float3(1.0, 0.25, 0.25), scene.a);
}

technique REST_SceneBoundaryCheck
{
    pass
    {
        VertexShader = PostProcessVS;
        PixelShader = RESTSceneBoundaryCheckPS;
    }
}
