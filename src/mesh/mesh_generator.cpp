#include "citygml/mesh/mesh_generator.h"
#include "citygml/core/citygml_appearance.h"
#include "citygml/mesh/mesh.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <list>
#include <vector>
#include <cstdint>

namespace citygml {

// Convert a LinearRing to a vector of Vec3 for processing.
static std::vector<Vec3> ringToVec3(const LinearRing& ring) {
    std::vector<Vec3> out;
    for (const Point& p : ring.getPoints()) {
        out.emplace_back(p.x(), p.y(), p.z());
    }
    return out;
}

// ================================================================
// Ear-clipping triangulation (VS2019 compatible, no external dependency).
// Cross product in 2D: positive = CCW, negative = CW.
static double cross2D(double ax, double ay, double bx, double by) {
    return ax * by - ay * bx;
}

// Signed area of triangle (a,b,c).
static double triangleArea2D(double ax, double ay, double bx, double by, double cx, double cy) {
    return cross2D(bx - ax, by - ay, cx - ax, cy - ay);
}

// Point-in-triangle test (strictly inside, assuming CCW winding).
// Returns true only if P is strictly inside (not on edge).
static bool pointInTriangle2D(double ax, double ay, double bx, double by,
                              double cx, double cy, double px, double py) {
    double areaABC = triangleArea2D(ax, ay, bx, by, cx, cy);
    if (std::abs(areaABC) < 1e-12) return false;
    double areaPBC = triangleArea2D(px, py, bx, by, cx, cy);
    double areaAPC = triangleArea2D(ax, ay, px, py, cx, cy);
    double areaABP = triangleArea2D(ax, ay, bx, by, px, py);
    return (areaPBC > 0) && (areaAPC > 0) && (areaABP > 0);
}

// Ear-clipping triangulation for a simple polygon.
// Input: 2D points in any winding order.
// Output: N-2 triangles as flat index array.
static std::vector<uint32_t> earClipTriangulate(const std::vector<Vec2>& pts2d) {
    std::vector<uint32_t> result;
    size_t n = pts2d.size();
    if (n < 3) return result;

    // Detect winding: compute signed polygon area via shoelace formula.
    // totalArea > 0 means CCW, < 0 means CW.
    double totalArea = 0;
    for (size_t i = 0; i < n; ++i) {
        const Vec2& a = pts2d[i];
        const Vec2& b = pts2d[(i + 1) % n];
        totalArea += a.u * b.v - b.u * a.v;
    }
    bool isCCW = (totalArea > 0);

    std::list<uint32_t> remaining;
    for (uint32_t i = 0; i < static_cast<uint32_t>(n); ++i) remaining.push_back(i);

    while (remaining.size() > 3) {
        bool earFound = false;
        for (auto it = remaining.begin(); it != remaining.end(); ++it) {
            auto prevIt = (it == remaining.begin()) ? std::prev(remaining.end()) : std::prev(it);
            auto nextIt = std::next(it);
            if (nextIt == remaining.end()) nextIt = remaining.begin();

            uint32_t vi = *it;
            uint32_t vp = *prevIt;
            uint32_t vn = *nextIt;

            double area = triangleArea2D(
                pts2d[vp].u, pts2d[vp].v,
                pts2d[vi].u, pts2d[vi].v,
                pts2d[vn].u, pts2d[vn].v);

            // CCW polygon: area>0 = convex ear. CW polygon: area<0 = convex ear.
            bool isConvex = isCCW ? (area > 0) : (area < 0);
            if (!isConvex) continue;

            bool hasInterior = false;
            for (uint32_t vk : remaining) {
                if (vk == vp || vk == vi || vk == vn) continue;
                if (vk >= static_cast<uint32_t>(n)) continue;
                if (pointInTriangle2D(
                        pts2d[vp].u, pts2d[vp].v,
                        pts2d[vi].u, pts2d[vi].v,
                        pts2d[vn].u, pts2d[vn].v,
                        pts2d[vk].u, pts2d[vk].v)) {
                    hasInterior = true;
                    break;
                }
            }
            if (hasInterior) continue;

            result.push_back(vp);
            result.push_back(vi);
            result.push_back(vn);
            auto eraseIt = it;
            if (it == remaining.begin()) it = std::prev(remaining.end());
            else --it;
            remaining.erase(eraseIt);
            earFound = true;
            break;
        }

        if (!earFound) break;
    }

    if (remaining.size() == 3) {
        for (uint32_t idx : remaining) result.push_back(idx);
    }

    return result;
}

// ================================================================
// Local frame projection for triangulation
// ================================================================

// Dot product of two Vec3 objects.
static double vecDot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Cross product of two Vec3 objects.
static Vec3 vecCross(const Vec3& a, const Vec3& b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

// Build local coordinate system from first 3 non-collinear points of the ring:
//   origin  = p0
//   x_axis  = normalize(p1 - p0)
//   z_axis  = normalize((p1-p0) x (p2-p0))  [polygon normal]
//   y_axis  = z_axis x x_axis
// Project all unique ring points onto this frame.
// Outputs parallel 3D (original positions) and 2D (local UV) arrays.
// uvPts: optional per-vertex texture coordinates (must be parallel to pts with start offset).
//        If provided, collinear-point removal keeps them synchronized.
//        If nullopt, only 3D positions are projected (no UV).
static void buildLocalFrame(const std::vector<Vec3>& pts,
                           const std::vector<Vec2>* uvPts,
                           Vec3& origin, Vec3& x_axis, Vec3& y_axis, Vec3& z_axis,
                           std::vector<Vec3>& out3d,
                           std::vector<Vec2>& out2d) {
    out3d.clear();
    out2d.clear();
    size_t n = pts.size();
    if (n < 3) return;

    // Skip closing duplicate point (first == last).
    size_t start = 0;
    if (n >= 2 &&
        std::abs(pts[0].x - pts[n - 1].x) < 1e-8 &&
        std::abs(pts[0].y - pts[n - 1].y) < 1e-8 &&
        std::abs(pts[0].z - pts[n - 1].z) < 1e-8) {
        start = 1;
    }
    if (n - start < 3) return;

    // Build orthonormal frame from first 3 unique points.
    origin = pts[start];
    x_axis = pts[start + 1] - origin;
    double xlen = std::sqrt(x_axis.x * x_axis.x + x_axis.y * x_axis.y + x_axis.z * x_axis.z);
    if (xlen < 1e-10) return;
    x_axis = { x_axis.x / xlen, x_axis.y / xlen, x_axis.z / xlen };

    Vec3 rawZ = vecCross(x_axis, pts[start + 2] - origin);
    double rawZlen = std::sqrt(rawZ.x * rawZ.x + rawZ.y * rawZ.y + rawZ.z * rawZ.z);
    if (rawZlen < 1e-10) return;  // collinear p0,p1,p2
    z_axis = { rawZ.x / rawZlen, rawZ.y / rawZlen, rawZ.z / rawZlen };

    y_axis = vecCross(z_axis, x_axis);
    double ylen = std::sqrt(y_axis.x * y_axis.x + y_axis.y * y_axis.y + y_axis.z * y_axis.z);
    if (ylen > 1e-10) {
        y_axis = { y_axis.x / ylen, y_axis.y / ylen, y_axis.z / ylen };
    } else {
        Vec3 ry = vecCross({0,0,1}, x_axis);
        double rylen = std::sqrt(ry.x * ry.x + ry.y * ry.y + ry.z * ry.z);
        if (rylen > 1e-10) {
            y_axis = { ry.x / rylen, ry.y / rylen, ry.z / rylen };
        } else {
            ry = vecCross({1,0,0}, x_axis);
            rylen = std::sqrt(ry.x * ry.x + ry.y * ry.y + ry.z * ry.z);
            y_axis = rylen > 1e-10 ? Vec3{ ry.x / rylen, ry.y / rylen, ry.z / rylen } : Vec3{0,1,0};
        }
    }

    // Project all unique points and compute 2D local UVs.
    // Also build raw2d for collinearity check (must use abs(area)).
    std::vector<Vec2> raw2d;
    raw2d.reserve(n - start);
    for (size_t i = start; i < n; ++i) {
        Vec3 d = pts[i] - origin;
        double u = vecDot(d, x_axis);
        double v = vecDot(d, y_axis);
        raw2d.push_back({u, v});
    }

    // Remove collinear points from both raw2d (for area check) and out3d/out2d (for output).
    size_t m = raw2d.size();
    for (size_t i = 0; i < m; ++i) {
        const Vec2& prev = raw2d[(i + m - 1) % m];
        const Vec2& curr = raw2d[i];
        const Vec2& next = raw2d[(i + 1) % m];
        if (std::abs(triangleArea2D(prev.u, prev.v, curr.u, curr.v, next.u, next.v)) > 1e-10) {
            out3d.push_back(pts[start + i]);
            if (uvPts && start + i < uvPts->size()) {
                out2d.push_back((*uvPts)[start + i]);
            } else {
                out2d.push_back(curr);  // use local projection as fallback UV
            }
        }
    }
}

// Forward declaration of the full UV-aware triangulateRing.
static void triangulateRing(const std::vector<Vec3>& pts,
                           const std::vector<Vec2>* uvPts,
                           std::vector<Vertex>& outVerts,
                           std::vector<Triangle>& outTris);

// Non-UV overload delegates to the full version.
static void triangulateRing(const std::vector<Vec3>& pts,
                           std::vector<Vertex>& outVerts,
                           std::vector<Triangle>& outTris) {
    triangulateRing(pts, nullptr, outVerts, outTris);
}

// Full UV-aware triangulateRing.
static void triangulateRing(const std::vector<Vec3>& pts,
                           const std::vector<Vec2>* uvPts,
                           std::vector<Vertex>& outVerts,
                           std::vector<Triangle>& outTris) {
    if (pts.size() < 3) return;

    Vec3 origin, x_axis, y_axis, z_axis;
    std::vector<Vec3> clean3d;
    std::vector<Vec2> clean2d;
    buildLocalFrame(pts, uvPts, origin, x_axis, y_axis, z_axis, clean3d, clean2d);

    if (clean3d.size() < 3) return;

    // Flip z_axis so that for a CCW polygon the face normal points outward
    // (toward +z in the local frame, matching the right-hand rule convention).
    z_axis = { -z_axis.x, -z_axis.y, -z_axis.z };

    std::vector<uint32_t> indices = earClipTriangulate(clean2d);
    if (indices.empty()) return;

    uint32_t baseIdx = static_cast<uint32_t>(outVerts.size());
    for (size_t i = 0; i < clean3d.size(); ++i) {
        if (uvPts && i < clean2d.size()) {
            outVerts.emplace_back(clean3d[i], z_axis, clean2d[i]);
        } else {
            outVerts.emplace_back(clean3d[i], z_axis);
        }
    }
    for (size_t i = 0; i < indices.size(); i += 3) {
        outTris.emplace_back(baseIdx + indices[i],
                             baseIdx + indices[i + 2],
                             baseIdx + indices[i + 1]);
    }
}

// ================================================================
// MeshGenerator
// ================================================================
MeshGenerator::MeshGenerator(const MeshGeneratorOptions& options)
    : options_(options) {}

void MeshGenerator::setOptions(const MeshGeneratorOptions& options) {
    options_ = options;
}

// ================================================================
// Core triangulation
// ================================================================
void MeshGenerator::triangulateLinearRing(const LinearRing& ring,
                                          std::vector<Vertex>& outVertices,
                                          std::vector<Triangle>& outTriangles) {
    std::vector<Vec3> pts = ringToVec3(ring);
    triangulateRing(pts, outVertices, outTriangles);
}

void MeshGenerator::triangulatePolygon(const Polygon& polygon,
                                       std::vector<Vertex>& outVertices,
                                       std::vector<Triangle>& outTriangles,
                                       Material& outMaterial) {
    LinearRingPtr exterior = polygon.getExteriorRing();
    if (!exterior || exterior->getPointsCount() < 3) return;

    std::vector<Vec3> pts = ringToVec3(*exterior);
    const std::vector<Vec2>* uvPtr = nullptr;
    std::vector<Vec2> uvCoords;

    if (auto tex = polygon.getTexture()) {
        std::vector<TextureCoordinates2D> texCoords = collectTextureCoords(polygon, *tex);
        if (!texCoords.empty() && !texCoords[0].coordinates.empty()) {
            uvCoords = texCoords[0].coordinates;
            // If the ring has a closing duplicate, the UV list may have one extra;
            // trim to match pts.size() if needed.
            if (uvCoords.size() >= pts.size()) {
                uvCoords.resize(pts.size());
                uvPtr = &uvCoords;
            }
        }
        outMaterial = fromParameterizedTexture(*tex);
    } else if (auto mat = polygon.getMaterial()) {
        outMaterial = fromX3DMaterial(*mat);
    }

    triangulateRing(pts, uvPtr, outVertices, outTriangles);
}

void MeshGenerator::triangulateMultiSurface(const MultiSurface& ms,
                                             Mesh& outMesh,
                                             const Material& defaultMat) {
    outMesh.clear();
    outMesh.name = ms.getId().empty() ? "MultiSurface" : ms.getId();

    size_t count = ms.getGeometriesCount();
    if (count == 0) return;

    for (size_t i = 0; i < count; ++i) {
        PolygonPtr poly = ms.getGeometry(i);
        if (!poly) continue;

        LinearRingPtr exterior = poly->getExteriorRing();
        if (!exterior || exterior->getPointsCount() < 3) continue;

        // Collect material and texture coordinates for this polygon.
        Material mat = defaultMat;
        const std::vector<Vec2>* uvPtr = nullptr;
        std::vector<Vec2> uvCoords;

        if (auto t = poly->getTexture()) {
            mat = fromParameterizedTexture(*t);
            std::vector<TextureCoordinates2D> texCoords = collectTextureCoords(*poly, *t);
            if (!texCoords.empty() && !texCoords[0].coordinates.empty()) {
                uvCoords = texCoords[0].coordinates;
                if (uvCoords.size() >= static_cast<size_t>(exterior->getPointsCount())) {
                    uvCoords.resize(exterior->getPointsCount());
                    uvPtr = &uvCoords;
                }
            }
        } else if (auto m = poly->getMaterial()) {
            mat = fromX3DMaterial(*m);
        }

        // Triangulate exterior ring.
        std::vector<Vertex> verts;
        std::vector<Triangle> tris;
        triangulateRing(ringToVec3(*exterior), uvPtr, verts, tris);

        if (verts.empty() || tris.empty()) continue;

        SubMesh& sm = outMesh.addSubMesh(mat);
        sm.vertices = std::move(verts);
        sm.triangles = std::move(tris);

        // Interior rings (holes) - skip for now, just emit warning.
        if (poly->getInteriorRingsCount() > 0) {
            std::cerr << "[MeshGenerator] Warning: interior rings not yet supported for Polygon "
                      << poly->getId() << std::endl;
        }
    }
}

void MeshGenerator::triangulateSolid(const Solid& solid,
                                      Mesh& outMesh,
                                      const Material& defaultMat) {
    MultiSurfacePtr shell = solid.getOuterShell();
    if (shell) {
        triangulateMultiSurface(*shell, outMesh, defaultMat);
    } else {
        outMesh.clear();
    }
}

// ================================================================
// CityGML object-level functions
// ================================================================
void MeshGenerator::triangulateCityObject(const CityObject& obj,
                                          std::vector<Mesh>& outMeshes) const {
    int lod = options_.targetLOD;
    std::vector<std::shared_ptr<MultiSurface>> candidates;
    std::vector<std::shared_ptr<Solid>> solidCandidates;

    auto tryAdd = [&](auto ptr) {
        if (ptr) candidates.push_back(ptr);
    };
    auto tryAddSolid = [&](auto ptr) {
        if (ptr) solidCandidates.push_back(ptr);
    };

    if (lod == -1 || lod == 4) { tryAdd(obj.getLod4MultiSurface()); tryAddSolid(obj.getLod4Solid()); }
    if (lod == -1 || lod == 3) { tryAdd(obj.getLod3MultiSurface()); tryAddSolid(obj.getLod3Solid()); }
    if (lod == -1 || lod == 2) { tryAdd(obj.getLod2MultiSurface()); tryAddSolid(obj.getLod2Solid()); }
    if (lod == -1 || lod == 1) { tryAdd(obj.getLod1MultiSurface()); tryAddSolid(obj.getLod1Solid()); }
    if (lod == -1 || lod == 0) {
        tryAdd(obj.getLod0MultiSurface());
        tryAdd(obj.getLod0FootPrint());
        tryAdd(obj.getLod0RoofEdge());
    }

    if (!candidates.empty()) {
        Mesh m;
        bool first = true;
        for (auto& ms : candidates) {
            Mesh tmp;
            triangulateMultiSurface(*ms, tmp);
            if (!tmp.isEmpty()) {
                for (SubMesh& sm : tmp.subMeshes) {
                    outMeshes.emplace_back();
                    outMeshes.back().name = obj.getId().empty() ? obj.getObjectType() : obj.getId();
                    outMeshes.back().addSubMesh(sm.material).vertices = std::move(sm.vertices);
                    outMeshes.back().subMeshes.back().triangles = std::move(sm.triangles);
                    if (first) first = false;
                }
            }
        }
    }

    for (auto& solid : solidCandidates) {
        Mesh m;
        triangulateSolid(*solid, m);
        if (!m.isEmpty()) {
            m.name = obj.getId().empty() ? obj.getObjectType() : obj.getId();
            outMeshes.push_back(std::move(m));
        }
    }
}

// ================================================================
// FootPrint & RoofEdge helpers
// ================================================================
void MeshGenerator::triangulateFootPrint(const CityObject& obj,
                                         Mesh& outMesh) {
    MultiSurfacePtr fp = obj.getLod0FootPrint();
    if (fp) {
        triangulateMultiSurface(*fp, outMesh);
    } else {
        outMesh.clear();
    }
}

void MeshGenerator::triangulateRoofEdge(const CityObject& obj,
                                        Mesh& outMesh) {
    MultiSurfacePtr re = obj.getLod0RoofEdge();
    if (re) {
        triangulateMultiSurface(*re, outMesh);
    } else {
        outMesh.clear();
    }
}

// ================================================================
// Opening / hole processing
// ================================================================
void MeshGenerator::triangulateWallSurface(const WallSurface& wall,
                                            std::vector<Mesh>& outMeshes) {
    (void)wall;
    outMeshes.clear();
}

void MeshGenerator::triangulatePolygonWithHoles(const Polygon&,
                                               const std::vector<LinearRingPtr>&,
                                               std::vector<Vertex>&,
                                               std::vector<Triangle>&,
                                               Material&) {}

// ================================================================
// Thematic surface helpers
// ================================================================
void MeshGenerator::triangulateRoofSurface(const RoofSurface& roof,
                                           Mesh& outMesh) {
    MultiSurfacePtr ms = roof.getMultiSurface();
    if (ms) {
        triangulateMultiSurface(*ms, outMesh);
    } else {
        outMesh.clear();
    }
}

void MeshGenerator::triangulateGroundSurface(const GroundSurface& ground,
                                             Mesh& outMesh) {
    MultiSurfacePtr ms = ground.getMultiSurface();
    if (ms) {
        triangulateMultiSurface(*ms, outMesh);
    } else {
        outMesh.clear();
    }
}

void MeshGenerator::triangulateThematicSurface(const AbstractThematicSurface& surface,
                                               Mesh& outMesh) {
    MultiSurfacePtr ms = surface.getMultiSurface();
    if (ms) {
        triangulateMultiSurface(*ms, outMesh);
    } else {
        outMesh.clear();
    }
}

// ================================================================
// ImplicitGeometry support
// ================================================================
void MeshGenerator::triangulateImplicitGeometry(const ImplicitGeometry& impl,
                                               Mesh& outMesh) {
    (void)impl;
    outMesh.clear();
}

// ================================================================
// Utility functions
// ================================================================
Vec3 MeshGenerator::computeFaceNormal(const Vertex& v0,
                                      const Vertex& v1,
                                      const Vertex& v2) {
    Vec3 ab = v1.position - v0.position;
    Vec3 ac = v2.position - v0.position;
    return ab.cross(ac).normalized();
}

void MeshGenerator::computeVertexNormals(std::vector<Vertex>&,
                                         const std::vector<Triangle>&) {}

void MeshGenerator::extrudeFootPrint(const std::vector<Vertex>&,
                                     const std::vector<Triangle>&,
                                     const Vec3&,
                                     double,
                                     Mesh&,
                                     const Material&) {}

Material MeshGenerator::fromX3DMaterial(const X3DMaterial& mat) {
    Material m;
    m.name = "X3DMaterial";
    const auto& dc = mat.getDiffuseColor();
    if (dc.size() >= 3) {
        m.diffuseR = dc[0]; m.diffuseG = dc[1]; m.diffuseB = dc[2];
        if (dc.size() >= 4) m.diffuseA = dc[3];
    }
    const auto& ac = mat.getAmbientColor();
    if (ac.size() >= 3) {
        m.ambientR = ac[0]; m.ambientG = ac[1]; m.ambientB = ac[2];
    }
    m.transparency = mat.getTransparency();
    m.shininess = mat.getShininess();
    return m;
}

Material MeshGenerator::fromParameterizedTexture(const ParameterizedTexture& tex) {
    Material m;
    m.name = "ParameterizedTexture";
    m.textureURI = tex.getImageURI();
    m.mimeType = tex.getMimeType();
    return m;
}

std::vector<TextureCoordinates2D>
MeshGenerator::collectTextureCoords(const Polygon& polygon,
                                    const ParameterizedTexture& tex) {
    std::vector<TextureCoordinates2D> result;

    const std::string& polyId = polygon.getId();
    if (polyId.empty()) return result;

    const auto& targets = tex.getTargets();
    for (const auto& target : targets) {
        // target.uri is an XLink like "#ringID" — strip leading '#'
        std::string targetId = target.uri;
        if (!targetId.empty() && targetId[0] == '#') {
            targetId = targetId.substr(1);
        }
        if (targetId != polyId) continue;

        for (const auto& tc : target.textureCoords) {
            if (tc.ringId != polyId) continue;
            if (tc.coordinates.empty()) continue;

            TextureCoordinates2D entry;
            entry.ringId = tc.ringId;
            entry.coordinates.reserve(tc.coordinates.size());
            for (const auto& uv : tc.coordinates) {
                entry.coordinates.push_back({uv[0], uv[1]});
            }
            if (!entry.coordinates.empty()) {
                result.push_back(std::move(entry));
            }
        }
    }
    return result;
}

// ================================================================
// Helper free functions
// ================================================================
void triangulateRings(const LinearRing&,
                      const std::vector<LinearRingPtr>&,
                      std::vector<Vertex>&,
                      std::vector<Triangle>&) {}

std::vector<LinearRingPtr> extractOpeningRings(const AbstractOpening& opening) {
    (void)opening;
    return {};
}

std::shared_ptr<MultiSurface> selectMultiSurface(const CityObject& obj, int targetLOD) {
    (void)obj; (void)targetLOD;
    return nullptr;
}

std::shared_ptr<Solid> selectSolid(const CityObject& obj, int targetLOD) {
    (void)obj; (void)targetLOD;
    return nullptr;
}

} // namespace citygml