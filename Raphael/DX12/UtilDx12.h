#pragma once
#include "D3D12CommonHeaders.h"
#include "Constants.h"

namespace raphael
{
    // Utility function to convert ResourceBindFlags to D3D12_RESOURCE_FLAGS
    inline DXGI_FORMAT convertFormatToDXGI(ResourceFormat format)
    {
        switch (format)
        {
        case raphael::ResourceFormat::Unknown:
            return DXGI_FORMAT_UNKNOWN;
        case raphael::ResourceFormat::R8G8B8A8_UNORM:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
		case raphael::ResourceFormat::R16G16B16A16_FLOAT:
			return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case raphael::ResourceFormat::R32G32_FLOAT:
            return DXGI_FORMAT_R32G32_FLOAT;
        case raphael::ResourceFormat::R32G32B32_FLOAT:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case raphael::ResourceFormat::R32G32B32A32_FLOAT:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case raphael::ResourceFormat::D24_UNORM_S8_UINT:
            return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case raphael::ResourceFormat::R32_FLOAT:
            return DXGI_FORMAT_R32_FLOAT;
        case raphael::ResourceFormat::R32_UINT:
            return DXGI_FORMAT_R32_UINT;
        default:
            return DXGI_FORMAT_UNKNOWN;
        }
    }

    inline ResourceFormat convertFormatFromDXGI(DXGI_FORMAT format)
    {
        switch (format)
        {
        case DXGI_FORMAT_UNKNOWN:
            return ResourceFormat::Unknown;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
            return ResourceFormat::R8G8B8A8_UNORM;
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
			return ResourceFormat::R16G16B16A16_FLOAT;
        case DXGI_FORMAT_R32G32_FLOAT:
            return ResourceFormat::R32G32_FLOAT;
        case DXGI_FORMAT_R32G32B32_FLOAT:
            return ResourceFormat::R32G32B32_FLOAT;
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
            return ResourceFormat::R32G32B32A32_FLOAT;
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            return ResourceFormat::D24_UNORM_S8_UINT;
        case DXGI_FORMAT_R32_FLOAT:
            return ResourceFormat::R32_FLOAT;
        case DXGI_FORMAT_R32_UINT:
            return ResourceFormat::R32_UINT;
        default:
            return ResourceFormat::Unknown;
        }
    }

    inline D3D12_FILL_MODE convertFillModeToD3D12(RasterizerFillMode fillMode)
    {
        switch (fillMode)
        {
        case RasterizerFillMode::Solid:
            return D3D12_FILL_MODE_SOLID;
        case RasterizerFillMode::Wireframe:
            return D3D12_FILL_MODE_WIREFRAME;
        default:
            return D3D12_FILL_MODE_SOLID;
        }
    }
     inline D3D12_CULL_MODE convertCullModeToD3D12(RasterizerCullMode cullMode)
    {
        switch (cullMode)
        {
        case RasterizerCullMode::None:
            return D3D12_CULL_MODE_NONE;
        case RasterizerCullMode::Front:
            return D3D12_CULL_MODE_FRONT;
        case RasterizerCullMode::Back:
            return D3D12_CULL_MODE_BACK;
        default:
            return D3D12_CULL_MODE_BACK;
        }
	 }
}