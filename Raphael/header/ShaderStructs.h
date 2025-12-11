#pragma once
#include <DirectXMath.h>

using namespace DirectX;

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
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
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
};