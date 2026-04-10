#include <iostream>
#include <memory>
#include <iomanip>
#include "citygml/citygml_parser.h"
#include "citygml/core/citygml_geometry.h"
#include "citygml/core/citygml_object.h"
#include "citygml/core/citygml_base.h"
#include "citygml/building/citygml_building.h"

using namespace citygml;

std::string getRelativeToTerrainStr(const std::optional<RelativeToTerrain>& rel) {
    if (!rel.has_value()) return "(none)";
    switch (*rel) {
        case RelativeToTerrain::EntirelyAboveTerrain: return "entirelyAboveTerrain";
        case RelativeToTerrain::SubstantiallyAboveTerrain: return "substantiallyAboveTerrain";
        case RelativeToTerrain::SubstantiallyAboveAndBelowTerrain: return "substantiallyAboveAndBelowTerrain";
        case RelativeToTerrain::SubstantiallyBelowTerrain: return "substantiallyBelowTerrain";
        case RelativeToTerrain::EntirelyBelowTerrain: return "entirelyBelowTerrain";
        default: return "unknown";
    }
}

void printEnvelope(const Envelope& env, const std::string& prefix = "") {
    std::cout << prefix << "SRS: " << (env.srsName.empty() ? "(none)" : env.srsName) << std::endl;
    std::cout << prefix << "Lower: " << std::fixed << std::setprecision(3)
              << env.lowerCorner[0] << ", " << env.lowerCorner[1] << ", " << env.lowerCorner[2] << std::endl;
    std::cout << prefix << "Upper: " << env.upperCorner[0] << ", " << env.upperCorner[1] << ", " << env.upperCorner[2] << std::endl;
}

