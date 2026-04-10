#pragma once

#include "citygml/core/citygml_feature.h"
#include "citygml/core/citygml_base.h"

namespace citygml {

// ============================================================
// 前向声明
// ============================================================

class AbstractGeometry;

// ============================================================
// AbstractGeometry - 抽象几何基类
// ============================================================

class AbstractGeometry : public CityGMLObject {
public:
    virtual ~AbstractGeometry() = default;
    virtual GeometryType getType() const = 0;
    virtual bool isEmpty() const = 0;

    const std::string& getSrsName() const { return srsName_; }
    void setSrsName(const std::string& name) { srsName_ = name; }

    int getSrsDimension() const { return srsDimension_; }
    void setSrsDimension(int dim) { srsDimension_ = dim; }

protected:
    std::string srsName_;
    int srsDimension_ = 3;
};

using AbstractGeometryPtr = std::shared_ptr<AbstractGeometry>;

// ============================================================
// Point - 点
// ============================================================

class Point : public AbstractGeometry {
public:
    Point() = default;
    Point(double x_, double y_, double z_ = 0.0) : x_(x_), y_(y_), z_(z_) {}
    
    double x() const { return x_; }
    double y() const { return y_; }
    double z() const { return z_; }
    
    void setX(double v) { x_ = v; }
    void setY(double v) { y_ = v; }
    void setZ(double v) { z_ = v; }
    
    GeometryType getType() const override { return GeometryType::GT_Point; }
    bool isEmpty() const override { return false; }
    
private:
    double x_ = 0.0, y_ = 0.0, z_ = 0.0;
};

using PointPtr = std::shared_ptr<Point>;

// ============================================================
// MultiPoint - 点集
// ============================================================

class MultiPoint : public AbstractGeometry {
public:
    size_t getPointsCount() const { return points.size(); }
    void addPoint(const Point& p) { points.push_back(p); }
    const std::vector<Point>& getPoints() const { return points; }
    
    GeometryType getType() const override { return GeometryType::GT_MultiPoint; }
    bool isEmpty() const override { return points.empty(); }
    
private:
    std::vector<Point> points;
};

using MultiPointPtr = std::shared_ptr<MultiPoint>;

// ============================================================
// LinearRing - 线性环
// ============================================================

class LinearRing : public AbstractGeometry {
public:
    LinearRing() = default;
    
    size_t getPointsCount() const { return points.size(); }
    void addPoint(const Point& p) { points.push_back(p); }
    void addPoint(double x, double y, double z = 0.0) { points.emplace_back(x, y, z); }
    
    const std::vector<Point>& getPoints() const { return points; }
    
    GeometryType getType() const override { return GeometryType::GT_Curve; }
    bool isEmpty() const override { return points.empty(); }
    
private:
    std::vector<Point> points;
};

using LinearRingPtr = std::shared_ptr<LinearRing>;

// ============================================================
// LineString - 线串
// ============================================================

class LineString : public AbstractGeometry {
public:
    LineString() = default;
    
    size_t getPointsCount() const { return points.size(); }
    void addPoint(const Point& p) { points.push_back(p); }
    void addPoint(double x, double y, double z = 0.0) { points.emplace_back(x, y, z); }
    
    const std::vector<Point>& getPoints() const { return points; }
    
    GeometryType getType() const override { return GeometryType::GT_LineString; }
    bool isEmpty() const override { return points.empty(); }
    
private:
    std::vector<Point> points;
};

using LineStringPtr = std::shared_ptr<LineString>;

// ============================================================
// MultiCurve - 多曲线
// ============================================================

class MultiCurve : public AbstractGeometry {
public:
    size_t getGeometriesCount() const { return curves.size(); }
    void addGeometry(LineStringPtr geom) { curves.push_back(geom); }
    LineStringPtr getGeometry(size_t index) const {
        return index < curves.size() ? curves[index] : nullptr;
    }
    
    GeometryType getType() const override { return GeometryType::GT_MultiCurve; }
    bool isEmpty() const override { return curves.empty(); }
    
private:
    std::vector<LineStringPtr> curves;
};

using MultiCurvePtr = std::shared_ptr<MultiCurve>;

// ============================================================
// Polygon - 多边形
// ============================================================

class Polygon : public AbstractGeometry {
public:
    Polygon() = default;
    
    void setExteriorRing(LinearRingPtr ring) { exteriorRing_ = ring; }
    LinearRingPtr getExteriorRing() const { return exteriorRing_; }
    
    void addInteriorRing(LinearRingPtr ring) { interiorRings_.push_back(ring); }
    size_t getInteriorRingsCount() const { return interiorRings_.size(); }
    LinearRingPtr getInteriorRing(size_t index) const {
        return index < interiorRings_.size() ? interiorRings_[index] : nullptr;
    }
    
    GeometryType getType() const override { return GeometryType::GT_Polygon; }
    bool isEmpty() const override { return !exteriorRing_; }
    
private:
    LinearRingPtr exteriorRing_;
    std::vector<LinearRingPtr> interiorRings_;
};

using PolygonPtr = std::shared_ptr<Polygon>;

// ============================================================
// MultiSurface - 多表面
// ============================================================

class MultiSurface : public AbstractGeometry {
public:
    MultiSurface() = default;
    
    void setType(GeometryType t) { type_ = t; }
    
    size_t getGeometriesCount() const { return polygons.size(); }
    void addGeometry(PolygonPtr poly) { polygons.push_back(poly); }
    PolygonPtr getGeometry(size_t index) const {
        return index < polygons.size() ? polygons[index] : nullptr;
    }
    
    GeometryType getType() const override { return type_; }
    bool isEmpty() const override { return polygons.empty(); }
    
private:
    GeometryType type_ = GeometryType::GT_MultiSurface;
    std::vector<PolygonPtr> polygons;
};

using MultiSurfacePtr = std::shared_ptr<MultiSurface>;

// ============================================================
// Solid - 体
// ============================================================

class Solid : public AbstractGeometry {
public:
    void setOuterShell(MultiSurfacePtr shell) { outerShell_ = shell; }
    MultiSurfacePtr getOuterShell() const { return outerShell_; }
    
    void setType(GeometryType t) { type_ = t; }
    
    GeometryType getType() const override { return type_; }
    bool isEmpty() const override { return !outerShell_; }
    
private:
    GeometryType type_ = GeometryType::GT_Solid;
    MultiSurfacePtr outerShell_;
};

using SolidPtr = std::shared_ptr<Solid>;

// ============================================================
// MultiSolid - 多体
// ============================================================

class MultiSolid : public AbstractGeometry {
public:
    size_t getGeometriesCount() const { return solids.size(); }
    void addGeometry(SolidPtr geom) { solids.push_back(geom); }
    SolidPtr getGeometry(size_t index) const {
        return index < solids.size() ? solids[index] : nullptr;
    }
    
    GeometryType getType() const override { return GeometryType::GT_MultiSolid; }
    bool isEmpty() const override { return solids.empty(); }
    
private:
    std::vector<SolidPtr> solids;
};

using MultiSolidPtr = std::shared_ptr<MultiSolid>;

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