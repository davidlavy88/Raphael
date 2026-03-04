#pragma once
#include <wrl.h>
#include <dxgiformat.h>
#include "Constants.h"

namespace raphael
{

    // Device initialization
    struct DeviceDesc {
        const char* applicationName = nullptr;
        bool enableDebugLayer = false;
    };

    struct ResourceDesc {
        enum class ResourceType {
            Buffer,
            Texture2D
        };

        enum class Usage {
            Default,
            Upload
        };

        ResourceType type;
        Usage usage;
        UINT64 width;
        UINT height;
        DXGI_FORMAT format;
        UINT bindFlags; // SRV, RTV, DSV, etc
    };

    struct CommandListDesc {
        const char* debugName = nullptr;
        CommandListType type = CommandListType::Direct;
    };
} // namespace raphael