int main(int argc, char* argv[]) {
    std::string filePath;
    if (argc > 1) {
        filePath = argv[1];
    } else {
        std::cerr << "Usage: " << argv[0] << " <citygml_file.gml>" << std::endl;
        return 1;
    }

    std::cout << "============================================================" << std::endl;
    std::cout << "          CityGML Parser - Full Verification Report           " << std::endl;
    std::cout << "============================================================" << std::endl << std::endl;
    std::cout << "File: " << filePath << std::endl << std::endl;

    CityGMLParser parser;
    std::shared_ptr<CityModel> cityModel;

    ParseResult result = parser.parse(filePath, cityModel);

    if (!result.success) {
        std::cerr << "[ERROR] Parse failed: " << result.errorMessage << std::endl;
        return 1;
    }

    std::cout << "[OK] Parse completed successfully!" << std::endl << std::endl;

    if (!cityModel) {
        std::cout << "[ERROR] No city model!" << std::endl;
        return 1;
    }

    // ========================================
    // 1. CityModel 信息
    // ========================================
    std::cout << "============================================================" << std::endl;
    std::cout << "  1. CityModel" << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "  ID: " << (cityModel->getId().empty() ? "(none)" : cityModel->getId()) << std::endl;
    std::cout << "  Name: " << (cityModel->getName().empty() ? "(none)" : cityModel->getName()) << std::endl;

    const auto& optEnv = cityModel->getEnvelope();
    if (optEnv.has_value()) {
        std::cout << "  Envelope:" << std::endl;
        printEnvelope(*optEnv, "    ");
    } else {
        std::cout << "  Envelope: (none)" << std::endl;
    }

    // ========================================
    // 2. City Objects
    // ========================================
    auto& objects = cityModel->getCityObjects();
    std::cout << std::endl << "============================================================" << std::endl;
    std::cout << "  2. City Objects (" << objects.size() << " found)" << std::endl;
    std::cout << "============================================================" << std::endl;

    for (size_t i = 0; i < objects.size(); ++i) {
        auto obj = objects[i];
        std::cout << std::endl << "  --- Object " << (i + 1) << " ---" << std::endl;
        std::cout << "    ObjectType: " << obj->getObjectType() << std::endl;
        std::cout << "    ID: " << (obj->getId().empty() ? "(none)" : obj->getId()) << std::endl;
        std::cout << "    Name: " << (obj->getName().empty() ? "(none)" : obj->getName()) << std::endl;
        std::cout << "    Description: " << (obj->getDescription().empty() ? "(none)" : obj->getDescription()) << std::endl;

        // CreationDate
        const auto& creationDate = obj->getCreationDate();
        if (creationDate.has_value()) {
            std::cout << "    CreationDate: " << creationDate->year << "-"
                      << std::setw(2) << std::setfill('0') << creationDate->month << "-"
                      << std::setw(2) << std::setfill('0') << creationDate->day << std::endl;
        } else {
            std::cout << "    CreationDate: (none)" << std::endl;
        }

        // RelativeToTerrain
        std::cout << "    RelativeToTerrain: " << getRelativeToTerrainStr(obj->getRelativeToTerrain()) << std::endl;

        // Storeys
        std::cout << "    StoreysAboveGround: " << obj->getStoreysAboveGround() << std::endl;
        std::cout << "    StoreysBelowGround: " << obj->getStoreysBelowGround() << std::endl;

        // Generic Attributes
        const auto& attrs = obj->getGenericAttributes();
        if (!attrs.empty()) {
            std::cout << "    Generic Attributes (" << attrs.size() << "):" << std::endl;
            for (const auto& attr : attrs) {
                std::cout << "      - " << attr->getName() << ": " << attr->getValueAsString() << std::endl;
            }
        } else {
            std::cout << "    Generic Attributes: (none)" << std::endl;
        }

        // LOD Geometries
        std::cout << "    LOD Geometries:" << std::endl;
        bool hasGeom = false;
        for (int lod = 0; lod <= 4; ++lod) {
            auto geom = obj->getLODGeometry(lod);
            if (geom) {
                hasGeom = true;
                GeometryType type = geom->getType();
                std::string typeName;
                switch (type) {
                    case GeometryType::GT_Polygon: typeName = "Polygon"; break;
                    case GeometryType::GT_MultiSurface: typeName = "MultiSurface"; break;
                    case GeometryType::GT_Solid: typeName = "Solid"; break;
                    case GeometryType::GT_CompositeSolid: typeName = "CompositeSolid"; break;
                    default: typeName = "Other"; break;
                }
                std::cout << "      LOD" << lod << ": " << typeName
                          << " (SRS: " << (geom->getSrsName().empty() ? "none" : geom->getSrsName()) << ")" << std::endl;
            }
        }
        if (!hasGeom) {
            std::cout << "      (none)" << std::endl;
        }

        // Object Envelope
        const auto& objEnv = obj->getEnvelope();
        if (objEnv.has_value()) {
            std::cout << "    Object Envelope:" << std::endl;
            printEnvelope(*objEnv, "      ");
        }
    }

    // ========================================
    // 3. Statistics
    // ========================================
    std::cout << std::endl << "============================================================" << std::endl;
    std::cout << "  3. Statistics" << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "  Total City Objects: " << objects.size() << std::endl;

    int lodCounts[5] = {0, 0, 0, 0, 0};
    int totalPolygons = 0;
    size_t totalPoints = 0;

    for (auto& obj : objects) {
        for (int lod = 0; lod <= 4; ++lod) {
            auto geom = obj->getLODGeometry(lod);
            if (geom) {
                lodCounts[lod]++;
                std::function<void(AbstractGeometryPtr)> countGeom = [&](AbstractGeometryPtr g) {
                    if (!g) return;
                    switch (g->getType()) {
                        case GeometryType::GT_Polygon: {
                            auto poly = std::dynamic_pointer_cast<Polygon>(g);
                            if (poly && poly->getExteriorRing()) {
                                totalPolygons++;
                                totalPoints += poly->getExteriorRing()->getPointsCount();
                            }
                            break;
                        }
                        case GeometryType::GT_MultiSurface:
                        case GeometryType::GT_CompositeSurface: {
                            auto ms = std::dynamic_pointer_cast<MultiSurface>(g);
                            if (ms) {
                                for (size_t j = 0; j < ms->getGeometriesCount(); ++j) {
                                    countGeom(ms->getGeometry(j));
                                }
                            }
                            break;
                        }
                        case GeometryType::GT_Solid:
                        case GeometryType::GT_CompositeSolid: {
                            auto solid = std::dynamic_pointer_cast<Solid>(g);
                            if (solid) {
                                auto shell = solid->getOuterShell();
                                if (shell) {
                                    for (size_t j = 0; j < shell->getGeometriesCount(); ++j) {
                                        countGeom(shell->getGeometry(j));
                                    }
                                }
                            }
                            break;
                        }
                        default: break;
                    }
                };
                countGeom(geom);
            }
        }
    }

    std::cout << "  LOD0 Objects: " << lodCounts[0] << std::endl;
    std::cout << "  LOD1 Objects: " << lodCounts[1] << std::endl;
    std::cout << "  LOD2 Objects: " << lodCounts[2] << std::endl;
    std::cout << "  LOD3 Objects: " << lodCounts[3] << std::endl;
    std::cout << "  LOD4 Objects: " << lodCounts[4] << std::endl;
    std::cout << "  Total Polygons: " << totalPolygons << std::endl;
    std::cout << "  Total Points: " << totalPoints << std::endl;

    std::cout << std::endl << "============================================================" << std::endl;
    std::cout << "  Verification Complete!" << std::endl;
    std::cout << "============================================================" << std::endl;

    return 0;
}
