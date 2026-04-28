#pragma once
#include "D3D12CommonHeaders.h"
#include "UploadBufferDx12.h"
#include "GPUStructs.h"

namespace raphael
{
    // Per-frame rendering resources
    struct FrameResources
    {
        // We cannot update a cbuffer until the GPU is done processing the commands
        // that reference it.  So each frame needs their own cbuffers.
        std::unique_ptr<UploadBuffer<PassConstants>> passCB;
        std::unique_ptr<UploadBuffer<ObjectConstants>> objectCB;
        std::unique_ptr<UploadBuffer<MaterialConstants>> materialCB;
    };

    struct FrameContext
    {
        ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;
        UINT64 fenceValue = 0;
        std::unique_ptr<FrameResources> resources = nullptr;
    };
}
