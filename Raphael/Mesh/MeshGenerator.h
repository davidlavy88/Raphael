#pragma once
#include "Mesh.h"

namespace raphael
{
    class MeshGenerator
    {
    public:
        using uint32 = std::uint32_t;

        /// Creates a box centered at the origin with the given dimensions.
        /// numSubdivisions controls how finely each face is tessellated.
        static Mesh CreateBox(float width, float height, float depth, uint32 numSubdivisions);

        /// Creates a sphere centered at the origin with the given radius.  The
        /// slices and stacks parameters control the degree of tessellation.
        static Mesh CreateSphere(float radius, uint32 sliceCount, uint32 stackCount);

        /// Creates a geosphere centered at the origin with the given radius.  The
        /// depth controls the level of tessellation.
        static Mesh CreateGeosphere(float radius, uint32 numSubdivisions);

        /// Creates a cylinder parallel to the y-axis, and centered about the origin.  
        /// The bottom and top radius can vary to form various cone shapes rather than true
        /// cylinders.  The slices and stacks parameters control the degree of tessellation.
        static Mesh CreateCylinder(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount);

        /// Creates an mxn grid in the xz-plane with m rows and n columns, centered
        /// at the origin with the specified width and depth.
        static Mesh CreateGrid(float width, float depth, uint32 m, uint32 n);

        /// Creates a quad aligned with the screen.  This is useful for postprocessing and screen effects.
        static Mesh CreateQuad(float x, float y, float w, float h, float depth);

    private:
        static void Subdivide(Mesh& mesh);
        static Vertex MidPoint(const Vertex& v0, const Vertex& v1);
        static void BuildCylinderTopCap(Mesh& mesh, float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount);
        static void BuildCylinderBottomCap(Mesh& mesh, float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount);
    };
}
