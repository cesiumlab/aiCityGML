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

// Compute bounding box center from all meshes (for coordinate offset).
Vec3 computeBoundingBoxCenter(const std::vector<Mesh>& meshes);

// Write meshes with all vertices shifted by -offset, improving coordinate precision.
// The mtllib line in the OBJ file uses only the basename of mtlPath.
bool writeObjFile(const std::vector<Mesh>& meshes,
                  const std::string& objPath,
                  const std::string& mtlPath,
                  const Vec3& offset);

// Write meshes with bounding-box offset; mtllib uses only the basename of mtlPath.
// The mtl file is written to mtlPath, and objPath is used for output file.
bool writeObjFile(const std::vector<Mesh>& meshes,
                  const std::string& objPath,
                  const Vec3& offset);

} // namespace citygml
