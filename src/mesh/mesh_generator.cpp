#include "mesh_generator.h"
#include "core/citygml_appearance.h"
#include "mesh.h"
#include <mapbox/earcut.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <set>
#include <tuple>
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

// EarcutPoint: an alias for std::array<double,2>.
// mapbox::earcut natively supports std::array via its std::tuple_element specializations.
using EarcutPoint = std::array<double, 2>;

} // namespace citygml

namespace citygml {

// Mapbox Earcut triangulation for a simple polygon (no holes).
// Input: 2D points (any winding order).
// Output: N-2 triangles as flat index array.
static std::vector<uint32_t> mapboxEarcutTriangulate(const std::vector<Vec2>& pts2d) {
    std::vector<EarcutPoint> polygon;
    polygon.reserve(pts2d.size());
    for (const auto& p : pts2d) {
        polygon.push_back({p.u, p.v});
    }

    std::vector<std::vector<EarcutPoint>> data;
    data.push_back(std::move(polygon));

    std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(data);
    return indices;
}

// Signed area of triangle (a,b,c) = cross((b-a), (c-a)).
static double triangleArea2D(double ax, double ay, double bx, double by, double cx, double cy) {
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
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

// Build local coordinate system:
//   origin  = pts[0]
//   x_axis  = normalize(pts[1] - pts[0])
//   z_axis  = outward 3D normal (from full polygon shoelace)
//   y_axis  = z_axis x x_axis   (right-handed: CCW in UV = outward in 3D)
// Project all unique ring points onto this frame.
// Output: out3d = 3D positions, out2d = 2D projection (for earcut only).
static void buildLocalFrame(const std::vector<Vec3>& pts,
                           Vec3& origin, Vec3& x_axis, Vec3& y_axis, Vec3& z_axis,
                           std::vector<Vec3>& out3d,
                           std::vector<Vec2>& out2d) {
    out3d.clear();
    out2d.clear();
    size_t n = pts.size();
    if (n < 3) return;

    // Compute outward 3D normal from all polygon vertices (shoelace in 3D).
    Vec3 area(0, 0, 0);
    for (size_t i = 0; i < n; ++i) {
        const Vec3& v0 = pts[i];
        const Vec3& v1 = pts[(i + 1) % n];
        area.x += v0.y * v1.z - v0.z * v1.y;
        area.y += v0.z * v1.x - v0.x * v1.z;
        area.z += v0.x * v1.y - v0.y * v1.x;
    }
    double rawZlen = std::sqrt(area.x * area.x + area.y * area.y + area.z * area.z);
    if (rawZlen < 1e-10) return;  // degenerate polygon
    z_axis = { area.x / rawZlen, area.y / rawZlen, area.z / rawZlen };

    // x_axis = normalize(pts[1] - pts[0])  [first edge]
    origin = pts[0];
    x_axis = pts[1] - origin;
    double xlen = std::sqrt(x_axis.x * x_axis.x + x_axis.y * x_axis.y + x_axis.z * x_axis.z);
    if (xlen < 1e-10) return;
    x_axis = { x_axis.x / xlen, x_axis.y / xlen, x_axis.z / xlen };

    // y_axis = z_axis x x_axis  (right-handed: CCW in UV = outward in 3D)
    y_axis = vecCross(z_axis, x_axis);
    double ylen = std::sqrt(y_axis.x * y_axis.x + y_axis.y * y_axis.y + y_axis.z * y_axis.z);
    if (ylen < 1e-10) {
        // Degenerate: pick safe fallback
        Vec3 ry = vecCross({0, 0, 1}, x_axis);
        double rylen = std::sqrt(ry.x * ry.x + ry.y * ry.y + ry.z * ry.z);
        y_axis = rylen > 1e-10 ? Vec3{ry.x / rylen, ry.y / rylen, ry.z / rylen} : Vec3{0, 1, 0};
    } else {
        y_axis = { y_axis.x / ylen, y_axis.y / ylen, y_axis.z / ylen };
    }

    // Ensure y_axis points toward +Z so that earcut's CCW winding
    // maps to outward-facing normals regardless of the polygon's slant.
    if (y_axis.z < 0) {
        y_axis = {-y_axis.x, -y_axis.y, -y_axis.z};
    }

    // Skip closing duplicate point (first == last).
    size_t start = 0;
    if (n >= 2 &&
        std::abs(pts[0].x - pts[n - 1].x) < 1e-8 &&
        std::abs(pts[0].y - pts[n - 1].y) < 1e-8 &&
        std::abs(pts[0].z - pts[n - 1].z) < 1e-8) {
        start = 1;
    }
    size_t uniq = n - start;
    if (uniq < 3) return;

    // Project all unique points and compute 2D local UVs.
    // Also build raw2d for collinearity check (must use abs(area)).
    std::vector<Vec2> raw2d;
    raw2d.reserve(uniq);
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
            out2d.push_back(curr);
        }
    }
}

