#pragma once
#include "ObjectDescriptors.h"

using Microsoft::WRL::ComPtr;

namespace raphael
{
    class ShaderDx12
    {
    public:
        ShaderDx12(const ShaderDesc& desc);
        ~ShaderDx12() = default;
        const ShaderDesc& getDesc() const { return m_desc; }
        
        // DX12 specific methods to get shader bytecode
        D3D12_SHADER_BYTECODE getVertexShaderBytecode() const;
        D3D12_SHADER_BYTECODE getPixelShaderBytecode() const;

        ComPtr<ID3DBlob> compileShader(
            const std::wstring& filename,
            const D3D_SHADER_MACRO* defines,
            const std::string& entrypoint,
            const std::string& target);

    public:
        std::unordered_map<ShaderDesc::ShaderType, ComPtr<ID3DBlob>> m_shaderObjs;

    private:
        ShaderDesc m_desc = {};
    };
}
