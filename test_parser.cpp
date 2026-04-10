#include <iostream>
#include <memory>
#include "citygml/citygml_parser.h"
#include "citygml/core/citygml_geometry.h"
#include "citygml/core/citygml_object.h"

using namespace citygml;

void printEnvelope(const Envelope& env) {
    std::cout << "  Envelope:" << std::endl;
    std::cout << "    SRS: " << env.srsName << std::endl;
    std::cout << "    Lower: " << env.lowerCorner[0] << ", " << env.lowerCorner[1] << ", " << env.lowerCorner[2] << std::endl;
    std::cout << "    Upper: " << env.upperCorner[0] << ", " << env.upperCorner[1] << ", " << env.upperCorner[2] << std::endl;
}

void printGeometry(std::shared_ptr<AbstractGeometry> geom, int indent = 2) {
    if (!geom) {
        std::cout << "  (null geometry)" << std::endl;
        return;
    }

    std::string prefix(indent, ' ');

    GeometryType type = geom->getType();

    if (!geom->getSrsName().empty()) {
        std::cout << prefix << "SRS: " << geom->getSrsName() << std::endl;
    }

    switch (type) {
        case GeometryType::GT_MultiSurface:
        case GeometryType::GT_CompositeSurface: {
            auto ms = std::dynamic_pointer_cast<MultiSurface>(geom);
            std::cout << prefix << "MultiSurface (polygons: " << ms->getGeometriesCount() << ")" << std::endl;
            size_t showCount = std::min(ms->getGeometriesCount(), static_cast<size_t>(3));
            for (size_t i = 0; i < showCount; ++i) {
                auto poly = ms->getGeometry(i);
                if (poly) {
                    auto ring = poly->getExteriorRing();
                    if (ring) {
                        std::cout << prefix << "  Polygon " << i << ": " << ring->getPointsCount() << " points" << std::endl;
                    }
                }
            }
            if (ms->getGeometriesCount() > 3) {
                std::cout << prefix << "  ... and " << (ms->getGeometriesCount() - 3) << " more polygons" << std::endl;
            }
            break;
        }
        case GeometryType::GT_Polygon: {
            auto poly = std::dynamic_pointer_cast<Polygon>(geom);
            auto ring = poly->getExteriorRing();
            if (ring) {
                std::cout << prefix << "Polygon: " << ring->getPointsCount() << " points" << std::endl;
            } else {
                std::cout << prefix << "Polygon: no exterior ring" << std::endl;
            }
            break;
        }
        case GeometryType::GT_Solid:
        case GeometryType::GT_CompositeSolid: {
            auto solid = std::dynamic_pointer_cast<Solid>(geom);
            auto shell = solid->getOuterShell();
            if (shell) {
                std::cout << prefix << "Solid with shell: " << shell->getGeometriesCount() << " surfaces" << std::endl;
            }
            break;
        }
        default:
            std::cout << prefix << "Geometry type: " << static_cast<int>(type) << std::endl;
            break;
    }
}

int main(int argc, char* argv[]) {
    std::string filePath = "testdata/2.gml";
    if (argc > 1) {
        filePath = argv[1];
    }
    
    std::cout << "=== CityGML Parser Test ===" << std::endl;
    std::cout << "File: " << filePath << std::endl << std::endl;
    
    CityGMLParser parser;
    std::shared_ptr<CityModel> cityModel;
    
    ParseResult result = parser.parse(filePath, cityModel);
    
    if (!result.success) {
        std::cerr << "Parse error: " << result.errorMessage << std::endl;
        return 1;
    }
    
    std::cout << "Parse success!" << std::endl << std::endl;
    
    if (cityModel) {
        auto env = cityModel->getEnvelope();
        if (env) {
            printEnvelope(*env);
            std::cout << std::endl;
        }
        
        auto& objects = cityModel->getCityObjects();
        std::cout << "City objects: " << objects.size() << std::endl << std::endl;
        
        size_t showCount = std::min(objects.size(), static_cast<size_t>(5));
        for (size_t i = 0; i < showCount; ++i) {
            auto obj = objects[i];
            std::cout << "Object " << (i + 1) << ":" << std::endl;
            
            bool hasGeometry = false;
            for (int lod = 1; lod <= 4; ++lod) {
                auto geom = obj->getLODGeometry(lod);
                if (geom) {
                    hasGeometry = true;
                    std::cout << "  LOD" << lod << ": ";
                    printGeometry(geom, 4);
                }
            }
            if (!hasGeometry) {
                std::cout << "  (no LOD geometry)" << std::endl;
            }
            std::cout << std::endl;
        }
        
        if (objects.size() > 5) {
            std::cout << "... and " << (objects.size() - 5) << " more objects" << std::endl;
        }
    }
    
    std::cout << std::endl << "Test completed successfully!" << std::endl;
    
    return 0;
}