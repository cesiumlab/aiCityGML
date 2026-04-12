#include "building/citygml_building.h"

namespace citygml {

// AbstractBuilding 方法实现
const std::vector<std::shared_ptr<BuildingConstructiveElement>>&
AbstractBuilding::getConstructiveElements() const {
    return constructiveElements_;
}

void AbstractBuilding::addConstructiveElement(std::shared_ptr<BuildingConstructiveElement> elem) {
    constructiveElements_.push_back(elem);
}

const std::vector<std::shared_ptr<BuildingInstallation>>&
AbstractBuilding::getInstallations() const {
    return installations_;
}

void AbstractBuilding::addInstallation(std::shared_ptr<BuildingInstallation> inst) {
    installations_.push_back(inst);
}

const std::vector<std::shared_ptr<AbstractSpace>>&
AbstractBuilding::getRooms() const {
    return rooms_;
}

void AbstractBuilding::addRoom(std::shared_ptr<AbstractSpace> room) {
    rooms_.push_back(room);
}

const std::vector<std::shared_ptr<BuildingFurniture>>&
AbstractBuilding::getFurniture() const {
    return furniture_;
}

void AbstractBuilding::addFurniture(std::shared_ptr<BuildingFurniture> furniture) {
    furniture_.push_back(furniture);
}

const std::vector<std::shared_ptr<Address>>&
AbstractBuilding::getAddresses() const {
    return addresses_;
}

void AbstractBuilding::addAddress(std::shared_ptr<Address> addr) {
    addresses_.push_back(addr);
}

// Building 方法实现
const std::vector<std::shared_ptr<Building>>&
Building::getBuildingParts() const {
    return buildingParts_;
}

void Building::addBuildingPart(std::shared_ptr<Building> part) {
    buildingParts_.push_back(part);
}

// AbstractBuildingSubdivision 方法实现
const std::vector<BuildingFurniturePtr>&
AbstractBuildingSubdivision::getFurniture() const {
    return furniture_;
}

void AbstractBuildingSubdivision::addFurniture(BuildingFurniturePtr furniture) {
    furniture_.push_back(furniture);
}

const std::vector<BuildingInstallationPtr>&
AbstractBuildingSubdivision::getInstallations() const {
    return installations_;
}

void AbstractBuildingSubdivision::addInstallation(BuildingInstallationPtr inst) {
    installations_.push_back(inst);
}

// Storey 方法实现
const std::vector<std::shared_ptr<AbstractBuildingSubdivision>>&
Storey::getBuildingUnits() const {
    return buildingUnits_;
}

void Storey::addBuildingUnit(std::shared_ptr<AbstractBuildingSubdivision> unit) {
    buildingUnits_.push_back(unit);
}

// BuildingUnit 方法实现
const std::vector<std::shared_ptr<Storey>>&
BuildingUnit::getStoreys() const {
    return storeys_;
}

void BuildingUnit::addStorey(std::shared_ptr<Storey> storey) {
    storeys_.push_back(storey);
}

const std::vector<std::shared_ptr<Address>>&
BuildingUnit::getAddresses() const {
    return addresses_;
}

void BuildingUnit::addAddress(std::shared_ptr<Address> addr) {
    addresses_.push_back(addr);
}

// BuildingRoom 方法实现
const std::vector<RoomHeightPtr>&
BuildingRoom::getRoomHeights() const {
    return roomHeights_;
}

void BuildingRoom::addRoomHeight(RoomHeightPtr height) {
    roomHeights_.push_back(height);
}

const std::vector<BuildingFurniturePtr>&
BuildingRoom::getFurniture() const {
    return furniture_;
}

void BuildingRoom::addFurniture(BuildingFurniturePtr furniture) {
    furniture_.push_back(furniture);
}

const std::vector<BuildingInstallationPtr>&
BuildingRoom::getInstallations() const {
    return installations_;
}

void BuildingRoom::addInstallation(BuildingInstallationPtr inst) {
    installations_.push_back(inst);
}

} // namespace citygml