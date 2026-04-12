#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace citygml {

// ================================================================
// Vec3 - 3D vector
// ================================================================
struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;

    Vec3() = default;
    Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& other) const { return {x + other.x, y + other.y, z + other.z}; }
    Vec3 operator-(const Vec3& other) const { return {x - other.x, y - other.y, z + other.z}; }
    Vec3 operator*(double scalar) const { return {x * scalar, y * scalar, z * scalar}; }

    double dot(const Vec3& other) const { return x * other.x + y * other.y + z * other.z; }
    Vec3 cross(const Vec3& other) const;
    Vec3 normalized() const;
    double length() const;
};

// ================================================================
// Vec2 - 2D vector (for texture coordinates)
// ================================================================
struct Vec2 {
    double u = 0.0, v = 0.0;

    Vec2() = default;
    Vec2(double u_, double v_) : u(u_), v(v_) {}
};

// ================================================================
// Vertex - Vertex with position, normal, and optional UV
// ================================================================
struct Vertex {
    Vec3 position;
    Vec3 normal    = {0.0, 0.0, 0.0};   // face normal, set after triangulation
    std::optional<Vec2> uv;              // only if ParameterizedTexture is available

    Vertex() = default;
    explicit Vertex(const Vec3& pos) : position(pos) {}
    Vertex(const Vec3& pos, const Vec3& norm) : position(pos), normal(norm) {}
    Vertex(const Vec3& pos, const Vec3& norm, const Vec2& texCoord)
        : position(pos), normal(norm), uv(texCoord) {}

    void setNormal(const Vec3& n) { normal = n; }
};

// ================================================================
// Material - PBR-ish material for rendering
// ================================================================
struct Material {
    std::string name;

    // Base color
    double diffuseR = 0.8, diffuseG = 0.8, diffuseB = 0.8, diffuseA = 1.0;

    // Lighting
    double ambientR = 0.2, ambientG = 0.2, ambientB = 0.2;
    double specularR = 0.0, specularG = 0.0, specularB = 0.0;
    double emissiveR = 0.0, emissiveG = 0.0, emissiveB = 0.0;

    double shininess = 0.0;
    double transparency = 0.0;

    // Texture
    std::string textureURI;      // path to image file
    std::string mimeType;       // e.g. "image/png", "image/jpeg"

    bool hasTexture() const { return !textureURI.empty(); }
};

// ================================================================
// Triangle - Indexed triangle
// ================================================================
struct Triangle {
    uint32_t v0 = 0, v1 = 0, v2 = 0;

    Triangle() = default;
    Triangle(uint32_t a, uint32_t b, uint32_t c) : v0(a), v1(b), v2(c) {}
};

// ================================================================
// SubMesh - A mesh segment sharing one material/texture
// ================================================================
class SubMesh {
public:
    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;
    Material material;

    size_t triangleCount() const { return triangles.size(); }
    size_t vertexCount() const { return vertices.size(); }

    bool isEmpty() const { return vertices.empty() || triangles.empty(); }
};

// ================================================================
// Mesh - The complete mesh output for a CityGML geometry
// ================================================================
class Mesh {
public:
    std::string name;                          // e.g. "Building_01_LOD2"
    std::vector<SubMesh> subMeshes;             // each submesh = one material/texture group

    // Merge all submeshes into a single submesh (shared vertex buffer)
    void mergeAll();

    // Flatten all triangles (returns one big vertex + index array)
    void flatten(std::vector<Vertex>& outVertices,
                 std::vector<Triangle>& outTriangles) const;

    // Statistics
    size_t totalVertices() const;
    size_t totalTriangles() const;
    size_t subMeshCount() const { return subMeshes.size(); }

    bool isEmpty() const { return subMeshes.empty(); }
    void clear();

    SubMesh& addSubMesh(const Material& mat = Material{});
};

// ================================================================
// MeshBuilder - Helper to build a Mesh incrementally
// ================================================================
class MeshBuilder {
public:
    explicit MeshBuilder(const std::string& name = "");

    // Append raw triangles from a vertex/index pair
    void addTriangles(const std::vector<Vertex>& vertices,
                      const std::vector<Triangle>& triangles,
                      const Material& mat = Material{});

    // Append a complete SubMesh
    void addSubMesh(const SubMesh& subMesh);

    // Finish building and return the Mesh
    Mesh build();

private:
    Mesh mesh_;
};

// ================================================================
// TextureCoordinates2D - 2D ring texture coordinates for ParameterizedTexture
// ================================================================
struct TextureCoordinates2D {
    std::string ringId;                                  // gml:id of the ring
    std::vector<Vec2> coordinates;                        // parallel to ring vertex order
};

} // namespace citygml
