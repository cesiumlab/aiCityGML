#pragma once

#include "citygml/core/citygml_feature.h"
#include "citygml/core/citygml_appearance.h"
#include "citygml/core/citygml_geometry.h"

namespace citygml {

class AbstractAppearance;

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

    std::shared_ptr<AbstractGeometry> getLODGeometry(int lod) const {
        auto it = lodGeometries_.find(lod);
        if (it != lodGeometries_.end()) {
            return it->second;
        }
        return nullptr;
    }

    void setLODGeometry(int lod, std::shared_ptr<AbstractGeometry> geom) {
        lodGeometries_[lod] = geom;
    }

private:
    std::string type_;
    std::optional<RelativeToTerrain> relativeToTerrain_;
    std::optional<RelativeToWater> relativeToWater_;
    std::optional<Envelope> envelope_;
    std::vector<std::shared_ptr<AbstractCityObject>> generalizesTo_;
    int storeysAboveGround_ = 0;
    int storeysBelowGround_ = 0;
    std::map<int, std::shared_ptr<AbstractGeometry>> lodGeometries_;
};

using CityObjectPtr = std::shared_ptr<AbstractCityObject>;
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