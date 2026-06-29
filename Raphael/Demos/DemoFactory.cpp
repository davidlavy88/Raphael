#include "DemoFactory.h"

#include "BoxDemo.h"
#include "QuadDemo.h"
#include "TexturedBoxDemo.h"
#include "GltfDemo.h"
#include "GBufferDemo.h"
#include "RayTracerDemo.h"
#include "MultiObjectDemo.h"

#include <stdexcept>

namespace raphael
{
    std::unique_ptr<IDemo> CreateDemo(DemoType type)
    {
        switch (type)
        {
        case DemoType::Box:         return std::make_unique<BoxDemo>();
        case DemoType::Quad:        return std::make_unique<QuadDemo>();
        case DemoType::TexturedBox: return std::make_unique<TexturedBoxDemo>();
        case DemoType::Gltf:        return std::make_unique<GltfDemo>();
        case DemoType::GBuffer:     return std::make_unique<GBufferDemo>();
        case DemoType::RayTracer:   return std::make_unique<RayTracerDemo>();
        case DemoType::MultiObject: return std::make_unique<MultiObjectDemo>();
        }

        throw std::runtime_error("CreateDemo: unknown DemoType");
    }
}
