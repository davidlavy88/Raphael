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

cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseAlbedo;
	float gMetallic;
	float gRoughness;
	float2 uvTile; // For scaling texture coordinates
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
	float3 PosW : POSITION;
	float3 Normal : NORMAL;
	float2 TexC : TEXCOORD0;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    // Transform from local space -> world space -> clip space
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosH = mul(posW, gViewProj);
	vout.PosW = posW.xyz;

	// Assume uniform scale for normals. Otherwise, use the inverse-transpose of the world matrix.
	vout.Normal = mul(vin.NormalL, (float3x3)gWorld);

	// Pass through texture coordinates. 
	vout.TexC = vin.TexC;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    // Sample the diffuse texture and modulate by material albedo.
	float2 tiledTexC = pin.TexC * uvTile; // Apply tiling to texture coordinates
    float4 texColor = gDiffuseMap.Sample(gSampler, tiledTexC);
    float4 diffuseAlbedo = texColor * gDiffuseAlbedo;

    // Normalize interpolated normal.
    float3 normal = normalize(pin.Normal);

    // Simple directional light.
    float3 lightDir = normalize(float3(0.5f, 1.0f, 0.5f));
    float NdotL = saturate(dot(normal, lightDir));

    // View direction for specular.
    float3 viewDir = normalize(gEyePosW - pin.PosW);

    // Fresnel-Schlick approximation (F0 based on metallic).
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), diffuseAlbedo.rgb, gMetallic);

    // Half vector for specular.
    float3 halfVec = normalize(lightDir + viewDir);
    float NdotH = saturate(dot(normal, halfVec));

    // Roughness-based specular power (GGX-like approximation).
    float roughness = max(gRoughness, 0.01f);
    float specPower = 2.0f / (roughness * roughness) - 2.0f;
    float spec = pow(NdotH, specPower) * ((specPower + 2.0f) / 8.0f);

    // Combine diffuse and specular.
    float3 diffuse = diffuseAlbedo.rgb * (1.0f - gMetallic);
    float3 specular = F0 * spec;

    // Ambient term.
    float3 ambient = diffuseAlbedo.rgb * 0.15f;

    float3 finalColor = ambient + (diffuse + specular) * NdotL;

    return float4(finalColor, diffuseAlbedo.a);
}
