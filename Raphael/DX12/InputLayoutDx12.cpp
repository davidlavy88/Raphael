#include "InputLayoutDx12.h"

namespace raphael
{
    InputLayoutDx12::InputLayoutDx12()
    {
        // Example input layout for a vertex with position and color attributes
        // TODO: This should be moved into a render class
        std::vector<D3D12_INPUT_ELEMENT_DESC> defaultLayout =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        addInputLayout("DefaultLayout", defaultLayout);
    }

    void InputLayoutDx12::addInputLayout(const std::string& name, const std::vector<D3D12_INPUT_ELEMENT_DESC>& elementDesc)
    {
        m_inputLayouts.insert({ name, elementDesc });
    }

    const std::vector<D3D12_INPUT_ELEMENT_DESC>& InputLayoutDx12::getInputLayout(const std::string name) const
    {
        auto it = m_inputLayouts.find(name);
        if (it == m_inputLayouts.end())
        {
            throw std::runtime_error("Input layout not found: " + name);
        }
        else
        {
            return it->second;
        }
    }
}
