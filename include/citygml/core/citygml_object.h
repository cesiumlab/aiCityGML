#pragma once

#include "citygml/core/citygml_feature.h"
#include "citygml/core/citygml_appearance.h"
#include "citygml/core/citygml_geometry.h"
#include "citygml/building/citygml_building.h"

namespace citygml {

class AbstractAppearance;
class AbstractThematicSurface;
class AbstractOpening;
class AbstractCityObject;

// 类型别名提前声明
using CityObjectPtr = std::shared_ptr<AbstractCityObject>;

class AbstractCityObject : public CityGMLFeature {
public:
    AbstractCityObject() = default;
    explicit AbstractCityObject(const std::string& type) : type_(type) {}

    virtual ~AbstractCityObject() = default;

    const std::string& getObjectType() const { return type_; }

    const std::optional<RelativeToTerrain>& getRelativeToTerrain() const { return relativeToTerrain_; }
    void setRelativeToTerrain(RelativeToTerrain value) { relativeToTerrain_ = value; }

    const std::optional<RelativeToWater>& getRelativeToWater() const { return relativeToWater_; }
    void setRelativeToWater(RelativeToWater value) { relativeToWater_ = value; }

    const std::optional<Envelope>& getEnvelope() const { return envelope_; }
    void setEnvelope(const Envelope& env) { envelope_ = env; }

    const std::vector<std::shared_ptr<AbstractCityObject>>& getGeneralizesTo() const { return generalizesTo_; }
    void addGeneralization(std::shared_ptr<AbstractCityObject> obj) { generalizesTo_.push_back(obj); }

    int getStoreysAboveGround() const { return storeysAboveGround_; }
    void setStoreysAboveGround(int n) { storeysAboveGround_ = n; }

    int getStoreysBelowGround() const { return storeysBelowGround_; }
    void setStoreysBelowGround(int n) { storeysBelowGround_ = n; }

    // 建筑特有属性
    const std::optional<std::string>& getBuildingClass() const { return class_; }
    void setBuildingClass(const std::string& c) { class_ = c; }

    const std::optional<std::string>& getBuildingFunction() const { return function_; }
    void setBuildingFunction(const std::string& f) { function_ = f; }

    const std::optional<std::string>& getBuildingUsage() const { return usage_; }
    void setBuildingUsage(const std::string& u) { usage_ = u; }

    const std::optional<std::string>& getRoofType() const { return roofType_; }
    void setRoofType(const std::string& r) { roofType_ = r; }

    const std::optional<double>& getMeasuredHeight() const { return measuredHeight_; }
    void setMeasuredHeight(double h) { measuredHeight_ = h; }

    const std::optional<int>& getYearOfConstruction() const { return yearOfConstruction_; }
    void setYearOfConstruction(int y) { yearOfConstruction_ = y; }

    // ================================================================
    // LOD 0 - 只有 MultiSurface（不允许 Solid）
    // ================================================================
    std::shared_ptr<MultiSurface> getLod0FootPrint() const { return lod0FootPrint_; }
    void setLod0FootPrint(MultiSurfacePtr geom) { lod0FootPrint_ = geom; }

    std::shared_ptr<MultiSurface> getLod0RoofEdge() const { return lod0RoofEdge_; }
    void setLod0RoofEdge(MultiSurfacePtr geom) { lod0RoofEdge_ = geom; }

    std::shared_ptr<MultiSurface> getLod0MultiSurface() const { return lod0MultiSurface_; }
    void setLod0MultiSurface(MultiSurfacePtr geom) { lod0MultiSurface_ = geom; }

    // ================================================================
    // LOD 1 - MultiSurface + Solid
    // ================================================================
    std::shared_ptr<MultiSurface> getLod1MultiSurface() const { return lod1MultiSurface_; }
    void setLod1MultiSurface(MultiSurfacePtr geom) { lod1MultiSurface_ = geom; }

    std::shared_ptr<Solid> getLod1Solid() const { return lod1Solid_; }
    void setLod1Solid(SolidPtr geom) { lod1Solid_ = geom; }

    // ================================================================
    // LOD 2 - MultiSurface + Solid
    // ================================================================
    std::shared_ptr<MultiSurface> getLod2MultiSurface() const { return lod2MultiSurface_; }
    void setLod2MultiSurface(MultiSurfacePtr geom) { lod2MultiSurface_ = geom; }

    std::shared_ptr<Solid> getLod2Solid() const { return lod2Solid_; }
    void setLod2Solid(SolidPtr geom) { lod2Solid_ = geom; }

