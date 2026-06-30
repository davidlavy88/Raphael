#include "MeshGenerator.h"

namespace raphael
{
    using namespace DirectX;

    Mesh MeshGenerator::CreateBox(float width, float height, float depth, uint32 numSubdivisions)
    {
        Mesh mesh;

        Vertex v[24];

        float w2 = 0.5f * width;
        float h2 = 0.5f * height;
        float d2 = 0.5f * depth;

        // Fill in the front face vertex data.
        v[0] = Vertex(-w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
        v[1] = Vertex(-w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        v[2] = Vertex(+w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
        v[3] = Vertex(+w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

        // Fill in the back face vertex data.
        v[4] = Vertex(-w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
        v[5] = Vertex(+w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
        v[6] = Vertex(+w2, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        v[7] = Vertex(-w2, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

        // Fill in the top face vertex data.
        v[8] = Vertex(-w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
        v[9] = Vertex(-w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        v[10] = Vertex(+w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
        v[11] = Vertex(+w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

        // Fill in the bottom face vertex data.
        v[12] = Vertex(-w2, -h2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
        v[13] = Vertex(+w2, -h2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
        v[14] = Vertex(+w2, -h2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        v[15] = Vertex(-w2, -h2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

        // Fill in the left face vertex data.
        v[16] = Vertex(-w2, -h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
        v[17] = Vertex(-w2, +h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
        v[18] = Vertex(-w2, +h2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f);
        v[19] = Vertex(-w2, -h2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f);

        // Fill in the right face vertex data.
        v[20] = Vertex(+w2, -h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
        v[21] = Vertex(+w2, +h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
        v[22] = Vertex(+w2, +h2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
        v[23] = Vertex(+w2, -h2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

        mesh.m_vertices.assign(&v[0], &v[24]);

        uint32 i[36];

        // Fill in the front face index data
        i[0] = 0; i[1] = 1; i[2] = 2;
        i[3] = 0; i[4] = 2; i[5] = 3;

        // Fill in the back face index data
        i[6] = 4; i[7] = 5; i[8] = 6;
        i[9] = 4; i[10] = 6; i[11] = 7;

        // Fill in the top face index data
        i[12] = 8; i[13] = 9; i[14] = 10;
        i[15] = 8; i[16] = 10; i[17] = 11;

        // Fill in the bottom face index data
        i[18] = 12; i[19] = 13; i[20] = 14;
        i[21] = 12; i[22] = 14; i[23] = 15;

        // Fill in the left face index data
        i[24] = 16; i[25] = 17; i[26] = 18;
        i[27] = 16; i[28] = 18; i[29] = 19;

        // Fill in the right face index data
        i[30] = 20; i[31] = 21; i[32] = 22;
        i[33] = 20; i[34] = 22; i[35] = 23;

        mesh.m_indices32.assign(&i[0], &i[36]);

        // Put a cap on the number of subdivisions.
        numSubdivisions = std::min<uint32>(numSubdivisions, 6u);

        for (uint32 j = 0; j < numSubdivisions; ++j)
            Subdivide(mesh);

        return mesh;
    }

    Mesh MeshGenerator::CreateSphere(float radius, uint32 sliceCount, uint32 stackCount)
    {
        Mesh mesh;

        Vertex topVertex(0.0f, +radius, 0.0f, 0.0f, +1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        Vertex bottomVertex(0.0f, -radius, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

        mesh.m_vertices.push_back(topVertex);

        float phiStep = XM_PI / stackCount;
        float thetaStep = 2.0f * XM_PI / sliceCount;

        for (uint32 i = 1; i <= stackCount - 1; ++i)
        {
            float phi = i * phiStep;

            for (uint32 j = 0; j <= sliceCount; ++j)
            {
                float theta = j * thetaStep;

                Vertex v;

                v.Position.x = radius * sinf(phi) * cosf(theta);
                v.Position.y = radius * cosf(phi);
                v.Position.z = radius * sinf(phi) * sinf(theta);

                v.TangentU.x = -radius * sinf(phi) * sinf(theta);
                v.TangentU.y = 0.0f;
                v.TangentU.z = +radius * sinf(phi) * cosf(theta);

                XMVECTOR T = XMLoadFloat3(&v.TangentU);
                XMStoreFloat3(&v.TangentU, XMVector3Normalize(T));

                XMVECTOR p = XMLoadFloat3(&v.Position);
                XMStoreFloat3(&v.Normal, XMVector3Normalize(p));

                v.TexC.x = theta / XM_2PI;
                v.TexC.y = phi / XM_PI;

                mesh.m_vertices.push_back(v);
            }
        }

        mesh.m_vertices.push_back(bottomVertex);

        for (uint32 i = 1; i <= sliceCount; ++i)
        {
            mesh.m_indices32.push_back(0);
            mesh.m_indices32.push_back(i + 1);
            mesh.m_indices32.push_back(i);
        }

        uint32 baseIndex = 1;
        uint32 ringVertexCount = sliceCount + 1;
        for (uint32 i = 0; i < stackCount - 2; ++i)
        {
            for (uint32 j = 0; j < sliceCount; ++j)
            {
                mesh.m_indices32.push_back(baseIndex + i * ringVertexCount + j);
                mesh.m_indices32.push_back(baseIndex + i * ringVertexCount + j + 1);
                mesh.m_indices32.push_back(baseIndex + (i + 1) * ringVertexCount + j);

                mesh.m_indices32.push_back(baseIndex + (i + 1) * ringVertexCount + j);
                mesh.m_indices32.push_back(baseIndex + i * ringVertexCount + j + 1);
                mesh.m_indices32.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
            }
        }

        uint32 southPoleIndex = (uint32)mesh.m_vertices.size() - 1;

        baseIndex = southPoleIndex - ringVertexCount;

        for (uint32 i = 0; i < sliceCount; ++i)
        {
            mesh.m_indices32.push_back(southPoleIndex);
            mesh.m_indices32.push_back(baseIndex + i);
            mesh.m_indices32.push_back(baseIndex + i + 1);
        }

        return mesh;
    }

    Mesh MeshGenerator::CreateGeosphere(float radius, uint32 numSubdivisions)
    {
        Mesh mesh;

        numSubdivisions = std::min<uint32>(numSubdivisions, 6u);

        const float X = 0.525731f;
        const float Z = 0.850651f;

        XMFLOAT3 pos[12] =
        {
            XMFLOAT3(-X, 0.0f, Z),  XMFLOAT3(X, 0.0f, Z),
            XMFLOAT3(-X, 0.0f, -Z), XMFLOAT3(X, 0.0f, -Z),
            XMFLOAT3(0.0f, Z, X),   XMFLOAT3(0.0f, Z, -X),
            XMFLOAT3(0.0f, -Z, X),  XMFLOAT3(0.0f, -Z, -X),
            XMFLOAT3(Z, X, 0.0f),   XMFLOAT3(-Z, X, 0.0f),
            XMFLOAT3(Z, -X, 0.0f),  XMFLOAT3(-Z, -X, 0.0f)
        };

        uint32 k[60] =
        {
            1,4,0,  4,9,0,  4,5,9,  8,5,4,  1,8,4,
            1,10,8, 10,3,8, 8,3,5,  3,2,5,  3,7,2,
            3,10,7, 10,6,7, 6,11,7, 6,0,11, 6,1,0,
            10,1,6, 11,0,9, 2,11,9, 5,2,9,  11,2,7
        };

        mesh.m_vertices.resize(12);
        mesh.m_indices32.assign(&k[0], &k[60]);

        for (uint32 i = 0; i < 12; ++i)
            mesh.m_vertices[i].Position = pos[i];

        for (uint32 i = 0; i < numSubdivisions; ++i)
            Subdivide(mesh);

        for (uint32 i = 0; i < mesh.m_vertices.size(); ++i)
        {
            XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&mesh.m_vertices[i].Position));

            XMVECTOR p = radius * n;

            XMStoreFloat3(&mesh.m_vertices[i].Position, p);
            XMStoreFloat3(&mesh.m_vertices[i].Normal, n);

            float theta = atan2f(mesh.m_vertices[i].Position.z, mesh.m_vertices[i].Position.x);

            if (theta < 0.0f)
                theta += XM_2PI;

            float phi = acosf(mesh.m_vertices[i].Position.y / radius);

            mesh.m_vertices[i].TexC.x = theta / XM_2PI;
            mesh.m_vertices[i].TexC.y = phi / XM_PI;

            mesh.m_vertices[i].TangentU.x = -radius * sinf(phi) * sinf(theta);
            mesh.m_vertices[i].TangentU.y = 0.0f;
            mesh.m_vertices[i].TangentU.z = +radius * sinf(phi) * cosf(theta);

            XMVECTOR T = XMLoadFloat3(&mesh.m_vertices[i].TangentU);
            XMStoreFloat3(&mesh.m_vertices[i].TangentU, XMVector3Normalize(T));
        }

        return mesh;
    }

    Mesh MeshGenerator::CreateCylinder(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount)
    {
        Mesh mesh;

        float stackHeight = height / stackCount;

        float radiusStep = (topRadius - bottomRadius) / stackCount;

        uint32 ringCount = stackCount + 1;

        for (uint32 i = 0; i < ringCount; ++i)
        {
            float y = -0.5f * height + i * stackHeight;
            float r = bottomRadius + i * radiusStep;

            float dTheta = 2.0f * XM_PI / sliceCount;
            for (uint32 j = 0; j <= sliceCount; ++j)
            {
                Vertex vertex;

                float c = cosf(j * dTheta);
                float s = sinf(j * dTheta);

                vertex.Position = XMFLOAT3(r * c, y, r * s);

                vertex.TexC.x = (float)j / sliceCount;
                vertex.TexC.y = 1.0f - (float)i / stackCount;

                vertex.TangentU = XMFLOAT3(-s, 0.0f, c);

                float dr = bottomRadius - topRadius;
                XMFLOAT3 bitangent(dr * c, -height, dr * s);

                XMVECTOR T = XMLoadFloat3(&vertex.TangentU);
                XMVECTOR B = XMLoadFloat3(&bitangent);
                XMVECTOR N = XMVector3Normalize(XMVector3Cross(T, B));
                XMStoreFloat3(&vertex.Normal, N);

                mesh.m_vertices.push_back(vertex);
            }
        }

        uint32 ringVertexCount = sliceCount + 1;

        for (uint32 i = 0; i < stackCount; ++i)
        {
            for (uint32 j = 0; j < sliceCount; ++j)
            {
                mesh.m_indices32.push_back(i * ringVertexCount + j);
                mesh.m_indices32.push_back((i + 1) * ringVertexCount + j);
                mesh.m_indices32.push_back((i + 1) * ringVertexCount + j + 1);

                mesh.m_indices32.push_back(i * ringVertexCount + j);
                mesh.m_indices32.push_back((i + 1) * ringVertexCount + j + 1);
                mesh.m_indices32.push_back(i * ringVertexCount + j + 1);
            }
        }

        BuildCylinderTopCap(mesh, bottomRadius, topRadius, height, sliceCount, stackCount);
        BuildCylinderBottomCap(mesh, bottomRadius, topRadius, height, sliceCount, stackCount);

        return mesh;
    }

    Mesh MeshGenerator::CreateGrid(float width, float depth, uint32 m, uint32 n)
    {
        Mesh mesh;

        uint32 vertexCount = m * n;
        uint32 faceCount = (m - 1) * (n - 1) * 2;

        float halfWidth = 0.5f * width;
        float halfDepth = 0.5f * depth;

        float dx = width / (n - 1);
        float dz = depth / (m - 1);

        float du = 1.0f / (n - 1);
        float dv = 1.0f / (m - 1);

        mesh.m_vertices.resize(vertexCount);
        for (uint32 i = 0; i < m; ++i)
        {
            float z = halfDepth - i * dz;
            for (uint32 j = 0; j < n; ++j)
            {
                float x = -halfWidth + j * dx;

                mesh.m_vertices[i * n + j].Position = XMFLOAT3(x, 0.0f, z);
                mesh.m_vertices[i * n + j].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
                mesh.m_vertices[i * n + j].TangentU = XMFLOAT3(1.0f, 0.0f, 0.0f);

                mesh.m_vertices[i * n + j].TexC.x = j * du;
                mesh.m_vertices[i * n + j].TexC.y = i * dv;
            }
        }

        mesh.m_indices32.resize(faceCount * 3);

        uint32 k = 0;
        for (uint32 i = 0; i < m - 1; ++i)
        {
            for (uint32 j = 0; j < n - 1; ++j)
            {
                mesh.m_indices32[k] = i * n + j;
                mesh.m_indices32[k + 1] = i * n + j + 1;
                mesh.m_indices32[k + 2] = (i + 1) * n + j;

                mesh.m_indices32[k + 3] = (i + 1) * n + j;
                mesh.m_indices32[k + 4] = i * n + j + 1;
                mesh.m_indices32[k + 5] = (i + 1) * n + j + 1;

                k += 6;
            }
        }

        return mesh;
    }

    Mesh MeshGenerator::CreateQuad(float x, float y, float w, float h, float depth)
    {
        Mesh mesh;

        mesh.m_vertices.resize(4);
        mesh.m_indices32.resize(6);

        mesh.m_vertices[0] = Vertex(
            x, y - h, depth,
            0.0f, 0.0f, -1.0f,
            1.0f, 0.0f, 0.0f,
            0.0f, 1.0f);

        mesh.m_vertices[1] = Vertex(
            x, y, depth,
            0.0f, 0.0f, -1.0f,
            1.0f, 0.0f, 0.0f,
            0.0f, 0.0f);

        mesh.m_vertices[2] = Vertex(
            x + w, y, depth,
            0.0f, 0.0f, -1.0f,
            1.0f, 0.0f, 0.0f,
            1.0f, 0.0f);

        mesh.m_vertices[3] = Vertex(
            x + w, y - h, depth,
            0.0f, 0.0f, -1.0f,
            1.0f, 0.0f, 0.0f,
            1.0f, 1.0f);

        mesh.m_indices32[0] = 0;
        mesh.m_indices32[1] = 1;
        mesh.m_indices32[2] = 2;

        mesh.m_indices32[3] = 0;
        mesh.m_indices32[4] = 2;
        mesh.m_indices32[5] = 3;

        return mesh;
    }

    void MeshGenerator::Subdivide(Mesh& mesh)
    {
        using uint32 = std::uint32_t;

        std::vector<Vertex> copyVertices = mesh.m_vertices;
        std::vector<uint32> copyIndices32 = mesh.m_indices32;

        mesh.m_vertices.resize(0);
        mesh.m_indices32.resize(0);

        uint32 numTris = (uint32)copyIndices32.size() / 3;
        for (uint32 i = 0; i < numTris; ++i)
        {
            Vertex v0 = copyVertices[copyIndices32[i * 3 + 0]];
            Vertex v1 = copyVertices[copyIndices32[i * 3 + 1]];
            Vertex v2 = copyVertices[copyIndices32[i * 3 + 2]];

            Vertex m0 = MidPoint(v0, v1);
            Vertex m1 = MidPoint(v1, v2);
            Vertex m2 = MidPoint(v0, v2);

            mesh.m_vertices.push_back(v0); // 0
            mesh.m_vertices.push_back(v1); // 1
            mesh.m_vertices.push_back(v2); // 2
            mesh.m_vertices.push_back(m0); // 3
            mesh.m_vertices.push_back(m1); // 4
            mesh.m_vertices.push_back(m2); // 5

            mesh.m_indices32.push_back(i * 6 + 0);
            mesh.m_indices32.push_back(i * 6 + 3);
            mesh.m_indices32.push_back(i * 6 + 5);

            mesh.m_indices32.push_back(i * 6 + 3);
            mesh.m_indices32.push_back(i * 6 + 4);
            mesh.m_indices32.push_back(i * 6 + 5);

            mesh.m_indices32.push_back(i * 6 + 5);
            mesh.m_indices32.push_back(i * 6 + 4);
            mesh.m_indices32.push_back(i * 6 + 2);

            mesh.m_indices32.push_back(i * 6 + 3);
            mesh.m_indices32.push_back(i * 6 + 1);
            mesh.m_indices32.push_back(i * 6 + 4);
        }
    }

    Vertex MeshGenerator::MidPoint(const Vertex& v0, const Vertex& v1)
    {
        XMVECTOR p0 = XMLoadFloat3(&v0.Position);
        XMVECTOR p1 = XMLoadFloat3(&v1.Position);

        XMVECTOR n0 = XMLoadFloat3(&v0.Normal);
        XMVECTOR n1 = XMLoadFloat3(&v1.Normal);

        XMVECTOR tan0 = XMLoadFloat3(&v0.TangentU);
        XMVECTOR tan1 = XMLoadFloat3(&v1.TangentU);

        XMVECTOR tex0 = XMLoadFloat2(&v0.TexC);
        XMVECTOR tex1 = XMLoadFloat2(&v1.TexC);

        XMVECTOR pos = 0.5f * (p0 + p1);
        XMVECTOR normal = XMVector3Normalize(0.5f * (n0 + n1));
        XMVECTOR tangent = XMVector3Normalize(0.5f * (tan0 + tan1));
        XMVECTOR tex = 0.5f * (tex0 + tex1);

        Vertex v;
        XMStoreFloat3(&v.Position, pos);
        XMStoreFloat3(&v.Normal, normal);
        XMStoreFloat3(&v.TangentU, tangent);
        XMStoreFloat2(&v.TexC, tex);

        return v;
    }

    void MeshGenerator::BuildCylinderTopCap(Mesh& mesh, float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount)
    {
        uint32 baseIndex = (uint32)mesh.m_vertices.size();

        float y = 0.5f * height;
        float dTheta = 2.0f * XM_PI / sliceCount;

        for (uint32 i = 0; i <= sliceCount; ++i)
        {
            float x = topRadius * cosf(i * dTheta);
            float z = topRadius * sinf(i * dTheta);

            float u = x / height + 0.5f;
            float v = z / height + 0.5f;

            mesh.m_vertices.push_back(Vertex(x, y, z, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, u, v));
        }

        mesh.m_vertices.push_back(Vertex(0.0f, y, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f));

        uint32 centerIndex = (uint32)mesh.m_vertices.size() - 1;

        for (uint32 i = 0; i < sliceCount; ++i)
        {
            mesh.m_indices32.push_back(centerIndex);
            mesh.m_indices32.push_back(baseIndex + i + 1);
            mesh.m_indices32.push_back(baseIndex + i);
        }
    }

    void MeshGenerator::BuildCylinderBottomCap(Mesh& mesh, float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount)
    {
        uint32 baseIndex = (uint32)mesh.m_vertices.size();
        float y = -0.5f * height;

        float dTheta = 2.0f * XM_PI / sliceCount;
        for (uint32 i = 0; i <= sliceCount; ++i)
        {
            float x = bottomRadius * cosf(i * dTheta);
            float z = bottomRadius * sinf(i * dTheta);

            float u = x / height + 0.5f;
            float v = z / height + 0.5f;

            mesh.m_vertices.push_back(Vertex(x, y, z, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, u, v));
        }

        mesh.m_vertices.push_back(Vertex(0.0f, y, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f));

        uint32 centerIndex = (uint32)mesh.m_vertices.size() - 1;

        for (uint32 i = 0; i < sliceCount; ++i)
        {
            mesh.m_indices32.push_back(centerIndex);
            mesh.m_indices32.push_back(baseIndex + i);
            mesh.m_indices32.push_back(baseIndex + i + 1);
        }
    }
}
