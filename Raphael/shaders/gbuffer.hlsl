
// rootparam [0] - cbPerObject
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gTextureTransform;
};

// rootparam [1] - cbMaterial
cbuffer cbMaterial : register(b1)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
};

// rootparam [2] - cbPerFrame
cbuffer cbPerFrame : register(b2)
{
    float4x4 gView;
    float4x4 gProj;
    float4x4 gViewProj;

    float3 gEyePosW; 
    float pad1;

    float4 gAmbientLight;

    // Geomtry pass doesn't use lights array
    // The lighting pass will 
};

// Texture and sampler bout at rootparam [3]
Texture2D gDiffuseMap : register(t0);
SamplerState gSampler : register(s2); // linearwrap is at slot 2

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
	float4 PosH : SV_POSITION; // Clip space position for rasterizer
	float3 PosW : POSITION; // World space position for depth calculations and lighting
	float3 Normal : NORMAL; // World space normal 
	float2 TexC : TEXCOORD0; // Texture coordinates 
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    // Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;

    // Assume uniform scale for normals. Otherwise, use the inverse-transpose of the world matrix.
    vout.Normal = mul(float4(vin.NormalL, 0.0f), gWorld).xyz;

    // Transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);

	// Pass through texture coordinates, applying texture transform.
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gTextureTransform).xy ;

    return vout;
}

struct GBufferOutput
{
    float4 Albedo : SV_Target0;  // RGBA8 - diffuse color
    float4 Normal : SV_Target1;  // RGBA16F - world normal (xyz) + roughness (w)
    float  Depth : SV_Target2;  // R32F  - linear depth from camera
};

GBufferOutput PS(VertexOut pin)
{
    GBufferOutput output;

    // Sample the diffuse texture.
    float4 diffuseSample = gDiffuseMap.Sample(gSampler, pin.TexC);
    // Store the albedo (diffuse color) in the first render target.
    output.Albedo = diffuseSample * gDiffuseAlbedo;

    // Normalize the interpolated normal and store in the second render target
    // Store roughness in the alpha channel of the second render target.
    output.Normal.xyz = normalize(pin.Normal);
    output.Normal.w = gRoughness;

    // Store linear depth in the third render target.
    output.Depth = length(pin.PosW - gEyePosW);

    return output;
}
