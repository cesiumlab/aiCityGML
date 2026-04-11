#include "citygml/mesh/mesh_generator.h"
#include "citygml/core/citygml_appearance.h"
#include "citygml/mesh/mesh.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <list>
#include <set>
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
            if (uvPts && i < uvPts->size()) {
                out2d.push_back((*uvPts)[i]);
            } else {
                out2d.push_back(curr);
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
void MeshGenerator::triangulateBoundedBySurfaces(const CityObject& obj,
                                                  std::vector<Mesh>& outMeshes) const {
    const auto& surfaces = obj.getBoundedBySurfaces();

    for (const auto& surface : surfaces) {
        std::shared_ptr<MultiSurface> ms = surface->getMultiSurface();
        if (!ms) continue;
        if (ms->getGeometriesCount() == 0) continue;

        std::string surfaceType;
        if (std::dynamic_pointer_cast<WallSurface>(surface)) {
            surfaceType = "WallSurface";
        } else if (std::dynamic_pointer_cast<RoofSurface>(surface)) {
            surfaceType = "RoofSurface";
        } else if (std::dynamic_pointer_cast<GroundSurface>(surface)) {
            surfaceType = "GroundSurface";
        } else if (std::dynamic_pointer_cast<ClosureSurface>(surface)) {
            surfaceType = "ClosureSurface";
        } else if (std::dynamic_pointer_cast<InteriorWallSurface>(surface)) {
            surfaceType = "InteriorWallSurface";
        } else if (std::dynamic_pointer_cast<CeilingSurface>(surface)) {
            surfaceType = "CeilingSurface";
        } else if (std::dynamic_pointer_cast<FloorSurface>(surface)) {
            surfaceType = "FloorSurface";
        } else if (std::dynamic_pointer_cast<OuterCeilingSurface>(surface)) {
            surfaceType = "OuterCeilingSurface";
        } else if (std::dynamic_pointer_cast<OuterFloorSurface>(surface)) {
            surfaceType = "OuterFloorSurface";
        } else {
            surfaceType = "ThematicSurface";
        }

        // WallSurface 支持处理开口（门/窗）作为布尔挖孔
        if (options_.processOpenings && std::dynamic_pointer_cast<WallSurface>(surface)) {
            std::vector<Mesh> wallMeshes;
            triangulateWallSurface(*std::dynamic_pointer_cast<WallSurface>(surface), wallMeshes);
            for (Mesh& m : wallMeshes) {
                if (!m.isEmpty()) {
                    m.name = surface->getId().empty() ? surfaceType : surface->getId();
                    outMeshes.push_back(std::move(m));
                }
            }
        } else {
            // 其他表面类型：直接三角化其 MultiSurface
            Mesh m;
            triangulateMultiSurface(*ms, m);
            if (!m.isEmpty()) {
                m.name = surface->getId().empty() ? surfaceType : surface->getId();
                outMeshes.push_back(std::move(m));
            }
        }
    }
}

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
        bool first = true;
        for (auto& ms : candidates) {
            Mesh tmp;
            triangulateMultiSurface(*ms, tmp);
            if (!tmp.isEmpty()) {
                for (SubMesh& sm : tmp.subMeshes) {
                    Mesh outMesh;
                    outMesh.name = obj.getId().empty() ? obj.getObjectType() : obj.getId();
                    outMesh.addSubMesh(sm.material).vertices = sm.vertices;
                    outMesh.subMeshes.back().triangles = sm.triangles;
                    outMeshes.push_back(std::move(outMesh));
                    first = false;
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

    // Implicit representations (LOD 1-3).
    std::vector<std::shared_ptr<AbstractGeometry>> implCandidates;
    if (lod == -1 || lod == 1) implCandidates.push_back(obj.getLod1ImplicitRepresentation());
    if (lod == -1 || lod == 2) implCandidates.push_back(obj.getLod2ImplicitRepresentation());
    if (lod == -1 || lod == 3) implCandidates.push_back(obj.getLod3ImplicitRepresentation());

    for (auto& g : implCandidates) {
        if (!g) continue;
        if (auto impl = std::dynamic_pointer_cast<ImplicitGeometry>(g)) {
            Mesh m;
            triangulateImplicitGeometry(*impl, m);
            if (!m.isEmpty()) {
                m.name = impl->getId().empty() ? "ImplicitGeometry" : impl->getId();
                outMeshes.push_back(std::move(m));
            }
        } else {
            // Non-implicit geometry stored in implicit slot — triangulate normally.
            if (auto ms = std::dynamic_pointer_cast<MultiSurface>(g)) {
                Mesh m;
                triangulateMultiSurface(*ms, m);
                if (!m.isEmpty()) {
                    m.name = obj.getId().empty() ? obj.getObjectType() : obj.getId();
                    outMeshes.push_back(std::move(m));
                }
            } else if (auto solid = std::dynamic_pointer_cast<Solid>(g)) {
                Mesh m;
                triangulateSolid(*solid, m);
                if (!m.isEmpty()) {
                    m.name = obj.getId().empty() ? obj.getObjectType() : obj.getId();
                    outMeshes.push_back(std::move(m));
                }
            }
        }
    }

    // Triangulate boundedBy ThematicSurfaces (WallSurface, RoofSurface, etc.).
    // TODO: 暂时禁用以隔离崩溃问题
        triangulateBoundedBySurfaces(obj, outMeshes);
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
    outMeshes.clear();
    MultiSurfacePtr ms = wall.getMultiSurface();
    if (!ms) return;

    for (size_t i = 0; i < ms->getGeometriesCount(); ++i) {
        PolygonPtr poly = ms->getGeometry(i);
        if (!poly) continue;

        LinearRingPtr exterior = poly->getExteriorRing();
        if (!exterior || exterior->getPointsCount() < 3) continue;

        Material mat;
        const std::vector<Vec2>* uvPtr = nullptr;
        std::vector<Vec2> uvCoords;

        if (auto t = poly->getTexture()) {
            mat = fromParameterizedTexture(*t);
            std::vector<TextureCoordinates2D> tc = collectTextureCoords(*poly, *t);
            if (!tc.empty() && !tc[0].coordinates.empty()) {
                uvCoords = tc[0].coordinates;
                if (uvCoords.size() >= static_cast<size_t>(exterior->getPointsCount())) {
                    uvCoords.resize(exterior->getPointsCount());
                    uvPtr = &uvCoords;
                }
            }
        } else if (auto m = poly->getMaterial()) {
            mat = fromX3DMaterial(*m);
        }

        std::vector<LinearRingPtr> holes;
        for (const auto& opening : wall.getOpenings()) {
            std::vector<LinearRingPtr> rings = extractOpeningRings(*opening);
            for (auto& r : rings) {
                if (r && r->getPointsCount() >= 3) holes.push_back(r);
            }
        }

        std::vector<Vertex> verts;
        std::vector<Triangle> tris;
        if (holes.empty()) {
            triangulateRing(ringToVec3(*exterior), uvPtr, verts, tris);
        } else {
            triangulatePolygonWithHoles(*poly, holes, verts, tris, mat);
        }

        if (verts.empty() || tris.empty()) continue;

        Mesh mesh;
        mesh.name = poly->getId().empty() ? "WallSurface" : poly->getId();
        SubMesh& sm = mesh.addSubMesh(mat);
        sm.vertices = std::move(verts);
        sm.triangles = std::move(tris);
        outMeshes.push_back(std::move(mesh));
    }
}

// Point-in-polygon test using ray casting.
// Returns true if point (px,py) is strictly inside the polygon.
// polygon2d: flat array of (x,y) vertex pairs in winding order.
static bool pointInPolygon2D(const std::vector<Vec2>& poly2d, double px, double py) {
    size_t n = poly2d.size();
    if (n < 3) return false;
    bool inside = false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        double xi = poly2d[i].u, yi = poly2d[i].v;
        double xj = poly2d[j].u, yj = poly2d[j].v;
        bool intersect = ((yi > py) != (yj > py)) &&
            (px < (xj - xi) * (py - yi) / (yj - yi) + xi);
        if (intersect) inside = !inside;
    }
    return inside;
}

// Line segment intersection test (strict: does not count touching).
static bool segmentsIntersect2D(double ax, double ay, double bx, double by,
                               double cx, double cy, double dx, double dy) {
    auto orientation = [](double sx, double sy, double ex, double ey, double px, double py) {
        return (ex - sx) * (py - sy) - (ey - sy) * (px - sx);
    };
    double o1 = orientation(ax, ay, bx, by, cx, cy);
    double o2 = orientation(ax, ay, bx, by, dx, dy);
    double o3 = orientation(cx, cy, dx, dy, ax, ay);
    double o4 = orientation(cx, cy, dx, dy, bx, by);
    if ((o1 > 0 && o2 < 0) || (o1 < 0 && o2 > 0)) {
        if ((o3 > 0 && o4 < 0) || (o3 < 0 && o4 > 0)) return true;
    }
    return false;
}

static bool edgeIntersectsHoleEdges(const Vec2& p, const Vec2& q,
                                     const std::vector<std::vector<Vec2>>& holePoly2d) {
    for (const auto& hp : holePoly2d) {
        size_t nh = hp.size();
        for (size_t j = 0; j < nh; ++j) {
            if (segmentsIntersect2D(p.u, p.v, q.u, q.v,
                                    hp[j].u, hp[j].v,
                                    hp[(j+1)%nh].u, hp[(j+1)%nh].v))
                return true;
        }
    }
    return false;
}

void MeshGenerator::triangulatePolygonWithHoles(const Polygon& surface,
                                                 const std::vector<LinearRingPtr>& holes,
                                                 std::vector<Vertex>& outVertices,
                                                 std::vector<Triangle>& outTriangles,
                                                 Material& outMaterial) {
    outVertices.clear();
    outTriangles.clear();

    LinearRingPtr exterior = surface.getExteriorRing();
    if (!exterior || exterior->getPointsCount() < 3) return;

    std::vector<Vec3> pts3d = ringToVec3(*exterior);
    const std::vector<Vec2>* exteriorUvPtr = nullptr;
    std::vector<Vec2> exteriorUv;

    if (auto tex = surface.getTexture()) {
        std::vector<TextureCoordinates2D> tc = collectTextureCoords(surface, *tex);
        if (!tc.empty() && !tc[0].coordinates.empty()) {
            exteriorUv = tc[0].coordinates;
            if (exteriorUv.size() >= pts3d.size()) {
                exteriorUv.resize(pts3d.size());
                exteriorUvPtr = &exteriorUv;
            }
        }
        outMaterial = fromParameterizedTexture(*tex);
    } else if (auto mat = surface.getMaterial()) {
        outMaterial = fromX3DMaterial(*mat);
    }

    // Project exterior ring to 2D using local frame.
    Vec3 origin, x_axis, y_axis, z_axis;
    std::vector<Vec3> clean3d;
    std::vector<Vec2> clean2d;
    buildLocalFrame(pts3d, exteriorUvPtr, origin, x_axis, y_axis, z_axis, clean3d, clean2d);
    if (clean3d.size() < 3) return;
    z_axis = {-z_axis.x, -z_axis.y, -z_axis.z};

    // Project each hole ring to 2D using the same local frame.
    std::vector<std::vector<Vec2>> holePoly2d;
    for (const auto& hole : holes) {
        if (!hole || hole->getPointsCount() < 3) continue;
        std::vector<Vec3> hole3d = ringToVec3(*hole);
        std::vector<Vec3> h3dClean;
        std::vector<Vec2> h2dClean;
        buildLocalFrame(hole3d, nullptr, origin, x_axis, y_axis, z_axis, h3dClean, h2dClean);
        if (h2dClean.size() >= 3) {
            holePoly2d.push_back(h2dClean);
        }
    }

    // Ear-clipping with hole constraints.
    double totalArea = 0;
    for (size_t i = 0; i < clean2d.size(); ++i) {
        const Vec2& a = clean2d[i];
        const Vec2& b = clean2d[(i + 1) % clean2d.size()];
        totalArea += a.u * b.v - b.u * a.v;
    }
    bool exteriorCCW = (totalArea > 0);

    std::list<size_t> remaining;
    for (size_t i = 0; i < clean2d.size(); ++i) remaining.push_back(i);

    while (remaining.size() > 3) {
        bool earFound = false;
        for (auto it = remaining.begin(); it != remaining.end(); ++it) {
            auto prevIt = (it == remaining.begin()) ? std::prev(remaining.end()) : std::prev(it);
            auto nextIt = std::next(it);
            if (nextIt == remaining.end()) nextIt = remaining.begin();

            size_t vi = *it, vp = *prevIt, vn = *nextIt;
            double area = triangleArea2D(
                clean2d[vp].u, clean2d[vp].v,
                clean2d[vi].u, clean2d[vi].v,
                clean2d[vn].u, clean2d[vn].v);

            bool isConvex = exteriorCCW ? (area > 0) : (area < 0);
            if (!isConvex) continue;

            // Check no other vertex lies inside the ear.
            bool anyInside = false;
            for (size_t vk : remaining) {
                if (vk == vp || vk == vi || vk == vn) continue;
                if (pointInTriangle2D(
                        clean2d[vp].u, clean2d[vp].v,
                        clean2d[vi].u, clean2d[vi].v,
                        clean2d[vn].u, clean2d[vn].v,
                        clean2d[vk].u, clean2d[vk].v)) {
                    anyInside = true;
                    break;
                }
            }
            if (anyInside) continue;

            // Check ear does not cross any hole edge.
            if (edgeIntersectsHoleEdges(clean2d[vp], clean2d[vn], holePoly2d)) continue;

            // Check midpoint of the ear edge is not inside any hole.
            double mx = (clean2d[vp].u + clean2d[vn].u) * 0.5;
            double my = (clean2d[vp].v + clean2d[vn].v) * 0.5;
            bool inHole = false;
            for (const auto& hp : holePoly2d) {
                if (pointInPolygon2D(hp, mx, my)) { inHole = true; break; }
            }
            if (inHole) continue;

            uint32_t baseIdx = static_cast<uint32_t>(outVertices.size());
            outVertices.emplace_back(clean3d[vp], z_axis, clean2d[vp]);
            outVertices.emplace_back(clean3d[vi], z_axis, clean2d[vi]);
            outVertices.emplace_back(clean3d[vn], z_axis, clean2d[vn]);
            outTriangles.emplace_back(baseIdx, baseIdx + 1, baseIdx + 2);

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
        uint32_t baseIdx = static_cast<uint32_t>(outVertices.size());
        for (size_t idx : remaining) {
            outVertices.emplace_back(clean3d[idx], z_axis, clean2d[idx]);
        }
        outTriangles.emplace_back(baseIdx, baseIdx + 1, baseIdx + 2);
    }
}

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
static Vec3 transformVertexPos(const Vec3& v, const TransformationMatrix4x4& m, const Vec3& ref) {
    double x = m[0] * v.x + m[1] * v.y + m[2] * v.z;
    double y = m[4] * v.x + m[5] * v.y + m[6] * v.z;
    double z = m[8] * v.x + m[9] * v.y + m[10] * v.z;
    return {x + ref.x, y + ref.y, z + ref.z};
}

static Vec3 transformNormal(const Vec3& n, const TransformationMatrix4x4& m) {
    double x = m[0] * n.x + m[1] * n.y + m[2] * n.z;
    double y = m[4] * n.x + m[5] * n.y + m[6] * n.z;
    double z = m[8] * n.x + m[9] * n.y + m[10] * n.z;
    return Vec3{x, y, z}.normalized();
}

void MeshGenerator::triangulateImplicitGeometry(const ImplicitGeometry& impl,
                                               Mesh& outMesh) {
    outMesh.clear();
    if (!impl.relativeGeometry) return;

    // Reference point (translation origin); default to origin if not set.
    Vec3 ref{0, 0, 0};
    if (impl.referencePoint) {
        ref = {impl.referencePoint->x(), impl.referencePoint->y(), impl.referencePoint->z()};
    }

    const TransformationMatrix4x4& m = impl.transformationMatrix;

    if (auto ms = std::dynamic_pointer_cast<MultiSurface>(impl.relativeGeometry)) {
        outMesh.name = impl.getId().empty() ? "ImplicitGeometry" : impl.getId();
        Mesh tmp;
        triangulateMultiSurface(*ms, tmp, Material{});
        for (SubMesh& sm : tmp.subMeshes) {
            SubMesh& outSm = outMesh.addSubMesh(sm.material);
            outSm.vertices.reserve(sm.vertices.size());
            for (const Vertex& v : sm.vertices) {
                Vertex tv;
                tv.position = transformVertexPos(v.position, m, ref);
                tv.normal   = transformNormal(v.normal, m);
                tv.uv       = v.uv;
                outSm.vertices.push_back(tv);
            }
            outSm.triangles = sm.triangles;
        }
    } else if (auto solid = std::dynamic_pointer_cast<Solid>(impl.relativeGeometry)) {
        outMesh.name = impl.getId().empty() ? "ImplicitGeometry" : impl.getId();
        Mesh tmp;
        triangulateSolid(*solid, tmp, Material{});
        for (SubMesh& sm : tmp.subMeshes) {
            SubMesh& outSm = outMesh.addSubMesh(sm.material);
            outSm.vertices.reserve(sm.vertices.size());
            for (const Vertex& v : sm.vertices) {
                Vertex tv;
                tv.position = transformVertexPos(v.position, m, ref);
                tv.normal   = transformNormal(v.normal, m);
                tv.uv       = v.uv;
                outSm.vertices.push_back(tv);
            }
            outSm.triangles = sm.triangles;
        }
    }
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

void MeshGenerator::computeVertexNormals(std::vector<Vertex>& vertices,
                                         const std::vector<Triangle>& triangles) {
    for (auto& v : vertices) {
        v.normal = {0.0, 0.0, 0.0};
    }
    for (const auto& tri : triangles) {
        if (tri.v0 >= vertices.size() ||
            tri.v1 >= vertices.size() ||
            tri.v2 >= vertices.size()) continue;
        const Vec3& a = vertices[tri.v0].position;
        const Vec3& b = vertices[tri.v1].position;
        const Vec3& c = vertices[tri.v2].position;
        Vec3 ab = b - a;
        Vec3 ac = c - a;
        Vec3 fn = ab.cross(ac);  // outward normal for CCW winding
        vertices[tri.v0].normal = vertices[tri.v0].normal + fn;
        vertices[tri.v1].normal = vertices[tri.v1].normal + fn;
        vertices[tri.v2].normal = vertices[tri.v2].normal + fn;
    }
    for (auto& v : vertices) {
        double len = std::sqrt(v.normal.x*v.normal.x + v.normal.y*v.normal.y + v.normal.z*v.normal.z);
        if (len > 1e-10) {
            v.normal = {v.normal.x/len, v.normal.y/len, v.normal.z/len};
        } else {
            v.normal = {0.0, 1.0, 0.0};
        }
    }
}

void MeshGenerator::extrudeFootPrint(const std::vector<Vertex>& footprintVertices,
                                     const std::vector<Triangle>& footprintTriangles,
                                     const Vec3& extrusionDir,
                                     double height,
                                     Mesh& outMesh,
                                     const Material& defaultMat) {
    outMesh.clear();
    if (footprintVertices.empty() || footprintTriangles.empty()) return;

    double dirLen = std::sqrt(extrusionDir.x*extrusionDir.x +
                              extrusionDir.y*extrusionDir.y +
                              extrusionDir.z*extrusionDir.z);
    if (dirLen < 1e-10) return;
    Vec3 dir = {extrusionDir.x/dirLen, extrusionDir.y/dirLen, extrusionDir.z/dirLen};
    Vec3 offset = {dir.x*height, dir.y*height, dir.z*height};

    SubMesh& sm = outMesh.addSubMesh(defaultMat);

    // Build all vertices: bottom (original) then top (offset).
    size_t n = footprintVertices.size();
    std::vector<uint32_t> bottomIdx(n), topIdx(n);
    for (size_t i = 0; i < n; ++i) {
        bottomIdx[i] = static_cast<uint32_t>(sm.vertices.size());
        sm.vertices.push_back(footprintVertices[i]);

        topIdx[i] = static_cast<uint32_t>(sm.vertices.size());
        Vertex top = footprintVertices[i];
        top.position = top.position + offset;
        sm.vertices.push_back(top);
    }

    // Top and bottom faces.
    for (const auto& tri : footprintTriangles) {
        sm.triangles.emplace_back(bottomIdx[tri.v0], bottomIdx[tri.v2], bottomIdx[tri.v1]); // bottom (flipped winding)
        sm.triangles.emplace_back(topIdx[tri.v0], topIdx[tri.v1], topIdx[tri.v2]);           // top (keep winding)
    }

    // Side faces: collect boundary edges (appear once in the triangle list).
    std::set<std::pair<uint32_t, uint32_t>> seen;
    for (const auto& tri : footprintTriangles) {
        for (auto e : {std::pair{tri.v0, tri.v1},
                        std::pair{tri.v1, tri.v2},
                        std::pair{tri.v2, tri.v0}}) {
            if (seen.erase(e) > 0) {
                // Already present → interior edge → skip
            } else {
                seen.insert(e);
            }
        }
    }
    for (const auto& e : seen) {
        uint32_t a = e.first;
        uint32_t b = e.second;
        sm.triangles.emplace_back(bottomIdx[a], bottomIdx[b], topIdx[b]);
        sm.triangles.emplace_back(bottomIdx[a], topIdx[b], topIdx[a]);
    }
}

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
    // Use texture filename as material name so different textures get unique MTL names.
    std::string uri = tex.getImageURI();
    size_t pos = uri.rfind('/');
    m.name = (pos == std::string::npos) ? uri : uri.substr(pos + 1);
    m.textureURI = uri;
    m.mimeType = tex.getMimeType();
    return m;
}

std::vector<TextureCoordinates2D>
MeshGenerator::collectTextureCoords(const Polygon& polygon,
                                    const ParameterizedTexture& tex) {
    std::vector<TextureCoordinates2D> result;

    const std::string& polyId = polygon.getId();
    if (polyId.empty()) return result;

    // Get exterior ring ID for matching with textureCoordinates@ring attribute.
    std::string exteriorRingId;
    if (auto ring = polygon.getExteriorRing()) {
        exteriorRingId = ring->getId();
    }

    const auto& targets = tex.getTargets();
    for (const auto& target : targets) {
        std::string targetId = target.uri;
        if (!targetId.empty() && targetId[0] == '#') {
            targetId = targetId.substr(1);
        }
        if (targetId != polyId) continue;

        for (const auto& tc : target.textureCoords) {
            // tc.ringId has '#' prefix from XML attribute; strip it for comparison.
            std::string ringId = tc.ringId;
            if (!ringId.empty() && ringId[0] == '#') ringId = ringId.substr(1);
            if (ringId != exteriorRingId) continue;
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
void triangulateRings(const LinearRing& exterior,
                      const std::vector<LinearRingPtr>& interiorRings,
                      std::vector<Vertex>& outVertices,
                      std::vector<Triangle>& outTriangles) {
    if (exterior.getPointsCount() < 3) return;

    std::vector<LinearRingPtr> holes;
    for (const auto& r : interiorRings) {
        if (r && r->getPointsCount() >= 3) holes.push_back(r);
    }

    std::vector<Vec3> pts3d = ringToVec3(exterior);
    Vec3 origin, x_axis, y_axis, z_axis;
    std::vector<Vec3> clean3d;
    std::vector<Vec2> clean2d;
    buildLocalFrame(pts3d, nullptr, origin, x_axis, y_axis, z_axis, clean3d, clean2d);
    if (clean3d.size() < 3) return;
    z_axis = {-z_axis.x, -z_axis.y, -z_axis.z};

    std::vector<std::vector<Vec2>> holePoly2d;
    for (const auto& hole : holes) {
        std::vector<Vec3> h3d = ringToVec3(*hole);
        std::vector<Vec3> hc3d;
        std::vector<Vec2> hc2d;
        buildLocalFrame(h3d, nullptr, origin, x_axis, y_axis, z_axis, hc3d, hc2d);
        if (hc2d.size() >= 3) holePoly2d.push_back(hc2d);
    }

    double totalArea = 0;
    for (size_t i = 0; i < clean2d.size(); ++i) {
        const Vec2& a = clean2d[i];
        const Vec2& b = clean2d[(i + 1) % clean2d.size()];
        totalArea += a.u * b.v - b.u * a.v;
    }
    bool exteriorCCW = (totalArea > 0);

    std::list<size_t> remaining;
    for (size_t i = 0; i < clean2d.size(); ++i) remaining.push_back(i);

    while (remaining.size() > 3) {
        bool earFound = false;
        for (auto it = remaining.begin(); it != remaining.end(); ++it) {
            auto prevIt = (it == remaining.begin()) ? std::prev(remaining.end()) : std::prev(it);
            auto nextIt = std::next(it);
            if (nextIt == remaining.end()) nextIt = remaining.begin();

            size_t vi = *it, vp = *prevIt, vn = *nextIt;
            double area = triangleArea2D(
                clean2d[vp].u, clean2d[vp].v,
                clean2d[vi].u, clean2d[vi].v,
                clean2d[vn].u, clean2d[vn].v);

            bool isConvex = exteriorCCW ? (area > 0) : (area < 0);
            if (!isConvex) continue;

            bool anyInside = false;
            for (size_t vk : remaining) {
                if (vk == vp || vk == vi || vk == vn) continue;
                if (pointInTriangle2D(
                        clean2d[vp].u, clean2d[vp].v,
                        clean2d[vi].u, clean2d[vi].v,
                        clean2d[vn].u, clean2d[vn].v,
                        clean2d[vk].u, clean2d[vk].v)) {
                    anyInside = true;
                    break;
                }
            }
            if (anyInside) continue;

            if (edgeIntersectsHoleEdges(clean2d[vp], clean2d[vn], holePoly2d)) continue;

            uint32_t baseIdx = static_cast<uint32_t>(outVertices.size());
            outVertices.emplace_back(clean3d[vp], z_axis, clean2d[vp]);
            outVertices.emplace_back(clean3d[vi], z_axis, clean2d[vi]);
            outVertices.emplace_back(clean3d[vn], z_axis, clean2d[vn]);
            outTriangles.emplace_back(baseIdx, baseIdx + 1, baseIdx + 2);

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
        uint32_t baseIdx = static_cast<uint32_t>(outVertices.size());
        for (size_t idx : remaining) {
            outVertices.emplace_back(clean3d[idx], z_axis, clean2d[idx]);
        }
        outTriangles.emplace_back(baseIdx, baseIdx + 1, baseIdx + 2);
    }
}

std::vector<LinearRingPtr> extractOpeningRings(const AbstractOpening& opening) {
    std::vector<LinearRingPtr> result;
    MultiSurfacePtr ms = opening.getMultiSurface();
    if (!ms) return result;

    for (size_t i = 0; i < ms->getGeometriesCount(); ++i) {
        PolygonPtr poly = ms->getGeometry(i);
        if (!poly) continue;

        LinearRingPtr exterior = poly->getExteriorRing();
        if (exterior && exterior->getPointsCount() >= 3) {
            result.push_back(exterior);
        }
        for (size_t r = 0; r < poly->getInteriorRingsCount(); ++r) {
            LinearRingPtr interior = poly->getInteriorRing(r);
            if (interior && interior->getPointsCount() >= 3) {
                result.push_back(interior);
            }
        }
    }
    return result;
}

std::shared_ptr<MultiSurface> selectMultiSurface(const CityObject& obj, int targetLOD) {
    if (targetLOD == -1) {
        if (obj.getLod4MultiSurface()) return obj.getLod4MultiSurface();
        if (obj.getLod3MultiSurface()) return obj.getLod3MultiSurface();
        if (obj.getLod2MultiSurface()) return obj.getLod2MultiSurface();
        if (obj.getLod1MultiSurface()) return obj.getLod1MultiSurface();
        if (obj.getLod0MultiSurface()) return obj.getLod0MultiSurface();
        if (obj.getLod0FootPrint()) return obj.getLod0FootPrint();
        if (obj.getLod0RoofEdge()) return obj.getLod0RoofEdge();
        return nullptr;
    }
    switch (targetLOD) {
        case 0:
            if (obj.getLod0MultiSurface()) return obj.getLod0MultiSurface();
            if (obj.getLod0FootPrint()) return obj.getLod0FootPrint();
            if (obj.getLod0RoofEdge()) return obj.getLod0RoofEdge();
            return nullptr;
        case 1: return obj.getLod1MultiSurface();
        case 2: return obj.getLod2MultiSurface();
        case 3: return obj.getLod3MultiSurface();
        case 4: return obj.getLod4MultiSurface();
        default: return nullptr;
    }
}

std::shared_ptr<Solid> selectSolid(const CityObject& obj, int targetLOD) {
    if (targetLOD == -1) {
        if (obj.getLod4Solid()) return obj.getLod4Solid();
        if (obj.getLod3Solid()) return obj.getLod3Solid();
        if (obj.getLod2Solid()) return obj.getLod2Solid();
        if (obj.getLod1Solid()) return obj.getLod1Solid();
        return nullptr;
    }
    switch (targetLOD) {
        case 1: return obj.getLod1Solid();
        case 2: return obj.getLod2Solid();
        case 3: return obj.getLod3Solid();
        case 4: return obj.getLod4Solid();
        default: return nullptr;
    }
}

} // namespace citygml