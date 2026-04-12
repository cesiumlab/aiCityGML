#pragma once

#include "mesh/mesh.h"
#include "core/citygml_geometry.h"
#include "core/citygml_surface.h"
#include "core/citygml_object.h"
#include "core/citygml_opening.h"
#include <vector>
#include <memory>

namespace citygml {

// ================================================================
// MeshGeneratorOptions - Options for mesh generation
// ================================================================
struct MeshGeneratorOptions {
    // Which LOD to use (0-4). -1 = highest available.
    int targetLOD = -1;

    // Whether to merge all polygons of a MultiSurface into one mesh.
    bool mergeMultiSurfacePolygons = true;

    // Whether to include opening holes (doors/windows) as boolean cuts.
    bool processOpenings = true;

    // Whether to generate normals for each triangle face.
    bool generateFaceNormals = true;

    // Whether to generate per-vertex normals by averaging adjacent face normals.
    bool generateVertexNormals = false;

    // Whether to keep separate submeshes per material/texture (true)
    // or merge everything into one submesh (false).
    bool separateSubMeshes = true;
};

// ================================================================
// MeshGenerator - Main entry point for triangulating CityGML geometries
// ================================================================
class MeshGenerator {
public:
    explicit MeshGenerator(const MeshGeneratorOptions& options = MeshGeneratorOptions{});

    void setOptions(const MeshGeneratorOptions& options);
    const MeshGeneratorOptions& getOptions() const { return options_; }

    // =================================================================
    // Core triangulation functions
    // =================================================================

    // Triangulate a single LinearRing.
    // For a closed ring: outputs (n-2) triangles using ear-clipping.
    // Interior rings are ignored here; use triangulatePolygon() instead.
    static void triangulateLinearRing(const LinearRing& ring,
                                      std::vector<Vertex>& outVertices,
                                      std::vector<Triangle>& outTriangles);

    // Triangulate a single Polygon (exterior + interior rings).
    // outMaterial will be filled with appearance info from the Polygon.
    static void triangulatePolygon(const Polygon& polygon,
                                   std::vector<Vertex>& outVertices,
                                   std::vector<Triangle>& outTriangles,
                                   Material& outMaterial);

    // Triangulate a MultiSurface (one or more Polygons).
    static void triangulateMultiSurface(const MultiSurface& ms,
                                        Mesh& outMesh,
                                        const Material& defaultMat = Material{});

    // Triangulate a Solid (outer shell = MultiSurface).
    static void triangulateSolid(const Solid& solid,
                                 Mesh& outMesh,
                                 const Material& defaultMat = Material{});

    // =================================================================
    // CityGML object-level functions
    // =================================================================

    // Triangulate all LOD geometries of a CityObject.
    // Calls triangulateMultiSurface() or triangulateSolid() for each LOD level.
    void triangulateCityObject(const CityObject& obj,
                               std::vector<Mesh>& outMeshes) const;

    // Triangulate ThematicSurface bounded by this CityObject (WallSurface, RoofSurface, etc.).
    // Each ThematicSurface may have its own MultiSurface at various LOD levels.
    void triangulateBoundedBySurfaces(const CityObject& obj,
                                      std::vector<Mesh>& outMeshes) const;

    // =================================================================
    // FootPrint & RoofEdge helpers
    // These are stored as MultiSurface on the CityObject at LOD 0.
    // =================================================================

    // Extract and triangulate building footprint (lod0FootPrint).
    static void triangulateFootPrint(const CityObject& obj,
                                     Mesh& outMesh);

    // Extract and triangulate roof edge geometry (lod0RoofEdge).
    static void triangulateRoofEdge(const CityObject& obj,
                                    Mesh& outMesh);

    // =================================================================
    // Opening / hole processing
    // Used to cut holes (doors/windows) from WallSurface polygons.
    // =================================================================

