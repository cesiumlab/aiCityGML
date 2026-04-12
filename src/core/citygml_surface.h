#pragma once

#include "core/citygml_space.h"
#include "core/citygml_geometry.h"

namespace citygml {

// 前向声明
class AbstractOpening;

// ============================================================
// AbstractThematicSurface - 抽象主题表面基类
// CityGML 中所有空间边界表面的基类
// ============================================================

class AbstractThematicSurface : public CityGMLFeature {
public:
    virtual ~AbstractThematicSurface() = default;

    const std::vector<QualifiedAreaPtr>& getAreas() const { return areas_; }
    void addArea(QualifiedAreaPtr area) { areas_.push_back(area); }

    // 获取几何体（从 MultiSurface 中提取）
    std::shared_ptr<MultiSurface> getMultiSurface() const { return multiSurface_; }
    void setMultiSurface(std::shared_ptr<MultiSurface> ms) { multiSurface_ = ms; }

    // 开口（门、窗等）
    const std::vector<std::shared_ptr<AbstractOpening>>& getOpenings() const { return openings_; }
    void addOpening(std::shared_ptr<AbstractOpening> opening) { openings_.push_back(opening); }

private:
    std::vector<QualifiedAreaPtr> areas_;
    std::shared_ptr<MultiSurface> multiSurface_;
    std::vector<std::shared_ptr<AbstractOpening>> openings_;
};

using ThematicSurfacePtr = std::shared_ptr<AbstractThematicSurface>;

// ============================================================
// RoofSurface - 屋顶表面
// ============================================================

class RoofSurface : public AbstractThematicSurface {
public:
    virtual ~RoofSurface() = default;
};

using RoofSurfacePtr = std::shared_ptr<RoofSurface>;

// ============================================================
// WallSurface - 墙体表面
// ============================================================

class WallSurface : public AbstractThematicSurface {
public:
    virtual ~WallSurface() = default;
};

using WallSurfacePtr = std::shared_ptr<WallSurface>;

// ============================================================
// GroundSurface - 地面表面
// ============================================================

class GroundSurface : public AbstractThematicSurface {
public:
    virtual ~GroundSurface() = default;
};

using GroundSurfacePtr = std::shared_ptr<GroundSurface>;

// ============================================================
// ClosureSurface - 封闭表面
// ============================================================

class ClosureSurface : public AbstractThematicSurface {
public:
    virtual ~ClosureSurface() = default;
};

using ClosureSurfacePtr = std::shared_ptr<ClosureSurface>;

// ============================================================
// InteriorWallSurface - 室内墙体表面
// ============================================================

class InteriorWallSurface : public AbstractThematicSurface {
public:
    virtual ~InteriorWallSurface() = default;
};

using InteriorWallSurfacePtr = std::shared_ptr<InteriorWallSurface>;

// ============================================================
// CeilingSurface - 天花板表面
// ============================================================

class CeilingSurface : public AbstractThematicSurface {
public:
    virtual ~CeilingSurface() = default;
};

using CeilingSurfacePtr = std::shared_ptr<CeilingSurface>;

// ============================================================
// FloorSurface - 地板表面
// ============================================================

class FloorSurface : public AbstractThematicSurface {
public:
    virtual ~FloorSurface() = default;
};

using FloorSurfacePtr = std::shared_ptr<FloorSurface>;

// ============================================================
// OuterCeilingSurface - 室外天花板表面
// ============================================================

class OuterCeilingSurface : public AbstractThematicSurface {
public:
    virtual ~OuterCeilingSurface() = default;
};

using OuterCeilingSurfacePtr = std::shared_ptr<OuterCeilingSurface>;

// ============================================================
// OuterFloorSurface - 室外地板表面（如阳台地板）
// ============================================================

class OuterFloorSurface : public AbstractThematicSurface {
public:
    virtual ~OuterFloorSurface() = default;
};

using OuterFloorSurfacePtr = std::shared_ptr<OuterFloorSurface>;

} // namespace citygml
