// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

Texture2D gDiffuseMap : register(t0);
SamplerState gSampler : register(s0);

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gTextureTransform;
};

cbuffer cbMaterial : register(b1)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
};

cbuffer cbPerFrame : register(b2)
{
    float4x4 gView;
    float4x4 gProj;
    float4x4 gViewProj;
    float3 gEyePosW;
    float pad1;
    float4 gAmbientLight;

    Light gLights[16];
};

// L - Local space
// W - World space
// H - Homogeneous clip space
struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD0;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 Normal : NORMAL;
    float2 TexC : TEXCOORD0;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
    // Transform to homogeneous clip space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;

    // Assume uniform scale for normals. Otherwise, use the inverse-transpose of the world matrix.
    vout.Normal = mul(float4(vin.NormalL, 0.0f), gWorld).xyz;

    // Transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);

    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gTextureTransform);
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    // Sample the diffuse texture.
    float4 diffuseAlbedo = gDiffuseMap.Sample(gSampler, pin.TexC) * gDiffuseAlbedo;

    // Normalize interpolated normal.
    pin.Normal = normalize(pin.Normal);

    // Compute the vector from the surface to the eye.
    float3 toEye = normalize(gEyePosW - pin.PosW);

    // Indirect light (ambient).
    float4 ambient = gAmbientLight * diffuseAlbedo;

    const float shininess = 1.0f - gRoughness;
    Material mat = { diffuseAlbedo, gFresnelR0, shininess };
    float3 shadowFactor = 1.0f; // No shadows for now.
    float4 litColor = ambient + ComputeLighting(gLights, mat, pin.PosW, pin.Normal, toEye, shadowFactor);

    // Common convention to take alpha from diffuse texture.
    litColor.a = diffuseAlbedo.a;

    return litColor;
}
