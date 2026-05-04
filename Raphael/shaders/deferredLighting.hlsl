#define MAX_LIGHTS 16
struct LightParameters
{
	float3  Color;       // All light
	float   Intensity;   // All light
	float3  Direction;   // Directional light
	float   pad0;        // Padding to 16 bytes
};

cbuffer LightCommonData
{
	float4 gAmbientLight;
	LightParameters gLights[MAX_LIGHTS];
};

// G-Buffer textures bound as SRVs
Texture2D gAlbedoMap : register(t0); // SV_Target0 from geometry pass
Texture2D gNormalMap : register(t1); // SV_Target1 (xyz=normal, w=roughness)
Texture2D gDepthMap : register(t2); // SV_Target2 (linear depth)

SamplerState gSamPoint : register(s0); // pointWrap at slot 0

// Full-screen triangle vertex output
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
	float4 albedo = gAlbedoMap.Sample(gSamPoint, pin.TexC);
	float4 normalData = gNormalMap.Sample(gSamPoint, pin.TexC);
	float  depth = gDepthMap.Sample(gSamPoint, pin.TexC).r;

	// Early out for empty pixels (nothing was rendered here)
	if (depth == 0.0f)
		discard;

	float3 normal = normalize(normalData.xyz);
	float  roughness = normalData.w;

	// Start with ambient lighting
	float3  lighting = albedo.rgb * gAmbientLight.rgb;

	// Add contribution from each light
	for (int i = 0; i < 3; i++)
	{
		LightParameters light = gLights[i];
		// Skip lights with zero intensity
		if (light.Intensity == 0.0f)
			continue;
		// Simple Lambertian diffuse lighting
		float NdotL = max(dot(normal, normalize(-light.Direction)), 0.0f);

		lighting += albedo.rgb * light.Color * NdotL;
	}

	return float4(lighting, albedo.a);
}

