#pragma once
#include <cstdint>
#include <vector>
#include "DirectXMath.h"

using namespace DirectX;

namespace raphael
{
    struct Vertex
    {
        Vertex() {}
        Vertex(
            const XMFLOAT3& p,
            const XMFLOAT3& n,
            const XMFLOAT3& t,
            const XMFLOAT2& uv) :
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

        XMFLOAT3 Position;
        XMFLOAT3 Normal;
        XMFLOAT3 TangentU;
        XMFLOAT2 TexC;
    };

    struct PrimitiveVertex
    {
        PrimitiveVertex() {}
        PrimitiveVertex(
            const XMFLOAT3& p,
            const XMFLOAT4& c) :
            Position(p),
            Color(c) {
        }
        PrimitiveVertex(
            float px, float py, float pz,
            float r, float g, float b, float a) :
            Position(px, py, pz),
            Color(r, g, b, a) {
        }

        XMFLOAT3 Position;
        XMFLOAT4 Color;
    };
}

