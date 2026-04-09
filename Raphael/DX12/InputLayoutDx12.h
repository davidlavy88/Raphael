#pragma once
#include "D3D12CommonHeaders.h"
#include "ObjectDescriptors.h"

namespace raphael
{
    class InputLayoutDx12
    {
    public:
        InputLayoutDx12() = default;
        ~InputLayoutDx12() = default;

        // Convert API -agnostic input element descriptions to D3D12_INPUT_ELEMENT_DESC and store them
        static std::vector<D3D12_INPUT_ELEMENT_DESC> convertToD3D12InputLayout(const InputLayoutDesc& desc);

    private:
        static const char* semanticNameToString(InputElementSemantic semantic);

    private:
        std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayouts;
    };
}
