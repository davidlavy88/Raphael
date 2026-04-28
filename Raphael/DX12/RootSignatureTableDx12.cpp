#include "RootSignatureTableDx12.h"
#include "DeviceDx12.h"

namespace raphael
{
    RootSignatureTableDx12::RootSignatureTableDx12(DeviceDx12* device, DescriptorHeapDx12* srvHeap, const RootSignatureTableDesc& desc)
        : m_desc(desc), m_device(device), m_descriptorHeap(srvHeap)
    {
        // Resource views are expected to live on a shader-visible descriptor heap
        // We just grab the GPU handle for the first descriptor in the table, 
        // and rely on the shader to index into the table based on the root parameter index
        if (desc.viewCount > 0)
        {
            m_gpuHandle = m_desc.resourceViews[0].gpuHandle;
        }
    }
}
