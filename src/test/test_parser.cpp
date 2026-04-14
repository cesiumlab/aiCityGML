#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <utility>
#include "parser/citygml_parser.h"
#include "mesh/mesh_generator.h"
#include "writers/obj_writer.h"

using namespace citygml;

void printUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " <citygml_file.gml> [output_prefix]\n";
    std::cerr << "  Parses a CityGML file, triangulates all LOD geometries,\n";
    std::cerr << "  and exports each LOD to a separate Wavefront OBJ file.\n";
    std::cerr << "  Default output: <input>_LOD<n>.obj\n";
}

int main(int argc, char* argv[]) {
    std::string inputPath;
    std::string outputPrefix;

    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    inputPath = argv[1];

    if (argc >= 3) {
        outputPrefix = argv[2];
    } else {
        size_t dot = inputPath.rfind('.');
        if (dot != std::string::npos) {
            outputPrefix = inputPath.substr(0, dot);
        } else {
            outputPrefix = inputPath;
        }
    }

    std::cout << "Parsing: " << inputPath << "\n\n";

    CityGMLParser parser;
    std::shared_ptr<CityModel> cityModel;

    ParseResult result = parser.parse(inputPath, cityModel);
    if (!result.success) {
        std::cerr << "[ERROR] Parse failed: " << result.errorMessage << "\n";
        return 1;
    }
    if (!cityModel) {
        std::cerr << "[ERROR] No city model returned.\n";
        return 1;
    }

    std::cout << "[OK] Parse completed successfully.\n";

    const auto& objects = cityModel->getCityObjects();
    std::cout << "Found " << objects.size() << " city objects.\n\n";

    // Debug: find the specific RoofSurface that contains PolyID58908
    for (size_t i = 0; i < objects.size(); ++i) {
        const auto& obj = objects[i];
        std::cout << "CityObject[" << i << "]: " << obj->getId()
                  << " (type: " << obj->getObjectType() << "), boundedBy=" << obj->getBoundedBySurfaces().size() << "\n";

        const auto& surfaces = obj->getBoundedBySurfaces();
        for (const auto& surf : surfaces) {
            auto ms = surf->getMultiSurface();
            if (!ms) continue;
            size_t geomCount = ms->getGeometriesCount();
            if (geomCount > 0) {
                // Check the first polygon in this surface
                auto poly = ms->getGeometry(0);
                if (poly) {
                    std::cout << "  " << surf->getName() << " [" << geomCount << " geoms] poly[0]: "
                              << poly->getId() << "\n";
                }
            }
        }
    }
    (void)cityModel;

    // Per-LOD mesh collection.
    std::map<int, std::vector<Mesh>> lodMeshes;

    // Only export LOD4 for now.
    int targetLods[] = { 4 };

    for (size_t lodIdx = 0; lodIdx < sizeof(targetLods) / sizeof(targetLods[0]); ++lodIdx) {
        int lod = targetLods[lodIdx];
        MeshGenerator gen;
        MeshGeneratorOptions opts;
        opts.targetLOD = lod;
        gen.setOptions(opts);

        for (size_t i = 0; i < objects.size(); ++i) {
            const auto& obj = objects[i];
            std::vector<Mesh> meshes;
            gen.triangulateCityObject(*obj, meshes);
            if (!meshes.empty()) {
                for (Mesh& m : meshes) {
                    if (!m.name.empty()) {
                        m.name = "LOD" + std::to_string(lod) + "_" + m.name;
                    } else {
                        m.name = "LOD" + std::to_string(lod);
                    }
                }
                for (Mesh& m : meshes) {
                    lodMeshes[lod].push_back(std::move(m));
                }
            }
        }
    }

    if (lodMeshes.empty()) {
        std::cerr << "\n[ERROR] No meshes generated from any city objects.\n";
        return 1;
    }

    int exportedCount = 0;
    for (const auto& [lod, meshes] : lodMeshes) {
        if (meshes.empty()) continue;

        std::string objPath = outputPrefix + "_LOD" + std::to_string(lod) + ".obj";
        std::string mtlPath = outputPrefix + "_LOD" + std::to_string(lod) + ".mtl";

        size_t totalVerts = 0, totalTris = 0;
        for (const Mesh& m : meshes) {
            totalVerts += m.totalVertices();
            totalTris += m.totalTriangles();
        }

        Vec3 offset = computeBoundingBoxCenter(meshes);

        bool ok = writeObjFile(meshes, objPath, mtlPath, offset);
        if (!ok) {
            std::cerr << "[ERROR] Failed to write: " << objPath << "\n";
            continue;
        }

        std::cout << "[OK] LOD" << lod << ": " << meshes.size() << " mesh(es), "
                  << totalVerts << " vertices, " << totalTris << " triangles"
                  << "  -> " << objPath << "\n";
        exportedCount++;
    }

    std::cout << "\n============================================================\n";
    std::cout << "Exported " << exportedCount << " LOD OBJ file(s).\n";
    std::cout << "============================================================\n";

    return 0;
}
