struct CapturedVertex {
    float4 position : POSITION;
    float4 uv : TEXCOORD0;
    float4 scene : TEXCOORD1;
};

struct RasterVertex {
    float4 position : SV_Position;
    float4 uv : TEXCOORD0;
    float4 scene : TEXCOORD1;
};

RasterVertex ReplayVertex(CapturedVertex input) {
    RasterVertex output;
    output.position = input.position;
    output.uv = input.uv;
    output.scene = input.scene;
    return output;
}

Texture2D<float4> gradedScene : register(t0);

float4 TintScene(float4 position : SV_Position) : SV_Target {
    float4 scene = gradedScene.Load(int3(int2(position.xy), 0));
    return float4(scene.rgb * float3(1.0, 0.25, 0.25), scene.a);
}

float4 TintSceneOverwriteAlpha(float4 position : SV_Position) : SV_Target {
    return float4(TintScene(position).rgb, 0.0);
}
