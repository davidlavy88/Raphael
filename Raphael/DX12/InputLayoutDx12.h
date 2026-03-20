#pragma once
#include "D3D12CommonHeaders.h"

namespace raphael
{
    class InputLayoutDx12
    {
    public:
        InputLayoutDx12();
        ~InputLayoutDx12() = default;
        void addInputLayout(const std::string& name, const std::vector<D3D12_INPUT_ELEMENT_DESC>& elementDesc);
        const std::vector<D3D12_INPUT_ELEMENT_DESC>& getInputLayout(const std::string name) const;

    private:
        std::unordered_map<std::string, std::vector<D3D12_INPUT_ELEMENT_DESC>> m_inputLayouts;
    };
}
