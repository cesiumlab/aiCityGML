# CityGML to OBJ Converter

> This project was generated using Vibe Coding approach.

A pure C++ implementation of CityGML 1.0 / 2.0 / 3.0 data parser and Wavefront OBJ format generator.

## Features

- **Full CityGML Module Support**: Building, Road, WaterBody, Vegetation, Relief, Bridge, Tunnel, etc.
- **Multi-LOD Support**: LOD0 ~ LOD4, automatic geometry extraction
- **Polygon Triangulation**: Based on Earcut algorithm, supports polygons with holes
- **Automatic Normal Calculation**: Face normals + vertex normals
- **Material Color Extraction**: X3DMaterial color extraction from Appearance module
- **Texture Support**: ParameterizedTexture references (texture image paths exported to MTL)
- **OBJ Format Output**: Supports `.obj` + `.mtl` material library files
- **Zero External Dependencies**: Core dependencies are all header-only libraries (TinyXML-2 + Earcut)

## Project Structure

```
citygml/
├── CMakeLists.txt              # CMake build configuration
├── README.md                   # This file
├── README_en.md                # English version
├── src/                        # Source files + headers
│   ├── core/                   # Core module (Geometry, Surface, Space...)
│   ├── building/               # Building module
│   ├── construction/           # Construction module
│   ├── mesh/                   # Mesh module
│   ├── parser/                 # Parser module
│   ├── writers/                # OBJ writer
│   └── test/                   # Test programs
├── cmake/                      # CMake configuration templates
└── testdata/                   # Test data
```

## Third-Party Dependencies

| Library | Version | URL | Description |
|---|---|---|---|
| **tinyxml2** | >= 9.0.0 | https://github.com/leethomason/tinyxml2 | XML parsing, C++ header-only library |
| **earcut.hpp** | Recommended v2.2.4 | https://github.com/mapbox/earcut.hpp | Polygon triangulation, single-file header library |

> CMake automatically downloads dependencies via FetchContent. No manual preparation required.

## Build Instructions

### Requirements

- **C++ Compiler**: C++17 support (MSVC 2019+ / GCC 9+ / Clang 12+)
- **CMake**: 3.16 or higher
- **Network**: Required for first-time build to download TinyXML-2 and Earcut

### Quick Build (Windows / MSVC)

```bash
# 1. Navigate to project directory
cd citygml

# 2. Configure + Build
cmake -B build -G "Visual Studio 16 2019" -A x64
cmake --build build --config Release --parallel

# 3. Run test
build\Release\test_parser.exe testdata\2.gml
```

### Quick Build (Linux / macOS)

```bash
cd citygml
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/test_parser testdata/2.gml
```

### Using CMake GUI or Visual Studio

Open the project root directory with CMake GUI or Visual Studio, set source and build directories, then click Configure and Generate.

## Command Line Usage

After building, `test_parser.exe` (Windows) or `test_parser` (Linux/macOS) is generated in the `build/` directory.

```bash
# Basic usage (auto-generate .obj file with same name)
test_parser testdata/2.gml

# Specify output file
test_parser testdata/2.gml output/model.obj
```

## C++ Programming Interface

### Minimal Usage

```cpp
#include "parser/citygml_parser.h"
#include "mesh/mesh_generator.h"
#include "writers/obj_writer.h"

int main() {
    citygml::CityGMLParser parser;
    std::shared_ptr<citygml::CityModel> cityModel;

    citygml::ParseResult result = parser.parse("input/building.gml", cityModel);
    if (!result.success) {
        return 1;
    }

    citygml::MeshGenerator gen;
    std::vector<citygml::Mesh> allMeshes;

    for (const auto& obj : cityModel->getCityObjects()) {
        std::vector<citygml::Mesh> meshes;
        gen.triangulateCityObject(*obj, meshes);
        for (auto& m : meshes) {
            allMeshes.push_back(std::move(m));
        }
    }

    citygml::writeObjFile(allMeshes, "output/building.obj", "output/building.mtl");
    return 0;
}
```

### C++ API Quick Reference

| Class/Function | Description |
|------|------|
| `CityGMLParser` | Main CityGML parser class |
| `parser.parse(path, model)` | Parse CityGML file |
| `parser.parseString(xml, model)` | Parse XML string |
| `CityModel` | CityGML city model root object |
| `CityModel::getCityObjects()` | Get all city objects |
| `MeshGenerator::triangulateCityObject()` | Triangulate city object to generate Mesh |
| `writeObjFile()` | Write Mesh to OBJ file |

## Test Data

The `testdata/` directory contains CityGML sample data:

| File | Description |
|------|------|
| `testdata/2.gml` | CityGML 2.0 building data, LOD2 geometry, with appearance texture references |
| `testdata/2_appearance/*.png` | Texture images |
| `testdata/FZK-Haus-LoD*.gml` | KIT official samples (LoD0 ~ LoD3) |
| `testdata/samplycity_*.gml` | Samply City sample data |

### Other Recommended Data Sources

| Dataset | URL | Description |
|---------|------|------|
| **KIT CityGML Samples** | citygmlwiki.org | Learning starter |
| **3D BAG (Netherlands)** | 3dbag.nl | Large-scale building data |
| **PLATEAU (Japan)** | mlit.go.jp/plateau | 200+ cities, official open data |
| **Random3Dcity** | github.com/tudelft3d/Random3Dcity | Procedurally generated |
| **Berlin Data** | businesslocationcenter.de | LOD2 with textures |

## Example Output

```bash
$ ./build/test_parser testdata/2.gml
Parsing: testdata/2.gml
Output OBJ: testdata/2.obj
Output MTL: testdata/2.mtl

[OK] Parse completed successfully.
Found 23 city objects.

Processing object 1/23: Building (id=BUILDING_0)
  -> 1 mesh(es), 24 vertices, 36 triangles.
Processing object 2/23: Building (id=BUILDING_1)
  -> 1 mesh(es), 20 vertices, 30 triangles.
...

============================================================
Exporting 23 mesh(es) to OBJ...
[OK] OBJ exported successfully.
  File: testdata/2.obj
  MTL:  testdata/2.mtl
  Total: 23 mesh(es), 1234 vertices, 5678 triangles.
============================================================
```

## Architecture

```
CityGML XML ──▶ XML Parsing Layer ──▶ CityGML Reader ──▶ In-Memory Model
                  (TinyXML-2)            │                    │
                                         ▼                    ▼
                                  XML Event Callback   CityModel Tree
                                                        │
                                                        ▼
                                              Geometry Processing ──▶ OBJ Writer
                                                   (Earcut)          (OBJ/MTL)
                                                                    │
                                                                    ▼
                                                               .obj + .mtl
```

## Dependencies

| Library | Version | Purpose | License | Integration |
|---------|---------|---------|---------|-------------|
| TinyXML-2 | 9.0.0 | XML Parsing | Zlib | CMake FetchContent |
| Earcut | 2.2.4 | Polygon Triangulation | ISC | CMake FetchContent |
| CMake | ≥3.16 | Build System | BSD-3 | System Tool |

## License

Project code is licensed under MIT license. Third-party dependencies are under their respective licenses (Zlib / ISC).

## Known Limitations

- Only geometry data from CityGML is supported; topology relationships are not supported yet
- ImplicitGeometry template library references require external template files
- CRS (Coordinate Reference System) conversion requires additional proj library support (current version preserves original coordinates)
- CityGML 3.0 support is incomplete (Schema-level support, API compatible)