    // ================================================================
    // LOD 3 - MultiSurface + Solid
    // ================================================================
    std::shared_ptr<MultiSurface> getLod3MultiSurface() const { return lod3MultiSurface_; }
    void setLod3MultiSurface(MultiSurfacePtr geom) { lod3MultiSurface_ = geom; }

    std::shared_ptr<Solid> getLod3Solid() const { return lod3Solid_; }
    void setLod3Solid(SolidPtr geom) { lod3Solid_ = geom; }

    // ================================================================
    // LOD 4 - MultiSurface + Solid
    // ================================================================
    std::shared_ptr<MultiSurface> getLod4MultiSurface() const { return lod4MultiSurface_; }
    void setLod4MultiSurface(MultiSurfacePtr geom) { lod4MultiSurface_ = geom; }

    std::shared_ptr<Solid> getLod4Solid() const { return lod4Solid_; }
    void setLod4Solid(SolidPtr geom) { lod4Solid_ = geom; }

    // 主题表面（bldg:boundedBy 中的面）
    const std::vector<std::shared_ptr<AbstractThematicSurface>>& getBoundedBySurfaces() const { return boundedBy_; }
    void addBoundedBySurface(std::shared_ptr<AbstractThematicSurface> surface) { boundedBy_.push_back(surface); }

    // 开口（bldg:opening 中的门/窗）
    const std::vector<std::shared_ptr<AbstractOpening>>& getOpenings() const { return openings_; }
    void addOpening(std::shared_ptr<AbstractOpening> opening) { openings_.push_back(opening); }

    // 子城市对象（如 bldg:interiorRoom）
    const std::vector<CityObjectPtr>& getChildCityObjects() const { return childCityObjects_; }
    void addChildCityObject(CityObjectPtr obj) { childCityObjects_.push_back(obj); }

    // 地址列表（bldg:address）
    const std::vector<std::shared_ptr<Address>>& getAddresses() const { return addresses_; }
    void addAddress(std::shared_ptr<Address> addr) { addresses_.push_back(addr); }

private:
    std::string type_;
    std::optional<RelativeToTerrain> relativeToTerrain_;
    std::optional<RelativeToWater> relativeToWater_;
    std::optional<Envelope> envelope_;
    std::vector<std::shared_ptr<AbstractCityObject>> generalizesTo_;
    int storeysAboveGround_ = 0;
    int storeysBelowGround_ = 0;
    // 建筑特有属性
    std::optional<std::string> class_;
    std::optional<std::string> function_;
    std::optional<std::string> usage_;
    std::optional<std::string> roofType_;
    std::optional<double> measuredHeight_;
    std::optional<int> yearOfConstruction_;

    // LOD 几何体 - 扁平化存储
    // LOD 0
    MultiSurfacePtr lod0FootPrint_;
    MultiSurfacePtr lod0RoofEdge_;
    MultiSurfacePtr lod0MultiSurface_;
    // LOD 1
    MultiSurfacePtr lod1MultiSurface_;
    SolidPtr lod1Solid_;
    // LOD 2
    MultiSurfacePtr lod2MultiSurface_;
    SolidPtr lod2Solid_;
    // LOD 3
    MultiSurfacePtr lod3MultiSurface_;
    SolidPtr lod3Solid_;
    // LOD 4
    MultiSurfacePtr lod4MultiSurface_;
    SolidPtr lod4Solid_;

    std::vector<std::shared_ptr<AbstractThematicSurface>> boundedBy_;
    std::vector<std::shared_ptr<AbstractOpening>> openings_;
    std::vector<CityObjectPtr> childCityObjects_;
    std::vector<std::shared_ptr<Address>> addresses_;
};

using CityObject = AbstractCityObject;

class CityModel : public CityGMLFeature {
public:
    virtual ~CityModel() = default;

    const std::optional<Envelope>& getEnvelope() const { return envelope_; }
    void setEnvelope(const Envelope& env) { envelope_ = env; }

    const std::vector<CityObjectPtr>& getCityObjects() const { return cityObjects_; }
    void addCityObject(CityObjectPtr obj) { cityObjects_.push_back(obj); }

    const std::vector<AppearancePtr>& getAppearances() const { return appearances_; }
    void addAppearance(AppearancePtr app) { appearances_.push_back(app); }

private:
    std::optional<Envelope> envelope_;
    std::vector<CityObjectPtr> cityObjects_;
    std::vector<AppearancePtr> appearances_;
};

using CityModelPtr = std::shared_ptr<CityModel>;

} // namespace citygml
