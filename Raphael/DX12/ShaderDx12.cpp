#include "ShaderDx12.h"

namespace raphael
{
    ShaderDx12::ShaderDx12(const ShaderDesc& desc)
        : m_desc(desc)
    {
        for (const auto& type : desc.types)
        {
            std::string entrypoint;
            std::string target;
            switch (type)
            {
            case ShaderDesc::ShaderType::Vertex:
                entrypoint = "VS";
                target = "vs_5_0";
                break;
            case ShaderDesc::ShaderType::Pixel:
                entrypoint = "PS";
                target = "ps_5_0";
                break;
            case ShaderDesc::ShaderType::Compute:
                entrypoint = "CS";
                target = "cs_5_0";
                break;
            default:
                throw std::runtime_error("Unsupported shader type");
            }
            m_shaderObjs[type] = compileShader(desc.shaderFilePath, nullptr, entrypoint, target);
        }
    }

    D3D12_SHADER_BYTECODE ShaderDx12::getVertexShaderBytecode() const
    {
        auto it = m_shaderObjs.find(ShaderDesc::ShaderType::Vertex);
        if (it != m_shaderObjs.end())
        {
            const ComPtr<ID3DBlob>& bytecodeBlob = it->second;
            return { reinterpret_cast<BYTE*>(bytecodeBlob->GetBufferPointer()), bytecodeBlob->GetBufferSize() };
        }

        return { nullptr, 0 } ;
    }

    D3D12_SHADER_BYTECODE ShaderDx12::getPixelShaderBytecode() const
    {
        auto it = m_shaderObjs.find(ShaderDesc::ShaderType::Pixel);
        if (it != m_shaderObjs.end())
        {
            const ComPtr<ID3DBlob>& bytecodeBlob = it->second;
            return { reinterpret_cast<BYTE*>(bytecodeBlob->GetBufferPointer()), bytecodeBlob->GetBufferSize() };
        }

        return { nullptr, 0 };
    }

    ComPtr<ID3DBlob> ShaderDx12::compileShader(
        const std::wstring& filename,
        const D3D_SHADER_MACRO* defines,
        const std::string& entrypoint,
        const std::string& target)
    {
        UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
        compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_WARNINGS_ARE_ERRORS;
#endif

        HRESULT hr = S_OK;

        ComPtr<ID3DBlob> byteCode = nullptr;
        ComPtr<ID3DBlob> errors;
        if (FAILED(D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors)))
        {
            OutputDebugStringA((char*)errors->GetBufferPointer());
            throw std::runtime_error("Failed to compile shader");
        }

        if (errors != nullptr)
            OutputDebugStringA((char*)errors->GetBufferPointer());

        return byteCode;
    }


}
