#include "InputLayoutDx12.h"
#include "UtilDx12.h"

namespace raphael
{
    std::vector<D3D12_INPUT_ELEMENT_DESC> InputLayoutDx12::convertToD3D12InputLayout(const InputLayoutDesc& desc)
    {
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;

        for (const auto& element : desc.elements)
        {
            D3D12_INPUT_ELEMENT_DESC d12dDesc = {};
            d12dDesc.SemanticName = semanticNameToString(element.semanticName);
            d12dDesc.SemanticIndex = element.semanticIndex;
            d12dDesc.Format = convertFormatToDXGI(element.format);
            d12dDesc.InputSlot = element.inputSlot;
            d12dDesc.AlignedByteOffset = element.alignedByteOffset;
            d12dDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA; // Assuming per-vertex data for now. You can extend InputElementDesc to specify this if needed.
            d12dDesc.InstanceDataStepRate = 0; // Not used for per-vertex data

            inputLayout.push_back(d12dDesc);
        }

        return inputLayout;
    }

    const char* InputLayoutDx12::semanticNameToString(InputElementSemantic semantic)
    {
        switch (semantic)
        {
        case raphael::InputElementSemantic::Position:
            return "POSITION";
        case raphael::InputElementSemantic::Normal:
            return "NORMAL";
        case raphael::InputElementSemantic::TexCoord:
            return "TEXCOORD";
        case raphael::InputElementSemantic::Color:
            return "COLOR";
        default:
            return "UNKNOWN_SEMANTIC";
        }
    }
}
