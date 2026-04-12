#pragma once

#include "citygml/core/citygml_feature.h"
#include "citygml/core/citygml_geometry.h"
#include <optional>
#include <string>

namespace citygml {

class AbstractSpaceBoundary;
class QualifiedVolume;
class QualifiedArea;

class QualifiedVolume : public CityGMLObject {
public:
    double getValue() const { return value_; }
    void setValue(double value) { value_ = value; }
    const std::optional<std::string>& getTypeOfVolume() const { return typeOfVolume_; }
    void setTypeOfVolume(const std::string& type) { typeOfVolume_ = type; }

private:
    double value_ = 0.0;
    std::optional<std::string> typeOfVolume_;
};

using QualifiedVolumePtr = std::shared_ptr<QualifiedVolume>;

class QualifiedArea : public CityGMLObject {
public:
    double getValue() const { return value_; }
    void setValue(double value) { value_ = value; }
    const std::optional<std::string>& getTypeOfArea() const { return typeOfArea_; }
    void setTypeOfArea(const std::string& type) { typeOfArea_ = type; }

private:
    double value_ = 0.0;
    std::optional<std::string> typeOfArea_;
};

using QualifiedAreaPtr = std::shared_ptr<QualifiedArea>;

class AbstractSpace : public CityGMLFeatureWithLifespan {
public:
    virtual ~AbstractSpace() = default;
    const std::optional<SpaceType>& getSpaceType() const { return spaceType_; }
    void setSpaceType(SpaceType type) { spaceType_ = type; }
    const std::vector<QualifiedVolumePtr>& getVolumes() const { return volumes_; }
    void addVolume(QualifiedVolumePtr vol) { volumes_.push_back(vol); }
    const std::vector<QualifiedAreaPtr>& getAreas() const { return areas_; }
    void addArea(QualifiedAreaPtr area) { areas_.push_back(area); }

private:
    std::optional<SpaceType> spaceType_;
    std::vector<QualifiedVolumePtr> volumes_;
    std::vector<QualifiedAreaPtr> areas_;
};

class AbstractLogicalSpace : public AbstractSpace {
public:
    virtual ~AbstractLogicalSpace() = default;
};

class AbstractPhysicalSpace : public AbstractSpace {
public:
    virtual ~AbstractPhysicalSpace() = default;
    std::shared_ptr<AbstractGeometry> getLod1TerrainIntersectionCurve() const { return lod1TerrainIntersectionCurve_; }
    void setLod1TerrainIntersectionCurve(std::shared_ptr<AbstractGeometry> geom) { lod1TerrainIntersectionCurve_ = geom; }
    std::shared_ptr<AbstractGeometry> getLod2TerrainIntersectionCurve() const { return lod2TerrainIntersectionCurve_; }
    void setLod2TerrainIntersectionCurve(std::shared_ptr<AbstractGeometry> geom) { lod2TerrainIntersectionCurve_ = geom; }
    std::shared_ptr<AbstractGeometry> getLod3TerrainIntersectionCurve() const { return lod3TerrainIntersectionCurve_; }
    void setLod3TerrainIntersectionCurve(std::shared_ptr<AbstractGeometry> geom) { lod3TerrainIntersectionCurve_ = geom; }

private:
    std::shared_ptr<AbstractGeometry> lod1TerrainIntersectionCurve_;
    std::shared_ptr<AbstractGeometry> lod2TerrainIntersectionCurve_;
    std::shared_ptr<AbstractGeometry> lod3TerrainIntersectionCurve_;
};

class AbstractOccupiedSpace : public AbstractPhysicalSpace {
public:
    virtual ~AbstractOccupiedSpace() = default;
    std::shared_ptr<AbstractGeometry> getLod1ImplicitRepresentation() const { return lod1ImplicitRepresentation_; }
    void setLod1ImplicitRepresentation(std::shared_ptr<AbstractGeometry> geom) { lod1ImplicitRepresentation_ = geom; }
    std::shared_ptr<AbstractGeometry> getLod2ImplicitRepresentation() const { return lod2ImplicitRepresentation_; }
    void setLod2ImplicitRepresentation(std::shared_ptr<AbstractGeometry> geom) { lod2ImplicitRepresentation_ = geom; }
    std::shared_ptr<AbstractGeometry> getLod3ImplicitRepresentation() const { return lod3ImplicitRepresentation_; }
    void setLod3ImplicitRepresentation(std::shared_ptr<AbstractGeometry> geom) { lod3ImplicitRepresentation_ = geom; }

private:
    std::shared_ptr<AbstractGeometry> lod1ImplicitRepresentation_;
    std::shared_ptr<AbstractGeometry> lod2ImplicitRepresentation_;
    std::shared_ptr<AbstractGeometry> lod3ImplicitRepresentation_;
};

class AbstractUnoccupiedSpace : public AbstractPhysicalSpace {
public:
    virtual ~AbstractUnoccupiedSpace() = default;
};

} // namespace citygml
