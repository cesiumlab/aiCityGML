#include <iostream>
#include <memory>
#include <iomanip>
#include "citygml/citygml_parser.h"
#include "citygml/core/citygml_geometry.h"
#include "citygml/core/citygml_object.h"
#include "citygml/core/citygml_base.h"
#include "citygml/core/citygml_appearance.h"
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
    // 1b. Appearances
    // ========================================
    const auto& appearances = cityModel->getAppearances();
    std::cout << std::endl << "  Appearances (" << appearances.size() << " found)" << std::endl;
    if (appearances.empty()) {
        std::cout << "    (none)" << std::endl;
    } else {
        for (size_t i = 0; i < appearances.size(); ++i) {
            auto& app = appearances[i];
            std::cout << std::endl << "    --- Appearance " << (i + 1) << " ---" << std::endl;
            std::cout << "      ID: " << (app->getId().empty() ? "(none)" : app->getId()) << std::endl;
            std::cout << "      Theme: " << (app->getTheme().has_value() ? *app->getTheme() : "(none)") << std::endl;

            const auto& surfaceDataList = app->getSurfaceData();
            std::cout << "      SurfaceData (" << surfaceDataList.size() << "):" << std::endl;
            for (size_t j = 0; j < surfaceDataList.size(); ++j) {
                auto& sd = surfaceDataList[j];
                std::cout << "        --- SurfaceData " << (j + 1) << " ---" << std::endl;
                std::cout << "          ID: " << (sd->getId().empty() ? "(none)" : sd->getId()) << std::endl;
                std::cout << "          Theme: " << (sd->getTheme().has_value() ? *sd->getTheme() : "(none)") << std::endl;

                auto mat = std::dynamic_pointer_cast<X3DMaterial>(sd);
                if (mat) {
                    std::cout << "          Type: X3DMaterial" << std::endl;
                    auto& dc = mat->getDiffuseColor();
                    std::cout << std::fixed << std::setprecision(4)
                              << "          DiffuseColor: (" << dc[0] << ", " << dc[1] << ", " << dc[2] << ", " << dc[3] << ")" << std::endl;
                    std::cout << "          Transparency: " << mat->getTransparency() << std::endl;
                    std::cout << "          Shininess: " << mat->getShininess() << std::endl;
                    std::cout << "          IsSmooth: " << (mat->getIsSmooth() ? "true" : "false") << std::endl;
                    const auto& targets = mat->getTargets();
                    std::cout << "          Targets (" << targets.size() << "):" << std::endl;
                    for (const auto& t : targets) {
                        std::cout << "            - " << t << std::endl;
                    }
                }

                auto tex = std::dynamic_pointer_cast<ParameterizedTexture>(sd);
                if (tex) {
                    std::cout << "          Type: ParameterizedTexture" << std::endl;
                    std::cout << "          ImageURI: " << (tex->getImageURI().empty() ? "(none)" : tex->getImageURI()) << std::endl;
                    std::cout << "          MimeType: " << (tex->getMimeType().empty() ? "(none)" : tex->getMimeType()) << std::endl;
                    const auto& texTargets = tex->getTargets();
                    std::cout << "          Targets (" << texTargets.size() << "):" << std::endl;
                    for (const auto& tt : texTargets) {
                        std::cout << "            - URI: " << tt.uri << std::endl;
                        for (const auto& tc : tt.textureCoords) {
                            std::cout << "              ring: " << tc.ringId
                                      << " coords: (" << tc.coordinates.size() << " points)";
                            if (!tc.coordinates.empty()) {
                                std::cout << " first=(" << tc.coordinates[0][0] << "," << tc.coordinates[0][1] << ")";
                            }
                            std::cout << std::endl;
                        }
                    }
                }
            }
        }
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
