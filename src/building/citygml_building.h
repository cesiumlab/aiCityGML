#pragma once

#include "core/citygml_base.h"
#include "core/citygml_space.h"
#include "core/citygml_geometry.h"

namespace citygml {

// ============================================================
// 前向声明
// ============================================================

class AbstractThematicSurface;
class Address;
class BuildingConstructiveElement;
class BuildingInstallation;
class BuildingFurniture;
class RoomHeight;

// ============================================================
// AbstractBuilding - 建筑基类
// ============================================================

class AbstractBuilding : public AbstractOccupiedSpace {
public:
    virtual ~AbstractBuilding() = default;
    
    const std::optional<Code>& getClass() const { return class_; }
    void setClass(const Code& c) { class_ = c; }
    
    const std::vector<Code>& getFunctions() const { return functions_; }
    void addFunction(const Code& func) { functions_.push_back(func); }
    
    const std::vector<Code>& getUsages() const { return usages_; }
    void addUsage(const Code& usage) { usages_.push_back(usage); }
    
    const std::optional<Code>& getRoofType() const { return roofType_; }
    void setRoofType(const Code& type) { roofType_ = type; }
    
    int getStoreysAboveGround() const { return storeysAboveGround_; }
    void setStoreysAboveGround(int n) { storeysAboveGround_ = n; }
    
    int getStoreysBelowGround() const { return storeysBelowGround_; }
    void setStoreysBelowGround(int n) { storeysBelowGround_ = n; }
    
    const std::vector<double>& getStoreyHeightsAboveGround() const { return storeyHeightsAboveGround_; }
    void addStoreyHeightAboveGround(double h) { storeyHeightsAboveGround_.push_back(h); }
    
    const std::vector<double>& getStoreyHeightsBelowGround() const { return storeyHeightsBelowGround_; }
    void addStoreyHeightBelowGround(double h) { storeyHeightsBelowGround_.push_back(h); }
    
    const std::vector<std::shared_ptr<BuildingConstructiveElement>>& getConstructiveElements() const;
    void addConstructiveElement(std::shared_ptr<BuildingConstructiveElement> elem);
    
    const std::vector<std::shared_ptr<BuildingInstallation>>& getInstallations() const;
    void addInstallation(std::shared_ptr<BuildingInstallation> inst);
    
    const std::vector<std::shared_ptr<AbstractSpace>>& getRooms() const;
    void addRoom(std::shared_ptr<AbstractSpace> room);
    
    const std::vector<std::shared_ptr<BuildingFurniture>>& getFurniture() const;
    void addFurniture(std::shared_ptr<BuildingFurniture> furniture);
    
    const std::vector<std::shared_ptr<Address>>& getAddresses() const;
    void addAddress(std::shared_ptr<Address> addr);
    
private:
    std::optional<Code> class_;
    std::vector<Code> functions_;
    std::vector<Code> usages_;
    std::optional<Code> roofType_;
    int storeysAboveGround_ = 0;
    int storeysBelowGround_ = 0;
    std::vector<double> storeyHeightsAboveGround_;
    std::vector<double> storeyHeightsBelowGround_;
    
    std::vector<std::shared_ptr<BuildingConstructiveElement>> constructiveElements_;
    std::vector<std::shared_ptr<BuildingInstallation>> installations_;
    std::vector<std::shared_ptr<AbstractSpace>> rooms_;
    std::vector<std::shared_ptr<BuildingFurniture>> furniture_;
    std::vector<std::shared_ptr<Address>> addresses_;
};

// ============================================================
// Building - 建筑物
// ============================================================

class Building : public AbstractBuilding {
public:
    virtual ~Building() = default;
    
