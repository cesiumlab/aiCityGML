#pragma once

#include "citygml/core/citygml_feature.h"
#include "citygml/core/citygml_appearance.h"

namespace citygml {

// ============================================================
// 前向声明
// ============================================================

class AbstractAppearance;

// ============================================================
// AbstractCityObject - 所有城市对象的抽象基类
// ============================================================

class AbstractCityObject : public CityGMLFeature {
public:
    virtual ~AbstractCityObject() = default;
    
    const std::optional<RelativeToTerrain>& getRelativeToTerrain() const { return relativeToTerrain_; }
    void setRelativeToTerrain(RelativeToTerrain value) { relativeToTerrain_ = value; }
    
    const std::optional<RelativeToWater>& getRelativeToWater() const { return relativeToWater_; }
    void setRelativeToWater(RelativeToWater value) { relativeToWater_ = value; }
    
    const std::vector<std::shared_ptr<AbstractCityObject>>& getGeneralizesTo() const { return generalizesTo_; }
    void addGeneralization(std::shared_ptr<AbstractCityObject> obj) { generalizesTo_.push_back(obj); }
    
private:
    std::optional<RelativeToTerrain> relativeToTerrain_;
    std::optional<RelativeToWater> relativeToWater_;
    std::vector<std::shared_ptr<AbstractCityObject>> generalizesTo_;
};

using CityObjectPtr = std::shared_ptr<AbstractCityObject>;

// ============================================================
// CityModel - 城市模型（根容器）
// ============================================================

class CityModel : public CityGMLFeatureWithLifespan {
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