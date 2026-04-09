#pragma once

#include "citygml/core/citygml_space.h"

namespace citygml {

// ============================================================
// AbstractThematicSurface - 主题表面基类
// ============================================================

class AbstractThematicSurface : public CityGMLFeature {
public:
    virtual ~AbstractThematicSurface() = default;
    
    const std::vector<QualifiedAreaPtr>& getAreas() const { return areas_; }
    void addArea(QualifiedAreaPtr area) { areas_.push_back(area); }
    
private:
    std::vector<QualifiedAreaPtr> areas_;
};

// ============================================================
// ClosureSurface - 闭合表面
// ============================================================

class ClosureSurface : public AbstractThematicSurface {
public:
    virtual ~ClosureSurface() = default;
};

} // namespace citygml