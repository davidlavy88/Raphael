#pragma once

#include "D3D12CommonHeaders.h"

class PoissonDiskDistribution
{
public:
    PoissonDiskDistribution(float spawnRadius, const XMVECTOR& minExtent, const XMVECTOR& maxExtent);

    // Generate new sample points around an existing point
    void SpawnNewSamples(int count);

    // Check if a point is within the defined extents
    bool PointInExtents(const XMVECTOR& location) const;

    // Check if a point is too close to existing samples
    bool PointIntersectsGrid(const XMVECTOR& location) const;

    // Get all generated samples
    const std::vector<XMVECTOR>& GetSamples() const { return m_samples; }

    // Get the current number of samples
    size_t GetSampleCount() const { return m_samples.size(); }

    // Get active index for spawning
    size_t GetActiveIndex() const { return m_activeIndex; }

    // Check if there are more active points to process
    bool HasActiveSamples() const { return m_activeIndex < m_samples.size(); }

private:
    float m_spawnRadius;
    XMVECTOR m_minExtent;
    XMVECTOR m_maxExtent;

    std::vector<XMVECTOR> m_samples;
    size_t m_activeIndex;

    // Grid for spatial hashing
    float m_cellSize;
    float m_gridWidth;
    float m_gridHeight;
    float m_gridDepth;
    size_t m_cellsNumX;
    size_t m_cellsNumY;
    size_t m_cellsNumZ;
    std::vector<std::vector<std::vector<int>>> m_grid;
};

