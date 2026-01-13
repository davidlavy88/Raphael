#pragma once
#include <DirectXMath.h>

using namespace DirectX;

#define MAX_LIGHTS 16

static XMFLOAT4X4 XM4x4Identity()
{
    static XMFLOAT4X4 I(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);

    return I;
}

struct VertexShaderInput
{
	VertexShaderInput() {}
	VertexShaderInput(const XMFLOAT3& pos, const XMFLOAT3& normal)
		: Pos(pos)
		, Normal(normal)
	{}

	XMFLOAT3 Pos;
	XMFLOAT3 Normal;
};

struct LightConstants
{
	XMFLOAT3 Color = { 1.0f, 1.0f, 1.0f };		// Light color
	float FalloffStart = 1.0f;                  // point/spot light only
	XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f};	// directional/spot light only
	float FalloffEnd = 10.0f;                   // point/spot light only
	XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f};	// point/spot light only
	float SpotLightIntensity = 64.0f;           // spot light only
};

struct ObjectConstants
{
	XMFLOAT4X4 World = XM4x4Identity();
};

struct PassConstants
{
	XMFLOAT4X4 View = XM4x4Identity();
	XMFLOAT4X4 Proj = XM4x4Identity();
	XMFLOAT4X4 ViewProj = XM4x4Identity();

	XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float PassPad1 = 0.0f;

	XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

	LightConstants Lights[MAX_LIGHTS];
};

struct MaterialConstants
{
	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f};
	float Roughness = 0.25f;
};