    const std::vector<std::shared_ptr<Building>>& getBuildingParts() const;
    void addBuildingPart(std::shared_ptr<Building> part);
    
private:
    std::vector<std::shared_ptr<Building>> buildingParts_;
};

using BuildingPtr = std::shared_ptr<Building>;

// ============================================================
// BuildingPart - 建筑部件
// ============================================================

class BuildingPart : public AbstractBuilding {
public:
    virtual ~BuildingPart() = default;
};

using BuildingPartPtr = std::shared_ptr<BuildingPart>;

// ============================================================
// BuildingConstructiveElement - 建筑构件
// ============================================================

class BuildingConstructiveElement : public AbstractOccupiedSpace {
public:
    virtual ~BuildingConstructiveElement() = default;
    
    const std::optional<Code>& getClass() const { return class_; }
    void setClass(const Code& c) { class_ = c; }
    
    const std::vector<Code>& getFunctions() const { return functions_; }
    void addFunction(const Code& func) { functions_.push_back(func); }
    
    const std::vector<Code>& getUsages() const { return usages_; }
    void addUsage(const Code& usage) { usages_.push_back(usage); }
    
private:
    std::optional<Code> class_;
    std::vector<Code> functions_;
    std::vector<Code> usages_;
};

using BuildingConstructiveElementPtr = std::shared_ptr<BuildingConstructiveElement>;

// ============================================================
// BuildingInstallation - 建筑设施
// ============================================================

class BuildingInstallation : public AbstractOccupiedSpace {
public:
    virtual ~BuildingInstallation() = default;
    
    const std::optional<Code>& getClass() const { return class_; }
    void setClass(const Code& c) { class_ = c; }
    
    const std::vector<Code>& getFunctions() const { return functions_; }
    void addFunction(const Code& func) { functions_.push_back(func); }
    
    const std::vector<Code>& getUsages() const { return usages_; }
    void addUsage(const Code& usage) { usages_.push_back(usage); }
    
private:
    std::optional<Code> class_;
    std::vector<Code> functions_;
    std::vector<Code> usages_;
};

using BuildingInstallationPtr = std::shared_ptr<BuildingInstallation>;

// ============================================================
// BuildingFurniture - 建筑家具
// ============================================================

class BuildingFurniture : public AbstractOccupiedSpace {
public:
    virtual ~BuildingFurniture() = default;
    
    const std::optional<Code>& getClass() const { return class_; }
    void setClass(const Code& c) { class_ = c; }
    
    const std::vector<Code>& getFunctions() const { return functions_; }
    void addFunction(const Code& func) { functions_.push_back(func); }
    
    const std::vector<Code>& getUsages() const { return usages_; }
    void addUsage(const Code& usage) { usages_.push_back(usage); }
    
private:
    std::optional<Code> class_;
    std::vector<Code> functions_;
    std::vector<Code> usages_;
};

using BuildingFurniturePtr = std::shared_ptr<BuildingFurniture>;

// ============================================================
// AbstractBuildingSubdivision - 建筑逻辑细分
// ============================================================

class AbstractBuildingSubdivision : public AbstractLogicalSpace {
public:
    virtual ~AbstractBuildingSubdivision() = default;
    
    const std::optional<Code>& getClass() const { return class_; }
    void setClass(const Code& c) { class_ = c; }
    
    const std::vector<Code>& getFunctions() const { return functions_; }
    void addFunction(const Code& func) { functions_.push_back(func); }
    
    const std::vector<Code>& getUsages() const { return usages_; }
    void addUsage(const Code& usage) { usages_.push_back(usage); }
    
    double getSortKey() const { return sortKey_; }
    void setSortKey(double key) { sortKey_ = key; }
    
    const std::vector<BuildingFurniturePtr>& getFurniture() const;
    void addFurniture(BuildingFurniturePtr furniture);
    
    const std::vector<BuildingInstallationPtr>& getInstallations() const;
    void addInstallation(BuildingInstallationPtr inst);
    
private:
    std::optional<Code> class_;
    std::vector<Code> functions_;
    std::vector<Code> usages_;
    double sortKey_ = 0.0;
    
