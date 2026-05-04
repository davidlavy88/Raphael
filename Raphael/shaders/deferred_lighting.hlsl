#define MaxLights 16

struct Light
{
    float3 Color;       // Color
    float FalloffStart; // point/spot light only
    float3 Direction;   // directional/spot light only
    float FalloffEnd;   // point/spot light only
    float3 Position;    // point light only
    float SpotPower;    // SpotLightIntensity (spot light only)
};

// Pass constants - same layout as the geometry pass
cbuffer cbPerPass : register(b0)
{
    float4x4 gView;
    float4x4 gProj;
    float4x4 gViewProj;

    float3 gEyePosW;
    float  pad1;

    float4 gAmbientLight;

    Light gLights[16];
};

// GBuffer textures as SRV
Texture2D gAlbedoMap : register(t0); // SV_Target0 from geometry pass
Texture2D gNormalMap : register(t1); // SV_Target1 from geometry pass
Texture2D gDepthMap : register(t2); // Depth buffer from geometry pass

SamplerState gSampler : register(s0); // linearwrap is at slot 0

// Full screen triangle vertex output
struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

// Generate a full-screen triangle from vertex ID (no vertex buffer needed)
// IDs 0,1,2 produce a triangle that covers the entire screen:
//
//       (-1, 3)
//         ID1
//          *
//          | \
//          |  \
//          |   \
//          |    \
// (-1, 1)  + --- + (1, 1)
//          |     |\
//          |     | \        <- The triangle extends beyond the screen
//          |     |  \
// (-1, -1) + --- + --*(3, -1) ID2
// ID0
VertexOut VS(uint vertexID : SV_VertexID)
{
    VertexOut vout;

    // Generate clip-space position for a full-screen triangle
    float2 uvs[3] = { float2(0, 0), float2(2, 0), float2(0, 2) };
    vout.TexC = uvs[vertexID];
    vout.PosH = float4(vout.TexC * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    // Sample G-Buffer data at this pixel
    float4 albedo = gAlbedoMap.Sample(gSampler, pin.TexC);
    float4 normalData = gNormalMap.Sample(gSampler, pin.TexC);
    float  depth = gDepthMap.Sample(gSampler, pin.TexC).r;

    // Early out for empty pixels (nothing was rendered here)
    if (depth == 0.0f)
        discard;

    float3 normal = normalize(normalData.xyz);
    float  roughness = normalData.w;

    // Start with ambient lighting
    float3 litColor = albedo.rgb * gAmbientLight.rgb;

    // Compute directional light contributions (3 lights)
    for (int i = 0; i < 3; ++i)
    {
        float3 lightDir = normalize(-gLights[i].Direction);
        float  NdotL = max(dot(normal, lightDir), 0.0f);

        // Diffuse contribution
        litColor += albedo.rgb * gLights[i].Color * NdotL;
    }

    return float4(litColor, albedo.a);
}
