#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstddef>

namespace citygml {

class ParserContext;
class Envelope;
class Point;
class AbstractGeometry;
class Polygon;
class MultiSurface;
class MultiPoint;
class MultiCurve;
class Solid;
class LinearRing;

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
    
    std::vector<double> parsePos(const std::string& posStr);
    std::vector<std::vector<double>> parsePosList(const std::string& posListStr, int srsDimension = 3);
    std::shared_ptr<LinearRing> parseLinearRing(void* node);
    
private:
    std::shared_ptr<Polygon> parsePolygon(void* node);
    
    std::shared_ptr<ParserContext> context_;
};

} // namespace citygml