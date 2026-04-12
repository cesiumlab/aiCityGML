#pragma once

#include "core/citygml_feature.h"

namespace citygml {

// ============================================================
// AbstractConstruction - 建筑结构基类
// ============================================================

class AbstractConstruction : public CityGMLFeatureWithLifespan {
public:
    virtual ~AbstractConstruction() = default;
};

// ============================================================
// AbstractConstructiveElement - 结构构件基类
// ============================================================

class AbstractConstructiveElement : public AbstractConstruction {
public:
    virtual ~AbstractConstructiveElement() = default;
};

// ============================================================
// AbstractFurniture - 家具基类
// ============================================================

class AbstractFurniture : public AbstractConstruction {
public:
    virtual ~AbstractFurniture() = default;
    
    const std::array<double, 3>& getBoundedBy() const { return boundedBy_; }
    void setBoundedBy(double x, double y, double z) { boundedBy_ = {x, y, z}; }
    
private:
    std::array<double, 3> boundedBy_ = {0, 0, 0};
};

// ============================================================
// AbstractInstallation - 设施基类
// ============================================================

class AbstractInstallation : public AbstractConstruction {
public:
    virtual ~AbstractInstallation() = default;
};

} // namespace citygml