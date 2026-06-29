#pragma once
#include "Components/Window.h"
#include <cstdint>

namespace raphael
{
    // Number of frames in flight for CPU/GPU double-buffering. 
    // Shared by every demo
    inline constexpr uint32_t g_frameCount = 2;

    class IDemo
    {
    public:
        virtual ~IDemo() = default;
        virtual bool Initialize(WindowInfo windowInfo) = 0;
        virtual void Shutdown() = 0;
        virtual void Render() = 0;
        virtual void Resize(unsigned int width, unsigned int height) = 0;
    };
}
