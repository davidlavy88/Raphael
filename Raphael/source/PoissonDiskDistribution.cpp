#include "PoissonDiskDistribution.h"
#include <random>

PoissonDiskDistribution::PoissonDiskDistribution(float spawnRadius, 
                                                 const XMVECTOR& minExtent, 
                                                 const XMVECTOR& maxExtent)
    : m_spawnRadius(spawnRadius)
    , m_minExtent(minExtent)
    , m_maxExtent(maxExtent)
    , m_activeIndex(0)
{
    // Add initial sample at origin
    m_samples.push_back(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f));
    
    // Calculate grid dimensions
    m_cellSize = m_spawnRadius / std::sqrt(2.0f);
    m_gridWidth = XMVectorGetX(m_maxExtent) - XMVectorGetX(m_minExtent);
    m_gridHeight = XMVectorGetY(m_maxExtent) - XMVectorGetY(m_minExtent);
    m_gridDepth = XMVectorGetZ(m_maxExtent) - XMVectorGetZ(m_minExtent);
    m_cellsNumX = static_cast<size_t>(ceilf(m_gridWidth / m_cellSize));
    m_cellsNumY = static_cast<size_t>(ceilf(m_gridHeight / m_cellSize));
    m_cellsNumZ = static_cast<size_t>(ceilf(m_gridDepth / m_cellSize));
    
    // Initialize 3D grid with -1 (indicating empty cells)
    m_grid = std::vector<std::vector<std::vector<int>>>(m_cellsNumX,
        std::vector<std::vector<int>>(m_cellsNumY,
            std::vector<int>(m_cellsNumZ, -1)));
    
    // Set grid cell coordinates where the first sample (at position 0, 0, 0) is stored
    int pointIndexX = static_cast<int>(ceilf((m_gridWidth / 2.0f) / m_cellSize));
    int pointIndexY = static_cast<int>(ceilf((m_gridHeight / 2.0f) / m_cellSize));
    int pointIndexZ = static_cast<int>(ceilf((m_gridDepth / 2.0f) / m_cellSize));
    m_grid[pointIndexX][pointIndexY][pointIndexZ] = 0;
}

void PoissonDiskDistribution::SpawnNewSamples(int count)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0f, 1.0f);
    
    for (size_t i = 0; i < count; ++i)
    {
        XMVECTOR newLocation = m_samples[m_activeIndex];
        
        float distance = static_cast<float>(dis(gen) + 1.0f) * m_spawnRadius;  // Spawn in [R; 2*R]
        float anglePitch = static_cast<float>(dis(gen)) * XM_2PI;     // Random pitch angle
        float angleYaw = static_cast<float>(dis(gen)) * XM_2PI;       // Random yaw angle
        
        newLocation += XMVectorSet(distance * XMScalarCos(anglePitch), distance * XMScalarSin(anglePitch),
            distance * XMScalarSin(angleYaw), 0.0f);
        
        if (PointInExtents(newLocation) && !PointIntersectsGrid(newLocation))
        {
            m_samples.push_back(newLocation);
            int pointIndexX = static_cast<int>((XMVectorGetX(newLocation) + (m_gridWidth / 2.0f)) / m_cellSize);
            int pointIndexY = static_cast<int>((XMVectorGetY(newLocation) + (m_gridHeight / 2.0f)) / m_cellSize);
            int pointIndexZ = static_cast<int>((XMVectorGetZ(newLocation) + (m_gridDepth / 2.0f)) / m_cellSize);
            
            if (m_grid[pointIndexX][pointIndexY][pointIndexZ] != -1)
                continue;
            
            m_grid[pointIndexX][pointIndexY][pointIndexZ] = static_cast<int>(m_samples.size() - 1);
        }
    }
    
    ++m_activeIndex;
}

bool PoissonDiskDistribution::PointInExtents(const XMVECTOR& location) const
{
    return (XMVectorGetX(location) > XMVectorGetX(m_minExtent) && XMVectorGetY(location) > XMVectorGetY(m_minExtent) && XMVectorGetZ(location) > XMVectorGetZ(m_minExtent)) 
        && (XMVectorGetX(location) < XMVectorGetX(m_maxExtent) && XMVectorGetY(location) < XMVectorGetY(m_maxExtent) && XMVectorGetZ(location) < XMVectorGetZ(m_maxExtent));
}

bool PoissonDiskDistribution::PointIntersectsGrid(const XMVECTOR& location) const
{
    int pointIndexX = static_cast<int>((XMVectorGetX(location) + (m_gridWidth / 2.0f)) / m_cellSize);
    int pointIndexY = static_cast<int>((XMVectorGetY(location) + (m_gridHeight / 2.0f)) / m_cellSize);
    int pointIndexZ = static_cast<int>((XMVectorGetZ(location) + (m_gridDepth / 2.0f)) / m_cellSize);
    
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            for (int z = -1; z <= 1; z++)
            {
                int indX = pointIndexX + x;
                int indY = pointIndexY + y;
                int indZ = pointIndexZ + z;
                
                if (indX < 0 || indX >= m_cellsNumX)
                    continue;
                if (indY < 0 || indY >= m_cellsNumY)
                    continue;
                if (indZ < 0 || indZ >= m_cellsNumZ)
                    continue;
                
                int sampleIndex = m_grid[indX][indY][indZ];
                if (sampleIndex == -1)
                    continue;
                
                float dist = XMVectorGetX(
                    XMVector3Length(
                        XMVectorSubtract(m_samples[sampleIndex], location)));
                if (dist <= m_spawnRadius)
                    return true;
            }
        }
    }
    
    return false;
}

