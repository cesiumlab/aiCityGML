#pragma once

#include <string>
#include <vector>
#include "mesh/mesh.h"

namespace citygml {

// Write a single Mesh to a .obj file.
// Outputs both .obj and .mtl (material library) files.
// Returns true on success.
bool writeObjFile(const Mesh& mesh,
                  const std::string& objPath,
                  const std::string& mtlPath);

// Write all meshes from a CityGML model to a single .obj file.
// Returns true on success.
bool writeObjFile(const std::vector<Mesh>& meshes,
                   const std::string& objPath,
                   const std::string& mtlPath);

} // namespace citygml
