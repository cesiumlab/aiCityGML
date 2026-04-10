#include "citygml/mesh/mesh.h"
#include <cmath>

namespace citygml {

// ================================================================
// Vec3
// ================================================================
Vec3 Vec3::cross(const Vec3& other) const {
    return {
        y * other.z - z * other.y,
        z * other.x - x * other.z,
        x * other.y - y * other.x
    };
}

Vec3 Vec3::normalized() const {
    double len = length();
    if (len < 1e-12) return {0.0, 0.0, 0.0};
    return {x / len, y / len, z / len};
}

double Vec3::length() const {
    return std::sqrt(x * x + y * y + z * z);
}

// ================================================================
// Mesh
// ================================================================
SubMesh& Mesh::addSubMesh(const Material& mat) {
    subMeshes.emplace_back();
    subMeshes.back().material = mat;
    return subMeshes.back();
}

void Mesh::clear() {
    name.clear();
    subMeshes.clear();
}

void Mesh::mergeAll() {
    if (subMeshes.size() <= 1) return;

    std::vector<Vertex> allVerts;
    std::vector<Triangle> allTris;

    for (const SubMesh& sm : subMeshes) {
        for (const Triangle& t : sm.triangles) {
            allTris.emplace_back(
                static_cast<uint32_t>(allVerts.size()) + t.v0,
                static_cast<uint32_t>(allVerts.size()) + t.v1,
                static_cast<uint32_t>(allVerts.size()) + t.v2
            );
        }
        allVerts.insert(allVerts.end(), sm.vertices.begin(), sm.vertices.end());
    }

    clear();
    addSubMesh().vertices = std::move(allVerts);
    subMeshes.back().triangles = std::move(allTris);
}

void Mesh::flatten(std::vector<Vertex>& outVertices,
                   std::vector<Triangle>& outTriangles) const {
    outVertices.clear();
    outTriangles.clear();

    for (const SubMesh& sm : subMeshes) {
        uint32_t base = static_cast<uint32_t>(outVertices.size());
        for (const Triangle& t : sm.triangles) {
            outTriangles.emplace_back(base + t.v0, base + t.v1, base + t.v2);
        }
        outVertices.insert(outVertices.end(), sm.vertices.begin(), sm.vertices.end());
    }
}

size_t Mesh::totalVertices() const {
    size_t count = 0;
    for (const SubMesh& sm : subMeshes) count += sm.vertexCount();
    return count;
}

size_t Mesh::totalTriangles() const {
    size_t count = 0;
    for (const SubMesh& sm : subMeshes) count += sm.triangleCount();
    return count;
}

// ================================================================
// MeshBuilder
// ================================================================
MeshBuilder::MeshBuilder(const std::string& name) {
    mesh_.name = name;
}

void MeshBuilder::addTriangles(const std::vector<Vertex>& vertices,
                               const std::vector<Triangle>& triangles,
                               const Material& mat) {
    SubMesh& sm = mesh_.addSubMesh(mat);
    sm.vertices = vertices;
    sm.triangles = triangles;
}

void MeshBuilder::addSubMesh(const SubMesh& subMesh) {
    mesh_.subMeshes.push_back(subMesh);
}

Mesh MeshBuilder::build() {
    return std::move(mesh_);
}

} // namespace citygml
