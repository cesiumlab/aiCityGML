#pragma once

#include "citygml/core/citygml_space.h"

namespace citygml {

class AbstractThematicSurface : public CityGMLFeature {
public:
    virtual ~AbstractThematicSurface() = default;
    const std::vector<QualifiedAreaPtr>& getAreas() const { return areas_; }
    void addArea(QualifiedAreaPtr area) { areas_.push_back(area); }

private:
    std::vector<QualifiedAreaPtr> areas_;
};

class ClosureSurface : public AbstractThematicSurface {
public:
    virtual ~ClosureSurface() = default;
};

}
