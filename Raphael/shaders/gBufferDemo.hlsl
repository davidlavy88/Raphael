Texture2D gDiffuseMap : register(t0);
SamplerState gSampler : register(s2);

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
    float4 diffuseAlbedo = gDiffuseMap.Sample(gSampler, pin.TexC) * 1.0f;

    // Normalize interpolated normal.
    pin.Normal = normalize(pin.Normal);

    // Indirect light (ambient).
    float4 ambient = 1.0f * diffuseAlbedo;

	/*output.Albedo = diffuseAlbedo;
	output.Normal = float4(pin.Normal, 1.0f);
	output.Depth = pin.PosH.z / pin.PosH.w;*/

    // Visualize normals: remap from [-1,1] to [0,1] for display
    /*output.Albedo = float4(pin.Normal * 0.5f + 0.5f, 1.0f);
    output.Normal = float4(pin.Normal, 1.0f);
    output.Depth = pin.PosH.z / pin.PosH.w;*/

    // Visualize depth as grayscale
    float depth = pin.PosH.z / pin.PosH.w;
    output.Albedo = float4(depth, depth, depth, 1.0f);
    output.Normal = float4(pin.Normal, 1.0f);
    output.Depth = depth;

	return output;
}
