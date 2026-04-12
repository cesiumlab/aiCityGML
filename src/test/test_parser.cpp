#include <iostream>
#include <iomanip>
#include <fstream>
#include "citygml/citygml_parser.h"
#include "citygml/mesh/mesh_generator.h"
#include "citygml/writers/obj_writer.h"

using namespace citygml;

void printUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " <citygml_file.gml> [output.obj]\n";
    std::cerr << "  Parses a CityGML file, triangulates all LOD geometries,\n";
    std::cerr << "  and exports to a Wavefront OBJ file.\n";
    std::cerr << "  Default output: <input>.obj\n";
}

int main(int argc, char* argv[]) {
    std::string inputPath;
    std::string outputPath;

    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    inputPath = argv[1];

    if (argc >= 3) {
        outputPath = argv[2];
    } else {
        size_t dot = inputPath.rfind('.');
        if (dot != std::string::npos) {
            outputPath = inputPath.substr(0, dot) + ".obj";
        } else {
            outputPath = inputPath + ".obj";
        }
    }
    std::string mtlPath = outputPath;
    size_t dot = mtlPath.rfind('.');
    if (dot != std::string::npos) {
        mtlPath = mtlPath.substr(0, dot) + ".mtl";
    } else {
        mtlPath = mtlPath + ".mtl";
    }

    std::cout << "Parsing: " << inputPath << "\n";
    std::cout << "Output OBJ: " << outputPath << "\n";
    std::cout << "Output MTL: " << mtlPath << "\n\n";

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

    std::vector<Mesh> allMeshes;
    MeshGenerator gen;

    const auto& objects = cityModel->getCityObjects();
    std::cout << "Found " << objects.size() << " city objects.\n\n";

    for (size_t i = 0; i < objects.size(); ++i) {
        const auto& obj = objects[i];
        std::cout << "Processing object " << (i + 1) << "/" << objects.size()
                  << ": " << obj->getObjectType();
        if (!obj->getId().empty()) std::cout << " (id=" << obj->getId() << ")";
        std::cout << "\n";

        std::vector<Mesh> meshes;
        gen.triangulateCityObject(*obj, meshes);

        if (meshes.empty()) {
            std::cout << "  -> No mesh generated (no LOD geometry found).\n";
        } else {
            size_t totalVerts = 0, totalTris = 0;
            for (const Mesh& m : meshes) {
                totalVerts += m.totalVertices();
                totalTris += m.totalTriangles();
            }
            std::cout << "  -> " << meshes.size() << " mesh(es), "
                      << totalVerts << " vertices, " << totalTris << " triangles.\n";
            for (const Mesh& m : meshes) {
                allMeshes.push_back(std::move(m));
            }
        }
    }

    if (allMeshes.empty()) {
        std::cerr << "\n[ERROR] No meshes generated from any city objects.\n";
        return 1;
    }

    std::cout << "\n============================================================\n";
    std::cout << "Exporting " << allMeshes.size() << " mesh(es) to OBJ...\n";

    bool ok = writeObjFile(allMeshes, outputPath, mtlPath);
    if (!ok) {
        std::cerr << "[ERROR] Failed to write OBJ file.\n";
        return 1;
    }

    size_t totalVerts = 0, totalTris = 0;
    for (const Mesh& m : allMeshes) {
        totalVerts += m.totalVertices();
        totalTris += m.totalTriangles();
    }
    std::cout << "[OK] OBJ exported successfully.\n";
    std::cout << "  File: " << outputPath << "\n";
    std::cout << "  MTL:  " << mtlPath << "\n";
    std::cout << "  Total: " << allMeshes.size() << " mesh(es), "
              << totalVerts << " vertices, " << totalTris << " triangles.\n";
    std::cout << "============================================================\n";

    return 0;
}
