#pragma once

#include "citygml/core/citygml_feature.h"

namespace citygml {

// ============================================================
// 前向声明
// ============================================================

class AbstractGeometry;

// ============================================================
// AbstractGeometry - 几何基类
// ============================================================

class AbstractGeometry : public CityGMLObject {
public:
    virtual ~AbstractGeometry() = default;
    virtual GeometryType getType() const = 0;
    virtual bool isEmpty() const = 0;
};

using AbstractGeometryPtr = std::shared_ptr<AbstractGeometry>;

// ============================================================
// Point - 点
// ============================================================

class Point : public AbstractGeometry {
public:
    std::array<double, 3> coords = {0, 0, 0};
    
    GeometryType getType() const override { return GeometryType::GT_Point; }
    bool isEmpty() const override { return false; }
};

using PointPtr = std::shared_ptr<Point>;

// ============================================================
// MultiPoint - 点集
// ============================================================

class MultiPoint : public AbstractGeometry {
public:
    std::vector<Point> points;
    
    GeometryType getType() const override { return GeometryType::GT_MultiPoint; }
    bool isEmpty() const override { return points.empty(); }
};

using MultiPointPtr = std::shared_ptr<MultiPoint>;

// ============================================================
// Curve - 曲线基类
// ============================================================

class Curve : public AbstractGeometry {
public:
    std::vector<std::array<double, 3>> points;
    
    GeometryType getType() const override { return GeometryType::GT_Curve; }
    bool isEmpty() const override { return points.empty(); }
};

using CurvePtr = std::shared_ptr<Curve>;

// ============================================================
// LineString - 线串
// ============================================================

class LineString : public Curve {
public:
    GeometryType getType() const override { return GeometryType::GT_LineString; }
};

using LineStringPtr = std::shared_ptr<LineString>;

// ============================================================
// MultiCurve - 多曲线
// ============================================================

class MultiCurve : public AbstractGeometry {
public:
    std::vector<LineStringPtr> curves;
    
    GeometryType getType() const override { return GeometryType::GT_MultiCurve; }
    bool isEmpty() const override { return curves.empty(); }
};

using MultiCurvePtr = std::shared_ptr<MultiCurve>;

// ============================================================
// Surface - 表面基类
// ============================================================

class Surface : public AbstractGeometry {
public:
    std::vector<std::array<double, 3>> exteriorRing;
    std::vector<std::vector<std::array<double, 3>>> interiorRings;
    
    GeometryType getType() const override { return GeometryType::GT_Surface; }
    bool isEmpty() const override { return exteriorRing.empty(); }
};

using SurfacePtr = std::shared_ptr<Surface>;

// ============================================================
// Polygon - 多边形
// ============================================================

class Polygon : public Surface {
public:
    GeometryType getType() const override { return GeometryType::GT_Polygon; }
};

using PolygonPtr = std::shared_ptr<Polygon>;

// ============================================================
// MultiSurface - 多表面
// ============================================================

class MultiSurface : public AbstractGeometry {
public:
    std::vector<SurfacePtr> polygons;
    
    GeometryType getType() const override { return GeometryType::GT_MultiSurface; }
    bool isEmpty() const override { return polygons.empty(); }
};

using MultiSurfacePtr = std::shared_ptr<MultiSurface>;

// ============================================================
// CompositeSurface - 复合表面
// ============================================================

class CompositeSurface : public Surface {
public:
    GeometryType getType() const override { return GeometryType::GT_CompositeSurface; }
};

using CompositeSurfacePtr = std::shared_ptr<CompositeSurface>;

// ============================================================
// Solid - 体
// ============================================================

class Solid : public AbstractGeometry {
public:
    CompositeSurfacePtr exterior;
    
    GeometryType getType() const override { return GeometryType::GT_Solid; }
    bool isEmpty() const override { return !exterior || exterior->isEmpty(); }
};

using SolidPtr = std::shared_ptr<Solid>;

// ============================================================
// MultiSolid - 多体
// ============================================================

class MultiSolid : public AbstractGeometry {
public:
    std::vector<SolidPtr> solids;
    
    GeometryType getType() const override { return GeometryType::GT_MultiSolid; }
    bool isEmpty() const override { return solids.empty(); }
};

using MultiSolidPtr = std::shared_ptr<MultiSolid>;

// ============================================================
// CompositeSolid - 复合体
// ============================================================

class CompositeSolid : public AbstractGeometry {
public:
    std::vector<SolidPtr> solids;
    
    GeometryType getType() const override { return GeometryType::GT_CompositeSolid; }
    bool isEmpty() const override { return solids.empty(); }
};

using CompositeSolidPtr = std::shared_ptr<CompositeSolid>;

// ============================================================
// ImplicitGeometry - 隐式几何
// ============================================================

class ImplicitGeometry : public AbstractGeometry {
public:
    PointPtr referencePoint;
    AbstractGeometryPtr relativeGeometry;
    TransformationMatrix4x4 transformationMatrix;
    std::string mimeType;
    
    GeometryType getType() const override { return GeometryType::GT_Implicit; }
    bool isEmpty() const override { return !relativeGeometry; }
};

using ImplicitGeometryPtr = std::shared_ptr<ImplicitGeometry>;

} // namespace citygml