// Compute signed area of polygon (positive = CCW, negative = CW).
static double polygonSignedArea2D(const std::vector<Vec2>& pts) {
    double area = 0;
    size_t n = pts.size();
    for (size_t i = 0; i < n; ++i) {
        const Vec2& p0 = pts[i];
        const Vec2& p1 = pts[(i + 1) % n];
        area += p0.u * p1.v - p1.u * p0.v;
    }
    return area * 0.5;
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

// Full triangulateRing with optional real texture coordinates.
// realUVs: if provided, must be parallel to pts (same count as pts).
//          Collinear-point removal is synchronized so clean3d[i] uses realUVs[origIndex[i]].
//          If null, Vertex.uv is omitted.
static void triangulateRing(const std::vector<Vec3>& pts,
                           const std::vector<Vec2>* realUVs,
                           std::vector<Vertex>& outVerts,
                           std::vector<Triangle>& outTris) {
    if (pts.size() < 3) return;

    Vec3 origin, x_axis, y_axis, z_axis;
    std::vector<Vec3> clean3d;
    std::vector<Vec2> clean2d;
    buildLocalFrame(pts, origin, x_axis, y_axis, z_axis, clean3d, clean2d);

    if (clean3d.size() < 3) return;

    std::vector<uint32_t> indices = mapboxEarcutTriangulate(clean2d);
    if (indices.empty()) return;

    // earcut produces triangles with CCW winding in 2D. If our outer ring is CW,
    // flip all indices to maintain consistent CCW winding in 3D.
    double polygonArea = 0.0;
    for (size_t i = 0; i < clean2d.size(); ++i) {
        const Vec2& p0 = clean2d[i];
        const Vec2& p1 = clean2d[(i + 1) % clean2d.size()];
        polygonArea += p0.u * p1.v - p1.u * p0.v;
    }
    bool flipRing = (polygonArea > 0.0);

    // Build cleanUVs: parallel to clean3d, using original UV indices.
    // The buildLocalFrame removal tracks original indices starting from 1.
    std::vector<Vec2> cleanUVs;
    if (realUVs && !realUVs->empty()) {
        size_t n = pts.size();
        for (size_t i = 1; i < n; ++i) {
            // Same collinearity check as buildLocalFrame:
            size_t prev = (i == 1) ? 0 : (i - 1);
            size_t next = ((i + 1) == n) ? 0 : (i + 1);
            Vec2 pPrev = { pts[prev].x, pts[prev].z };
            Vec2 pCurr = { pts[i].x, pts[i].z };
            Vec2 pNext = { pts[next].x, pts[next].z };
            if (std::abs(triangleArea2D(pPrev.u, pPrev.v, pCurr.u, pCurr.v, pNext.u, pNext.v)) > 1e-10) {
                if (i < realUVs->size()) {
                    cleanUVs.push_back((*realUVs)[i]);
                }
            }
        }
        // buildLocalFrame also includes pts[0] at the start
        if (0 < realUVs->size()) {
            cleanUVs.insert(cleanUVs.begin(), (*realUVs)[0]);
        }
    }

    uint32_t baseIdx = static_cast<uint32_t>(outVerts.size());
    for (size_t i = 0; i < clean3d.size(); ++i) {
        Vec3 n{-z_axis.x, -z_axis.y, -z_axis.z};
        if (!cleanUVs.empty() && i < cleanUVs.size()) {
            outVerts.emplace_back(clean3d[i], n, cleanUVs[i]);
        } else {
            outVerts.emplace_back(clean3d[i], n);
        }
    }
    for (size_t i = 0; i < indices.size(); i += 3) {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];
        if (flipRing) {
            outTris.emplace_back(baseIdx + i0, baseIdx + i2, baseIdx + i1);
        } else {
            outTris.emplace_back(baseIdx + i0, baseIdx + i1, baseIdx + i2);
        }
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

void MeshGenerator::generate(const CityModel& cityModel,
                              std::vector<Mesh>& outMeshes) {
    const auto& objects = cityModel.getCityObjects();
    for (const auto& obj : objects) {
        if (!obj) continue;
        triangulateCityObject(*obj, outMeshes);
    }
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
                                             const Material& defaultMat,
                                             const std::set<std::string>* processedPolygonIds) {
    outMesh.clear();
    outMesh.name = ms.getId().empty() ? "MultiSurface" : ms.getId();

    size_t count = ms.getGeometriesCount();
    if (count == 0) return;

    for (size_t i = 0; i < count; ++i) {
        PolygonPtr poly = ms.getGeometry(i);
        if (!poly) continue;

        std::string polyId = poly->getId();
        if (processedPolygonIds && !polyId.empty() &&
            processedPolygonIds->find(polyId) != processedPolygonIds->end()) {
            continue; // Already processed by Solid/MultiSurface, skip to avoid duplicate
        }

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

        // Triangulate polygon (with or without holes).
        std::vector<Vertex> verts;
        std::vector<Triangle> tris;
        if (poly->getInteriorRingsCount() > 0) {
            std::vector<LinearRingPtr> holes;
            for (size_t r = 0; r < poly->getInteriorRingsCount(); ++r) {
                if (auto ring = poly->getInteriorRing(r)) {
                    holes.push_back(ring);
                }
            }
            triangulatePolygonWithHoles(*poly, holes, verts, tris, mat);
        } else {
            triangulateRing(ringToVec3(*exterior), uvPtr, verts, tris);
        }

        if (verts.empty() || tris.empty()) continue;

        SubMesh& sm = outMesh.addSubMesh(mat);
        sm.vertices = std::move(verts);
        sm.triangles = std::move(tris);
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
// Helper: collect all Polygon IDs from a geometry tree (for deduplication)
static void collectPolygonIds(const citygml::AbstractGeometry* geom, std::set<std::string>& ids) {
    if (!geom) return;

    if (auto polygon = dynamic_cast<const citygml::Polygon*>(geom)) {
        std::string pid = polygon->getId();
        if (!pid.empty()) ids.insert(pid);
    } else if (auto ms = dynamic_cast<const citygml::MultiSurface*>(geom)) {
        for (size_t i = 0; i < ms->getGeometriesCount(); ++i) {
            collectPolygonIds(ms->getGeometry(i).get(), ids);
        }
    } else if (auto solid = dynamic_cast<const citygml::Solid*>(geom)) {
        if (auto shell = solid->getOuterShell()) {
            collectPolygonIds(shell.get(), ids);
        }
    }
}

void MeshGenerator::triangulateBoundedBySurfaces(const CityObject& obj,
                                                  std::vector<Mesh>& outMeshes,
                                                  const std::set<std::string>* processedPolygonIds) const {
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
            // 其他表面类型：直接三角化其 MultiSurface，跳过已处理的 Polygon
            Mesh m;
            triangulateMultiSurface(*ms, m, Material{}, processedPolygonIds);
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

    // Collect Polygon IDs from candidates and Solids to avoid duplicates in BoundedBy surfaces
    std::set<std::string> processedPolygonIds;
    for (auto& ms : candidates) {
        collectPolygonIds(ms.get(), processedPolygonIds);
    }
    for (auto& solid : solidCandidates) {
        collectPolygonIds(solid.get(), processedPolygonIds);
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

    // Triangulate boundedBy ThematicSurfaces only for the HIGHEST available LOD.
    // In CityGML, BoundedBy surfaces (WallSurface/RoofSurface/etc.) belong to the
    // highest-LOD Solid that carries them. They should NOT be duplicated across LOD levels.
    // LOD0 has no Solid, so BoundedBy surfaces do not apply there.
    int maxLod = -1;
    if (obj.getLod4Solid() || obj.getLod4MultiSurface()) maxLod = 4;
    else if (obj.getLod3Solid() || obj.getLod3MultiSurface()) maxLod = 3;
    else if (obj.getLod2Solid() || obj.getLod2MultiSurface()) maxLod = 2;
    else if (obj.getLod1Solid() || obj.getLod1MultiSurface()) maxLod = 1;
    else if (obj.getLod0MultiSurface() || obj.getLod0FootPrint() || obj.getLod0RoofEdge()) maxLod = 0;

    if (options_.targetLOD == maxLod && maxLod > 0) {
        triangulateBoundedBySurfaces(obj, outMeshes, &processedPolygonIds);
    }

    // Triangulate Room geometries (bldg:interiorRoom) if enabled
    if (options_.exportRooms && lod == maxLod) {
        triangulateRooms(obj, outMeshes, &processedPolygonIds);
    }
}

// ================================================================
// Room triangulation (bldg:interiorRoom)
// ================================================================
void MeshGenerator::triangulateRooms(const CityObject& obj,
                                      std::vector<Mesh>& outMeshes,
                                      const std::set<std::string>* processedPolygonIds) const {
    const auto& children = obj.getChildCityObjects();

    for (const auto& child : children) {
        if (!child) continue;

        // 只处理 BuildingRoom 类型的子对象
        if (child->getObjectType() != "BuildingRoom") continue;

        const CityObject* room = child.get();

        // 遍历所有 LOD 级别（LOD 2, 3, 4 - Room 通常没有 LOD 0/1）
        std::vector<std::shared_ptr<MultiSurface>> candidates;
        std::vector<std::shared_ptr<Solid>> solidCandidates;

        auto tryAdd = [&](auto ptr) {
            if (ptr) candidates.push_back(ptr);
        };
        auto tryAddSolid = [&](auto ptr) {
            if (ptr) solidCandidates.push_back(ptr);
        };

        int roomLod = options_.targetLOD;
        // Room 通常在 LOD 2, 3, 4 有几何数据
        if (roomLod == -1 || roomLod == 4) { tryAdd(room->getLod4MultiSurface()); tryAddSolid(room->getLod4Solid()); }
        if (roomLod == -1 || roomLod == 3) { tryAdd(room->getLod3MultiSurface()); tryAddSolid(room->getLod3Solid()); }
        if (roomLod == -1 || roomLod == 2) { tryAdd(room->getLod2MultiSurface()); tryAddSolid(room->getLod2Solid()); }
        if (roomLod == -1 || roomLod == 1) { tryAdd(room->getLod1MultiSurface()); tryAddSolid(room->getLod1Solid()); }
        if (roomLod == -1 || roomLod == 0) { tryAdd(room->getLod0MultiSurface()); }

        // Triangulate MultiSurface candidates
        for (auto& ms : candidates) {
            Mesh tmp;
            triangulateMultiSurface(*ms, tmp, Material{}, processedPolygonIds);
            if (!tmp.isEmpty()) {
                for (SubMesh& sm : tmp.subMeshes) {
                    Mesh outMesh;
                    outMesh.name = room->getId().empty()
                                       ? ("Room_" + std::to_string(outMeshes.size()))
                                       : room->getId();
                    outMesh.addSubMesh(sm.material).vertices = sm.vertices;
                    outMesh.subMeshes.back().triangles = sm.triangles;
                    outMeshes.push_back(std::move(outMesh));
                }
            }
        }

        // Triangulate Solid candidates
        for (auto& solid : solidCandidates) {
            Mesh m;
            triangulateSolid(*solid, m);
            if (!m.isEmpty()) {
                m.name = room->getId().empty()
                             ? ("Room_" + std::to_string(outMeshes.size()))
                             : room->getId();
                outMeshes.push_back(std::move(m));
            }
        }

        // Triangulate boundedBy surfaces for the room (interior surfaces like FloorSurface, CeilingSurface, etc.)
        triangulateBoundedBySurfaces(*room, outMeshes, processedPolygonIds);
    }
}

void MeshGenerator::triangulateOnlyRooms(const CityModel& cityModel,
                                          std::vector<Mesh>& outMeshes) const {
    const auto& objects = cityModel.getCityObjects();

    for (const auto& obj : objects) {
        if (!obj) continue;

        const auto& children = obj->getChildCityObjects();
        for (const auto& child : children) {
            if (!child) continue;
            if (child->getObjectType() != "BuildingRoom") continue;

            const CityObject* room = child.get();

            // Collect LOD geometries
            std::vector<std::shared_ptr<MultiSurface>> candidates;
            std::vector<std::shared_ptr<Solid>> solidCandidates;

            auto tryAdd = [&](auto ptr) {
                if (ptr) candidates.push_back(ptr);
            };
            auto tryAddSolid = [&](auto ptr) {
                if (ptr) solidCandidates.push_back(ptr);
            };

            int roomLod = options_.targetLOD;
            if (roomLod == -1 || roomLod == 4) { tryAdd(room->getLod4MultiSurface()); tryAddSolid(room->getLod4Solid()); }
            if (roomLod == -1 || roomLod == 3) { tryAdd(room->getLod3MultiSurface()); tryAddSolid(room->getLod3Solid()); }
            if (roomLod == -1 || roomLod == 2) { tryAdd(room->getLod2MultiSurface()); tryAddSolid(room->getLod2Solid()); }
            if (roomLod == -1 || roomLod == 1) { tryAdd(room->getLod1MultiSurface()); tryAddSolid(room->getLod1Solid()); }
            if (roomLod == -1 || roomLod == 0) { tryAdd(room->getLod0MultiSurface()); }

            // Triangulate MultiSurface
            for (auto& ms : candidates) {
                Mesh tmp;
                triangulateMultiSurface(*ms, tmp, Material{}, nullptr);
                if (!tmp.isEmpty()) {
                    for (SubMesh& sm : tmp.subMeshes) {
                        Mesh outMesh;
                        outMesh.name = room->getId().empty()
                                           ? ("Room_" + std::to_string(outMeshes.size()))
                                           : room->getId();
                        outMesh.addSubMesh(sm.material).vertices = sm.vertices;
                        outMesh.subMeshes.back().triangles = sm.triangles;
                        outMeshes.push_back(std::move(outMesh));
                    }
                }
            }

            // Triangulate Solid
            for (auto& solid : solidCandidates) {
                Mesh m;
                triangulateSolid(*solid, m);
                if (!m.isEmpty()) {
                    m.name = room->getId().empty()
                                 ? ("Room_" + std::to_string(outMeshes.size()))
                                 : room->getId();
                    outMeshes.push_back(std::move(m));
                }
            }

            // Triangulate room's boundedBy surfaces
            triangulateBoundedBySurfaces(*room, outMeshes, nullptr);
        }
    }
}

void MeshGenerator::triangulateRoomByIndex(const CityModel& cityModel,
                                            std::vector<Mesh>& outMeshes) const {
    if (options_.roomIndex < 0) {
        triangulateOnlyRooms(cityModel, outMeshes);
        return;
    }

    const auto& objects = cityModel.getCityObjects();

    for (const auto& obj : objects) {
        if (!obj) continue;

        const auto& children = obj->getChildCityObjects();

        int roomCount = 0;
        for (const auto& child : children) {
            if (child && child->getObjectType() == "BuildingRoom") {
                if (roomCount == options_.roomIndex) {
                    const CityObject* room = child.get();

                    std::vector<std::shared_ptr<MultiSurface>> candidates;
                    std::vector<std::shared_ptr<Solid>> solidCandidates;

                    auto tryAdd = [&](auto ptr) {
                        if (ptr) candidates.push_back(ptr);
                    };
                    auto tryAddSolid = [&](auto ptr) {
                        if (ptr) solidCandidates.push_back(ptr);
                    };

                    int roomLod = options_.targetLOD;
                    if (roomLod == -1 || roomLod == 4) { tryAdd(room->getLod4MultiSurface()); tryAddSolid(room->getLod4Solid()); }
                    if (roomLod == -1 || roomLod == 3) { tryAdd(room->getLod3MultiSurface()); tryAddSolid(room->getLod3Solid()); }
                    if (roomLod == -1 || roomLod == 2) { tryAdd(room->getLod2MultiSurface()); tryAddSolid(room->getLod2Solid()); }
                    if (roomLod == -1 || roomLod == 1) { tryAdd(room->getLod1MultiSurface()); tryAddSolid(room->getLod1Solid()); }
                    if (roomLod == -1 || roomLod == 0) { tryAdd(room->getLod0MultiSurface()); }

                    for (auto& ms : candidates) {
                        Mesh tmp;
                        triangulateMultiSurface(*ms, tmp, Material{}, nullptr);
                        if (!tmp.isEmpty()) {
                            for (SubMesh& sm : tmp.subMeshes) {
                                Mesh outMesh;
                                outMesh.name = room->getId().empty()
                                                   ? ("Room_" + std::to_string(options_.roomIndex))
                                                   : room->getId();
                                outMesh.addSubMesh(sm.material).vertices = sm.vertices;
                                outMesh.subMeshes.back().triangles = sm.triangles;
                                outMeshes.push_back(std::move(outMesh));
                            }
                        }
                    }

                    for (auto& solid : solidCandidates) {
                        Mesh m;
                        triangulateSolid(*solid, m);
                        if (!m.isEmpty()) {
                            m.name = room->getId().empty()
                                         ? ("Room_" + std::to_string(options_.roomIndex))
                                         : room->getId();
                            outMeshes.push_back(std::move(m));
                        }
                    }

                    triangulateBoundedBySurfaces(*room, outMeshes, nullptr);
                    return;
                }
                roomCount++;
            }
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

// Signed area of polygon ring (shoelace formula).
// Positive = CCW, Negative = CW.
static double polygonSignedArea(const std::vector<Vec2>& ring) {
    double area = 0;
    size_t n = ring.size();
    for (size_t i = 0; i < n; ++i) {
        const Vec2& a = ring[i];
        const Vec2& b = ring[(i + 1) % n];
        area += a.u * b.v - b.u * a.v;
    }
    return area * 0.5;
}

// Find the ring index with the largest absolute signed area.
// Used to determine which ring is the exterior (largest) vs holes (smaller).
static size_t findExteriorRingIndex(const std::vector<std::vector<Vec2>>& rings) {
    size_t best = 0;
    double bestArea = std::abs(polygonSignedArea(rings[0]));
    for (size_t i = 1; i < rings.size(); ++i) {
        double a = std::abs(polygonSignedArea(rings[i]));
        if (a > bestArea) {
            bestArea = a;
            best = i;
        }
    }
    return best;
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
    std::vector<Vec2> exteriorUv;

    if (auto tex = surface.getTexture()) {
        std::vector<TextureCoordinates2D> tc = collectTextureCoords(surface, *tex);
        if (!tc.empty() && !tc[0].coordinates.empty()) {
            exteriorUv = tc[0].coordinates;
            if (exteriorUv.size() > pts3d.size()) {
                exteriorUv.resize(pts3d.size());
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
    buildLocalFrame(pts3d, origin, x_axis, y_axis, z_axis, clean3d, clean2d);
    if (clean3d.size() < 3) return;

    // Build cleanUVs for exterior ring (parallel to clean3d).
    std::vector<Vec2> cleanUVs;
    if (!exteriorUv.empty()) {
        // Re-apply same collinear removal to match clean3d.
        size_t n = pts3d.size();
        for (size_t i = 1; i < n; ++i) {
            size_t prev = (i == 1) ? 0 : (i - 1);
            size_t next = ((i + 1) == n) ? 0 : (i + 1);
            Vec2 pPrev = { pts3d[prev].x, pts3d[prev].z };
            Vec2 pCurr = { pts3d[i].x, pts3d[i].z };
            Vec2 pNext = { pts3d[next].x, pts3d[next].z };
            if (std::abs(triangleArea2D(pPrev.u, pPrev.v, pCurr.u, pCurr.v, pNext.u, pNext.v)) > 1e-10) {
                if (i < exteriorUv.size()) {
                    cleanUVs.push_back(exteriorUv[i]);
                }
            }
        }
        if (!exteriorUv.empty()) {
            cleanUVs.insert(cleanUVs.begin(), exteriorUv[0]);
        }
    }

    // Project each hole ring to 2D using the exterior's local frame.
    struct HoleData {
        std::vector<Vec3> pts3d;
        std::vector<Vec2> pts2d;
    };
    std::vector<HoleData> holesData;
    for (const auto& hole : holes) {
        if (!hole || hole->getPointsCount() < 3) continue;
        std::vector<Vec3> h3d = ringToVec3(*hole);
        std::vector<Vec3> h3dClean;
        std::vector<Vec2> h2d;
        buildLocalFrame(h3d, origin, x_axis, y_axis, z_axis, h3dClean, h2d);
        if (h2d.size() >= 3) {
            holesData.push_back({std::move(h3dClean), std::move(h2d)});
        }
    }

    // Build ring data for earcut and 3D output.
    struct RingData {
        std::vector<Vec2> pts2d;
        std::vector<Vec3> pts3d;
    };
    std::vector<RingData> allRings;
    allRings.push_back({clean2d, clean3d});
    for (auto& hd : holesData) {
        allRings.push_back({hd.pts2d, hd.pts3d});
    }

    // Find exterior ring (largest area) using 2D projected coordinates.
    std::vector<std::vector<Vec2>> rings2d;
    rings2d.reserve(allRings.size());
    for (auto& r : allRings) rings2d.push_back(r.pts2d);
    size_t exteriorIdx = findExteriorRingIndex(rings2d);

    // Reorder so exterior ring is first.
    if (exteriorIdx != 0) {
        std::swap(allRings[0], allRings[exteriorIdx]);
    }

    // Convert to EarcutPoint for earcut.
    std::vector<std::vector<EarcutPoint>> earcutData;
    earcutData.reserve(allRings.size());
    for (const auto& ring : allRings) {
        std::vector<EarcutPoint> wrapped;
        wrapped.reserve(ring.pts2d.size());
        for (const auto& p : ring.pts2d) wrapped.push_back({p.u, p.v});
        earcutData.push_back(std::move(wrapped));
    }

    std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(earcutData);
    if (indices.empty()) return;

    // Emit 3D vertices: exterior has UV if available, holes have no UV.
    bool hasExteriorUV = (!cleanUVs.empty() && cleanUVs.size() == allRings[0].pts3d.size());
    for (size_t ri = 0; ri < allRings.size(); ++ri) {
        for (size_t i = 0; i < allRings[ri].pts3d.size(); ++i) {
            if (ri == 0 && hasExteriorUV) {
                outVertices.emplace_back(allRings[ri].pts3d[i], z_axis, cleanUVs[i]);
            } else {
                outVertices.emplace_back(allRings[ri].pts3d[i], z_axis);
            }
        }
    }

    // Emit triangles directly from earcut output (no winding swap).
    for (size_t i = 0; i < indices.size(); i += 3) {
        outTriangles.emplace_back(indices[i], indices[i + 2], indices[i + 1]);
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
    // 生成唯一的材质名称，基于 diffuseColor 的 RGB 值
    const auto& dc = mat.getDiffuseColor();
    if (dc.size() >= 3) {
        m.diffuseR = dc[0]; m.diffuseG = dc[1]; m.diffuseB = dc[2];
        if (dc.size() >= 4) m.diffuseA = dc[3];

        // 根据 RGB 值生成唯一的材质名称，避免多个 X3DMaterial 使用相同名称
        std::ostringstream oss;
        oss << "mat_" << std::fixed << std::setprecision(3)
            << dc[0] << "_" << dc[1] << "_" << dc[2];
        m.name = oss.str();
    } else {
        m.name = "X3DMaterial";
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
    buildLocalFrame(pts3d, origin, x_axis, y_axis, z_axis, clean3d, clean2d);
    if (clean3d.size() < 3) return;

    // For earcut: use projected 2D (in exterior's frame).
    // For 3D output: use each ring's ORIGINAL 3D coordinates.
    struct HoleData {
        std::vector<Vec3> pts3d;
        std::vector<Vec2> pts2d;
    };
    std::vector<HoleData> holesData;
    for (const auto& hole : holes) {
        std::vector<Vec3> h3d = ringToVec3(*hole);
        std::vector<Vec3> hc3d;
        std::vector<Vec2> hc2d;
        buildLocalFrame(h3d, origin, x_axis, y_axis, z_axis, hc3d, hc2d);
        if (hc2d.size() >= 3) {
            holesData.push_back({std::move(hc3d), std::move(hc2d)});
        }
    }

    struct RingData {
        std::vector<Vec2> pts2d;
        std::vector<Vec3> pts3d;
    };
    std::vector<RingData> allRings;
    allRings.push_back({clean2d, clean3d});
    for (auto& hd : holesData) {
        allRings.push_back({hd.pts2d, hd.pts3d});
    }

    // Extract 2D rings for exterior index detection and winding check.
    std::vector<std::vector<Vec2>> rings2d;
    rings2d.reserve(allRings.size());
    for (auto& r : allRings) rings2d.push_back(r.pts2d);
    size_t exteriorIdx = findExteriorRingIndex(rings2d);
    if (exteriorIdx != 0) {
        std::swap(allRings[0], allRings[exteriorIdx]);
        std::swap(rings2d[0], rings2d[exteriorIdx]);
    }

    // earcut expects outer ring to be CCW. If outer ring is CW (negative area),
    // flip it so earcut produces correct triangulation.
    double exteriorArea = 0.0;
    const auto& ext2d = rings2d[0];
    for (size_t i = 0; i < ext2d.size(); ++i) {
        const Vec2& p0 = ext2d[i];
        const Vec2& p1 = ext2d[(i + 1) % ext2d.size()];
        exteriorArea += p0.u * p1.v - p1.u * p0.v;
    }
    bool needFlipOuter = (exteriorArea < 0.0);
    if (needFlipOuter) {
        // Flip outer ring 2D points in rings2d (used for earcut).
        std::reverse(rings2d[0].begin(), rings2d[0].end());
        // Also flip allRings[0] so 3D vertices and 2D coords stay in sync.
        std::reverse(allRings[0].pts2d.begin(), allRings[0].pts2d.end());
        std::reverse(allRings[0].pts3d.begin(), allRings[0].pts3d.end());
        // Flip z_axis since the ring winding is reversed.
        z_axis = Vec3(-z_axis.x, -z_axis.y, -z_axis.z);
    }

    std::vector<std::vector<EarcutPoint>> earcutData;
    earcutData.reserve(allRings.size());
    for (const auto& ring : rings2d) {
        std::vector<EarcutPoint> wrapped;
        wrapped.reserve(ring.size());
        for (const auto& p : ring) wrapped.push_back({p.u, p.v});
        earcutData.push_back(std::move(wrapped));
    }

    std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(earcutData);
    if (indices.empty()) return;

    // Build vertices for all rings. earcut produces CCW triangles, no index flipping needed.
    size_t baseIdx = outVertices.size();
    for (const auto& ring : allRings) {
        for (size_t i = 0; i < ring.pts3d.size(); ++i) {
            outVertices.emplace_back(ring.pts3d[i], z_axis, ring.pts2d[i]);
        }
    }

    for (size_t i = 0; i < indices.size(); i += 3) {
        outTriangles.emplace_back(baseIdx + indices[i], baseIdx + indices[i + 2], baseIdx + indices[i + 1]);
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