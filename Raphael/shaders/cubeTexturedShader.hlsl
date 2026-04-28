Texture2D gDiffuseMap : register(t0);
SamplerState gSampler : register(s0);

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
};

cbuffer cbPerFrame : register(b1)
{
    float4x4 gViewProj;
    float3   gEyePosW;
    float    pad0;
};

struct VertexIn
{
    float3 PosL : POSITION;
	float3 NormalL : NORMAL;
	float2 TexC : TEXCOORD0;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
	float3 Normal : NORMAL;
	float2 TexC : TEXCOORD0;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    // Transform from local space -> world space -> clip space
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosH = mul(posW, gViewProj);

	// Assume uniform scale for normals. Otherwise, use the inverse-transpose of the world matrix.
	vout.Normal = mul(vin.NormalL, (float3x3)gWorld);

	// Pass through texture coordinates.
	vout.TexC = vin.TexC;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    // Sample the diffuse texture.
    float4 diffuseAlbedo = gDiffuseMap.Sample(gSampler, pin.TexC) * 1.0f;

    // Normalize interpolated normal.
    pin.Normal = normalize(pin.Normal);

    // Indirect light (ambient).
    float4 ambient = 1.0f * diffuseAlbedo;

	return ambient;
}
