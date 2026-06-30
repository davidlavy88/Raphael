#pragma once
#include <cstdint>
#include <vector>
#include "DirectXMath.h"

namespace raphael
{
    struct Vertex
    {
        Vertex() {}
        Vertex(
            const DirectX::XMFLOAT3& p,
            const DirectX::XMFLOAT3& n,
            const DirectX::XMFLOAT3& t,
            const DirectX::XMFLOAT2& uv) :
            Position(p),
            Normal(n),
            TangentU(t),
            TexC(uv) {
        }
        Vertex(
            float px, float py, float pz,
            float nx, float ny, float nz,
            float tx, float ty, float tz,
            float u, float v) :
            Position(px, py, pz),
            Normal(nx, ny, nz),
            TangentU(tx, ty, tz),
            TexC(u, v) {
        }

        DirectX::XMFLOAT3 Position;
        DirectX::XMFLOAT3 Normal;
        DirectX::XMFLOAT3 TangentU;
        DirectX::XMFLOAT2 TexC;
    };

    struct PrimitiveVertex
    {
        PrimitiveVertex() {}
        PrimitiveVertex(
            const DirectX::XMFLOAT3& p,
            const DirectX::XMFLOAT4& c) :
            Position(p),
            Color(c) {
        }
        PrimitiveVertex(
            float px, float py, float pz,
            float r, float g, float b, float a) :
            Position(px, py, pz),
            Color(r, g, b, a) {
        }

        DirectX::XMFLOAT3 Position;
        DirectX::XMFLOAT4 Color;
    };
}

