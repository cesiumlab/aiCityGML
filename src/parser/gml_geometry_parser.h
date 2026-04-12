#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstddef>

namespace citygml {

class ParserContext;
struct Envelope;
class Point;
class AbstractGeometry;
class Polygon;
class MultiSurface;
class MultiPoint;
class MultiCurve;
class Solid;
class LinearRing;
class ImplicitGeometry;

class GMLGeometryParser {
public:
    GMLGeometryParser(std::shared_ptr<ParserContext> context);
    ~GMLGeometryParser();
    
    std::shared_ptr<AbstractGeometry> parseGeometry(void* node);
    std::shared_ptr<Point> parsePoint(void* node);
    std::shared_ptr<MultiPoint> parseMultiPoint(void* node);
    std::shared_ptr<MultiCurve> parseMultiCurve(void* node);
    std::shared_ptr<MultiSurface> parseMultiSurface(void* node);
    std::shared_ptr<Solid> parseSolid(void* node);
    std::shared_ptr<Polygon> parsePolygon(void* node);
    
    std::vector<double> parsePos(const std::string& posStr);
    std::vector<std::vector<double>> parsePosList(const std::string& posListStr, int srsDimension = 3);
    std::shared_ptr<LinearRing> parseLinearRing(void* node);

    // 解析 xlink:href 引用，展开 Polygon 到 MultiSurface
    void resolveXLinkReference(void* elemNode, std::shared_ptr<MultiSurface> multiSurface);

    // 注册 Polygon 节点到上下文，支持 xlink:href 引用解析
    void registerPolygonNode(void* node, const std::string& gmlId);

    // 根据 gml:id 查找并解析几何节点（支持 Polygon 和 MultiSurface）
    std::shared_ptr<AbstractGeometry> resolveGeometryById(const std::string& id);

private:
    std::shared_ptr<ImplicitGeometry> parseImplicitGeometry(void* node);
    
    std::shared_ptr<ParserContext> context_;
};

} // namespace citygml