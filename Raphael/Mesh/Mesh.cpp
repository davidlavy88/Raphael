#include "Mesh.h"

namespace raphael
{
    const std::vector<Mesh::uint16>& Mesh::GetIndices16() const
    {
        return m_indices16;
    }

    void Mesh::GenerateIndices16()
    {
        m_indices16.resize(m_indices32.size());
        for (size_t i = 0; i < m_indices32.size(); ++i)
        {
            m_indices16[i] = static_cast<uint16>(m_indices32[i]);
        }
    }
}
