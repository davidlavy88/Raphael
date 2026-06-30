#pragma once
#include "Components/Window.h"
#include <cstdint>
#include <optional>

namespace raphael
{
    // Number of frames in flight for CPU/GPU double-buffering. 
    // Shared by every demo
    inline constexpr uint32_t g_frameCount = 2;

    // Identifies each demo. Used by the factory (CreateDemo) and the in-app switcher.
    enum class DemoType
    {
        Box,
        Quad,
        TexturedBox,
        Gltf,
        GBuffer,
        RayTracer,
        MultiObject
    };

    class IDemo
    {
    public:
        virtual ~IDemo() = default;
        virtual bool Initialize(WindowInfo windowInfo) = 0;
        virtual void Shutdown() = 0;
        virtual void Render() = 0;
        virtual void Resize(unsigned int width, unsigned int height) = 0;

        // --- In-app demo switching ---
        // TestRenderer tags each demo with its own type after construction so the
        // switcher can highlight the active selection.
        void SetDemoType(DemoType type) { m_demoType = type; }
        // Returns and clears a switch request raised by the ImGui dropdown.
        std::optional<DemoType> TakeSwitchRequest();

    protected:
        // Draws an ImGui combo of the switchable demos. Call once per frame inside
        // the active ImGui frame (after NewFrame, before the ImGui render pass).
        void DrawDemoSwitcher();

        DemoType m_demoType = DemoType::GBuffer;
        std::optional<DemoType> m_switchRequest;
    };
}