    // Triangulate a WallSurface, cutting holes for each Opening.
    // Internally calls triangulatePolygonWithHoles().
    static void triangulateWallSurface(const WallSurface& wall,
                                       std::vector<Mesh>& outMeshes);

    // Triangulate a Polygon with a list of interior ring "holes".
    // Each hole is an Opening converted to its LinearRing boundary.
    static void triangulatePolygonWithHoles(const Polygon& surface,
                                             const std::vector<LinearRingPtr>& holes,
                                             std::vector<Vertex>& outVertices,
                                             std::vector<Triangle>& outTriangles,
                                             Material& outMaterial);

    // =================================================================
    // Thematic surface helpers
    // =================================================================

    // Triangulate a RoofSurface.
    static void triangulateRoofSurface(const RoofSurface& roof,
                                       Mesh& outMesh);

    // Triangulate a GroundSurface (building footprint extruded).
    static void triangulateGroundSurface(const GroundSurface& ground,
                                         Mesh& outMesh);

    // Triangulate an arbitrary ThematicSurface (Roof/Wall/Ground/etc.).
    static void triangulateThematicSurface(const AbstractThematicSurface& surface,
                                            Mesh& outMesh);

    // =================================================================
    // ImplicitGeometry support
    // =================================================================

    // Instantiate an ImplicitGeometry at its reference point.
    // Applies the transformation matrix, then triangulates the relative geometry.
    static void triangulateImplicitGeometry(const ImplicitGeometry& impl,
                                             Mesh& outMesh);

    // =================================================================
    // Utility functions
    // =================================================================

    // Compute face normal for a triangle (right-hand rule, outward for a closed solid).
    static Vec3 computeFaceNormal(const Vertex& v0, const Vertex& v1, const Vertex& v2);

    // Compute per-vertex normals by averaging adjacent face normals.
    static void computeVertexNormals(std::vector<Vertex>& vertices,
                                     const std::vector<Triangle>& triangles);

    // Extrude a flat 2D footprint polygon along a height vector to form a prism.
    // topCenter + bottomCenter define the extrusion axis.
    static void extrudeFootPrint(const std::vector<Vertex>& footprintVertices,
                                 const std::vector<Triangle>& footprintTriangles,
                                 const Vec3& extrusionDir,
                                 double height,
                                 Mesh& outMesh,
                                 const Material& defaultMat = Material{});

    // Convert X3DMaterial from CityGML appearance to Mesh Material.
    static Material fromX3DMaterial(const X3DMaterial& mat);

    // Convert ParameterizedTexture from CityGML appearance to Mesh Material + UVs.
    static Material fromParameterizedTexture(const ParameterizedTexture& tex);

    // Collect texture coordinates for a Polygon from ParameterizedTexture bindings.
    static std::vector<TextureCoordinates2D>
    collectTextureCoords(const Polygon& polygon,
                          const ParameterizedTexture& tex);

private:
    MeshGeneratorOptions options_;

    Mesh triangulateCityObjectLOD(const CityObject& obj, int lod) const;
    Mesh triangulateSolidLOD(const CityObject& obj, int lod) const;
};

// ================================================================
// Helper free functions (can be used without MeshGenerator instance)
// ================================================================

// Triangulate a Polygon with arbitrary ring list (exterior first, then holes).
void triangulateRings(const LinearRing& exterior,
                     const std::vector<LinearRingPtr>& interiorRings,
                     std::vector<Vertex>& outVertices,
                     std::vector<Triangle>& outTriangles);

// Extract LinearRings from an Opening (Window/Door).
std::vector<LinearRingPtr> extractOpeningRings(const AbstractOpening& opening);

// Pick the best LOD geometry from a CityObject based on options.
std::shared_ptr<MultiSurface> selectMultiSurface(const CityObject& obj,
                                                  int targetLOD);
std::shared_ptr<Solid> selectSolid(const CityObject& obj,
                                    int targetLOD);

} // namespace citygml
