#pragma once

// -----------------------------------------------------------------------------
// PIX GPU event markers
//
// Thin wrapper over WinPixEventRuntime's pix3.h. GPU markers make a captured
// frame readable: instead of an anonymous wall of DrawIndexedInstanced calls,
// PIX (and RenderDoc) show named, colored, nestable regions such as
// "Cube Render Pass > Geometry".
//
// pix3.h emits real events only when PIX is enabled (Debug/Profile builds, via
// _DEBUG) and compiles to no-ops otherwise, so there is zero cost in shipping
// builds. The WinPixEventRuntime NuGet package supplies the header, the import
// library, and copies WinPixEventRuntime.dll next to the executable.
// -----------------------------------------------------------------------------

#include <cstdint>
#include <windows.h>
#include <pix3.h>

namespace raphael
{
    // Stable colors so passes are easy to tell apart on the PIX timeline.
    namespace PixColors
    {
        inline const uint32_t Pass     = PIX_COLOR(150, 120, 230); // whole render pass
        inline const uint32_t Geometry = PIX_COLOR( 70, 150, 255); // scene draws
        inline const uint32_t Ui       = PIX_COLOR(255, 180,  60); // ImGui / UI
    }
}