    std::vector<BuildingFurniturePtr> furniture_;
    std::vector<BuildingInstallationPtr> installations_;
};

// ============================================================
// Storey - 楼层
// ============================================================

class Storey : public AbstractBuildingSubdivision {
public:
    virtual ~Storey() = default;
    
    const std::vector<std::shared_ptr<AbstractBuildingSubdivision>>& getBuildingUnits() const;
    void addBuildingUnit(std::shared_ptr<AbstractBuildingSubdivision> unit);
    
private:
    std::vector<std::shared_ptr<AbstractBuildingSubdivision>> buildingUnits_;
};

using StoreyPtr = std::shared_ptr<Storey>;

// ============================================================
// BuildingUnit - 建筑单元
// ============================================================

class BuildingUnit : public AbstractBuildingSubdivision {
public:
    virtual ~BuildingUnit() = default;
    
    const std::vector<std::shared_ptr<Storey>>& getStoreys() const;
    void addStorey(std::shared_ptr<Storey> storey);
    
    const std::vector<std::shared_ptr<Address>>& getAddresses() const;
    void addAddress(std::shared_ptr<Address> addr);
    
private:
    std::vector<std::shared_ptr<Storey>> storeys_;
    std::vector<std::shared_ptr<Address>> addresses_;
};

using BuildingUnitPtr = std::shared_ptr<BuildingUnit>;

// ============================================================
// RoomHeight - 房间高度
// ============================================================

class RoomHeight : public CityGMLObject {
public:
    const std::string& getHighReference() const { return highReference_; }
    void setHighReference(const std::string& ref) { highReference_ = ref; }
    
    const std::string& getLowReference() const { return lowReference_; }
    void setLowReference(const std::string& ref) { lowReference_ = ref; }
    
    const std::string& getStatus() const { return status_; }
    void setStatus(const std::string& s) { status_ = s; }
    
    double getValue() const { return value_; }
    void setValue(double v) { value_ = v; }
    
private:
    std::string highReference_;
    std::string lowReference_;
    std::string status_;
    double value_ = 0.0;
};

using RoomHeightPtr = std::shared_ptr<RoomHeight>;

// ============================================================
// BuildingRoom - 建筑房间
// ============================================================

class BuildingRoom : public AbstractUnoccupiedSpace {
public:
    virtual ~BuildingRoom() = default;
    
    const std::optional<Code>& getClass() const { return class_; }
    void setClass(const Code& c) { class_ = c; }
    
    const std::vector<Code>& getFunctions() const { return functions_; }
    void addFunction(const Code& func) { functions_.push_back(func); }
    
    const std::vector<Code>& getUsages() const { return usages_; }
    void addUsage(const Code& usage) { usages_.push_back(usage); }
    
    const std::vector<RoomHeightPtr>& getRoomHeights() const;
    void addRoomHeight(RoomHeightPtr height);
    
    const std::vector<BuildingFurniturePtr>& getFurniture() const;
    void addFurniture(BuildingFurniturePtr furniture);
    
    const std::vector<BuildingInstallationPtr>& getInstallations() const;
    void addInstallation(BuildingInstallationPtr inst);
    
private:
    std::optional<Code> class_;
    std::vector<Code> functions_;
    std::vector<Code> usages_;
    std::vector<RoomHeightPtr> roomHeights_;
    std::vector<BuildingFurniturePtr> furniture_;
    std::vector<BuildingInstallationPtr> installations_;
};

using BuildingRoomPtr = std::shared_ptr<BuildingRoom>;

// ============================================================
// Address - 地址
// ============================================================

class Address : public CityGMLFeature {
public:
    virtual ~Address() = default;
    
    const std::string& getXalAddress() const { return xalAddress_; }
    void setXalAddress(const std::string& addr) { xalAddress_ = addr; }
    
private:
    std::string xalAddress_;
};

using AddressPtr = std::shared_ptr<Address>;

} // namespace citygml