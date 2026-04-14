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
    std::cerr << "Usage: " << prog << " <citygml_file.gml> [output_prefix] [options]\n";
    std::cerr << "  Parses a CityGML file, triangulates all LOD geometries,\n";
    std::cerr << "  and exports each LOD to a separate Wavefront OBJ file.\n";
    std::cerr << "  Default output: <input>_LOD<n>.obj\n";
    std::cerr << "\nOptions:\n";
    std::cerr << "  --lod <N>        Specify LOD level (0-4), or -1 for auto (highest). Default: 4\n";
    std::cerr << "  --export-rooms    Export Room geometries (bldg:interiorRoom). Default: disabled\n";
    std::cerr << "  --only-rooms     Export ONLY Room geometries, skip Building and other objects.\n";
    std::cerr << "  --room-index <N> Export only the N-th Room (0-based index). Default: all rooms\n";
}

int main(int argc, char* argv[]) {
    std::string inputPath;
    std::string outputPrefix;

    // MeshGenerator options
    int targetLod = 4;
    bool exportRooms = false;
    bool onlyExportRooms = false;
    int roomIndex = -1;

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--lod" && i + 1 < argc) {
            targetLod = std::stoi(argv[++i]);
        } else if (arg == "--export-rooms") {
            exportRooms = true;
        } else if (arg == "--only-rooms") {
            exportRooms = true;
            onlyExportRooms = true;
        } else if (arg == "--room-index" && i + 1 < argc) {
            exportRooms = true;
            onlyExportRooms = true;
            roomIndex = std::stoi(argv[++i]);
        } else if (arg[0] != '-') {
            // Positional argument: input file
            if (inputPath.empty()) {
                inputPath = arg;
            } else {
                outputPrefix = arg;
            }
        } else {
            std::cerr << "[ERROR] Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    if (inputPath.empty()) {
        printUsage(argv[0]);
        return 1;
    }

    if (outputPrefix.empty()) {
        size_t dot = inputPath.rfind('.');
        if (dot != std::string::npos) {
            outputPrefix = inputPath.substr(0, dot);
        } else {
            outputPrefix = inputPath;
        }
    }

    std::cout << "Parsing: " << inputPath << "\n";
    std::cout << "Target LOD: " << targetLod << "\n";
    std::cout << "Export rooms: " << (exportRooms ? "yes" : "no");
    if (onlyExportRooms) {
        std::cout << " (only rooms)";
        if (roomIndex >= 0) {
            std::cout << " [room-index=" << roomIndex << "]";
        }
    }
    std::cout << "\n\n";

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

    // Debug: print room information
    for (size_t i = 0; i < objects.size(); ++i) {
        const auto& obj = objects[i];
        std::cout << "CityObject[" << i << "]: " << obj->getId()
                  << " (type: " << obj->getObjectType() << "), boundedBy=" << obj->getBoundedBySurfaces().size() << "\n";

        // Print child city objects (Rooms)
        const auto& children = obj->getChildCityObjects();
        if (!children.empty()) {
            std::cout << "  Child city objects (" << children.size() << "):\n";
            for (const auto& child : children) {
                std::cout << "    - " << child->getId()
                          << " (type: " << child->getObjectType() << ")";
                // Check for LOD geometries
                if (child->getLod2MultiSurface() || child->getLod3MultiSurface() || child->getLod4MultiSurface()) {
                    std::cout << " [has LOD geometry]";
                }
                std::cout << "\n";
            }
        }

        const auto& surfaces = obj->getBoundedBySurfaces();
        for (const auto& surf : surfaces) {
            auto ms = surf->getMultiSurface();
            if (!ms) continue;
            size_t geomCount = ms->getGeometriesCount();
            if (geomCount > 0) {
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

    // LOD levels to export (from command line, default just targetLod)
    std::vector<int> targetLods;
    if (targetLod == -1) {
        // Auto mode: export all available LODs
        targetLods = {0, 1, 2, 3, 4};
    } else {
        targetLods = {targetLod};
    }

    for (int lod : targetLods) {
        MeshGenerator gen;
        MeshGeneratorOptions opts;
        opts.targetLOD = lod;
        opts.exportRooms = exportRooms;
        opts.onlyExportRooms = onlyExportRooms;
        opts.roomIndex = roomIndex;
        gen.setOptions(opts);

        for (size_t i = 0; i < objects.size(); ++i) {
            const auto& obj = objects[i];
            std::vector<Mesh> meshes;

            if (onlyExportRooms) {
                if (roomIndex >= 0) {
                    gen.triangulateRoomByIndex(*cityModel, meshes);
                } else {
                    gen.triangulateOnlyRooms(*cityModel, meshes);
                }
            } else {
                gen.triangulateCityObject(*obj, meshes);
            }

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
