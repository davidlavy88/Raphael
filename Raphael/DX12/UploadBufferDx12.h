#pragma once
#include "D3D12CommonHeaders.h"
#include "DeviceDx12.h"
#include "ResourceDx12.h"
#include "ObjectDescriptors.h"

namespace raphael
{
    static UINT CalcConstantBufferByteSize(UINT byteSize)
    {
        // Round up to nearest multiple of 256.  We do this by adding 255 and then masking off
        // the lower 2 bytes which store all bits < 256. Example: byteSize = 300.
        // (300 + 255) & ~255 = 555 & ~255
        // 0x022B & ~0x00ff = 0x022B & 0xff00
        // 0x0200 = 512
        return (byteSize + 255) & ~255;
    }

    template<typename T>
    class UploadBuffer
    {
    public:
        UploadBuffer(DeviceDx12* device, UINT elementCount, bool isConstantBuffer)
        {
            // Constant buffer elements need to be multiples of 256 bytes.
            // This is because the hardware can only view constant data 
            // at m*256 byte offsets and of n*256 byte lengths. 
            isConstantBuffer ?
                m_byteSize = CalcConstantBufferByteSize(sizeof(T)) : m_byteSize = sizeof(T);

            ResourceDesc desc;
            desc.type = ResourceDesc::ResourceType::Buffer;
            desc.usage = ResourceDesc::Usage::Upload;
            desc.width = m_byteSize * elementCount;

            m_uploadBuffer = std::make_unique<ResourceDx12>(device, desc);

            if (!m_uploadBuffer->map(reinterpret_cast<void**>(&m_mappedData)))
            {
                throw std::runtime_error("Failed to map upload buffer");
            }
        }

        UploadBuffer(const UploadBuffer& rhs) = delete;
        UploadBuffer& operator=(const UploadBuffer& rhs) = delete;
        ~UploadBuffer()
        {
            m_uploadBuffer->unmap();
            m_mappedData = nullptr;
        }

        ID3D12Resource* getResource()const
        {
            return m_uploadBuffer->getNativeResource();
        }

        void CopyData(size_t elementIndex, const T& data)
        {
            memcpy(&m_mappedData[elementIndex * m_byteSize], &data, sizeof(T));
        }

    private:
        std::unique_ptr<ResourceDx12> m_uploadBuffer;
        BYTE* m_mappedData = nullptr;
        UINT m_byteSize = 0;
    };
}
