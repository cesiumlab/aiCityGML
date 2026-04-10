#include "citygml/parser/gml_geometry_parser.h"
#include "citygml/parser/xml_document.h"
#include "citygml/parser/parser_context.h"
#include "citygml/core/citygml_geometry.h"
#include <sstream>

namespace citygml {

GMLGeometryParser::GMLGeometryParser(std::shared_ptr<ParserContext> context)
    : context_(context) {}

GMLGeometryParser::~GMLGeometryParser() = default;

std::shared_ptr<AbstractGeometry> GMLGeometryParser::parseGeometry(void* node) {
    if (!node) return nullptr;
    
    std::string name = XMLDocument::nodeName(node);
    
    if (name == "Point" || name == "gml:Point") {
        return parsePoint(node);
    }
    else if (name == "MultiPoint" || name == "gml:MultiPoint") {
        return parseMultiPoint(node);
    }
    else if (name == "MultiCurve" || name == "gml:MultiCurve") {
        return parseMultiCurve(node);
    }
    else if (name == "MultiSurface" || name == "gml:MultiSurface" ||
             name == "CompositeSurface" || name == "gml:CompositeSurface") {
        return parseMultiSurface(node);
    }
    else if (name == "Solid" || name == "gml:Solid" ||
             name == "CompositeSolid" || name == "gml:CompositeSolid") {
        return parseSolid(node);
    }
    else if (name == "Polygon" || name == "gml:Polygon") {
        return parsePolygon(node);
    }
    
    return nullptr;
}

std::shared_ptr<Point> GMLGeometryParser::parsePoint(void* node) {
    if (!node) return nullptr;
    
    std::string posStr = XMLDocument::text(node);
    auto coords = parsePos(posStr);
    
    if (coords.size() >= 2) {
        double z = (coords.size() >= 3) ? coords[2] : 0.0;
        return std::make_shared<Point>(coords[0], coords[1], z);
    }
    
    return nullptr;
}

std::shared_ptr<MultiPoint> GMLGeometryParser::parseMultiPoint(void* node) {
    if (!node) return nullptr;
    
    auto multiPoint = std::make_shared<MultiPoint>();
    
    auto members = XMLDocument::children(node, "pointMember");
    for (void* member : members) {
        void* pointNode = XMLDocument::child(member, "Point");
        if (pointNode) {
            auto point = parsePoint(pointNode);
            if (point) {
                multiPoint->addPoint(*point);
            }
        }
    }
    
    void* pointMembers = XMLDocument::child(node, "pointMembers");
    if (pointMembers) {
        auto points = XMLDocument::children(pointMembers, "Point");
        for (void* pointNode : points) {
            auto point = parsePoint(pointNode);
            if (point) {
                multiPoint->addPoint(*point);
            }
        }
    }
    
    return multiPoint;
}

std::shared_ptr<MultiCurve> GMLGeometryParser::parseMultiCurve(void* node) {
    if (!node) return nullptr;
    
    auto multiCurve = std::make_shared<MultiCurve>();
    
    auto members = XMLDocument::children(node, "curveMember");
    for (void* member : members) {
        void* curveNode = XMLDocument::firstChildElement(member);
        if (curveNode) {
            std::string curveType = XMLDocument::nodeName(curveNode);
            if (curveType == "LineString" || curveType == "Curve") {
                void* posListNode = XMLDocument::child(curveNode, "posList");
                if (!posListNode) {
                    posListNode = XMLDocument::child(curveNode, "coordinates");
                }
                if (posListNode) {
                    std::string posStr = XMLDocument::text(posListNode);
                    auto coords = parsePosList(posStr, 3);
                    if (coords.size() >= 2) {
                        auto lineString = std::make_shared<LineString>();
                        for (const auto& coord : coords) {
                            lineString->addPoint(coord[0], coord[1], coord.size() > 2 ? coord[2] : 0.0);
                        }
                        multiCurve->addGeometry(lineString);
                    }
                }
            }
        }
    }
    
    return multiCurve;
}

std::shared_ptr<MultiSurface> GMLGeometryParser::parseMultiSurface(void* node) {
    if (!node) return nullptr;
    
    auto multiSurface = std::make_shared<MultiSurface>();
    std::string nodeName = XMLDocument::nodeName(node);
    
    if (nodeName == "CompositeSurface" || nodeName == "gml:CompositeSurface") {
        multiSurface->setType(GeometryType::GT_CompositeSurface);
    }
    
    auto members = XMLDocument::children(node, "surfaceMember");
    for (void* member : members) {
        void* surfaceNode = XMLDocument::firstChildElement(member);
        if (surfaceNode) {
            std::string type = XMLDocument::nodeName(surfaceNode);
            if (type == "Polygon" || type == "gml:Polygon") {
                auto polygon = parsePolygon(surfaceNode);
                if (polygon) {
                    multiSurface->addGeometry(polygon);
                }
            }
            else if (type == "MultiSurface" || type == "gml:MultiSurface" ||
                     type == "CompositeSurface" || type == "gml:CompositeSurface") {
                auto childSurface = parseMultiSurface(surfaceNode);
                if (childSurface) {
                    for (size_t i = 0; i < childSurface->getGeometriesCount(); ++i) {
                        auto poly = childSurface->getGeometry(i);
                        if (poly) {
                            multiSurface->addGeometry(poly);
                        }
                    }
                }
            }
        }
    }
    
    void* surfaceMembers = XMLDocument::child(node, "surfaceMembers");
    if (!surfaceMembers) surfaceMembers = XMLDocument::child(node, "gml:surfaceMembers");
    if (surfaceMembers) {
        auto polygons = XMLDocument::children(surfaceMembers, "Polygon");
        for (void* polygonNode : polygons) {
            auto polygon = parsePolygon(polygonNode);
            if (polygon) {
                multiSurface->addGeometry(polygon);
            }
        }
    }
    
    return multiSurface;
}

std::shared_ptr<Solid> GMLGeometryParser::parseSolid(void* node) {
    if (!node) return nullptr;
    
    auto solid = std::make_shared<Solid>();
    std::string nodeName = XMLDocument::nodeName(node);
    
    void* exterior = XMLDocument::child(node, "exterior");
    if (!exterior) exterior = XMLDocument::child(node, "gml:exterior");
    if (exterior) {
        void* surfaceNode = XMLDocument::firstChildElement(exterior);
        if (surfaceNode) {
            std::string type = XMLDocument::nodeName(surfaceNode);
            if (type == "CompositeSurface" || type == "gml:CompositeSurface") {
                auto composite = parseMultiSurface(surfaceNode);
                if (composite) solid->setOuterShell(composite);
            }
            else if (type == "MultiSurface" || type == "gml:MultiSurface") {
                auto multiSurface = parseMultiSurface(surfaceNode);
                if (multiSurface) solid->setOuterShell(multiSurface);
            }
        }
    }
    
    if (nodeName == "CompositeSolid" || nodeName == "gml:CompositeSolid") {
        solid->setType(GeometryType::GT_CompositeSolid);
    }
    
    return solid;
}

std::shared_ptr<Polygon> GMLGeometryParser::parsePolygon(void* node) {
    if (!node) return nullptr;
    
    auto polygon = std::make_shared<Polygon>();
    
    std::string id = XMLDocument::attribute(node, "gml:id");
    polygon->setId(id);
    
    void* exterior = XMLDocument::child(node, "exterior");
    if (!exterior) exterior = XMLDocument::child(node, "gml:exterior");
    if (exterior) {
        void* ringNode = XMLDocument::firstChildElement(exterior);
        if (ringNode) {
            std::string ringName = XMLDocument::nodeName(ringNode);
            if (ringName == "LinearRing" || ringName == "gml:LinearRing") {
                auto ring = parseLinearRing(ringNode);
                if (ring) polygon->setExteriorRing(ring);
            }
        }
    }
    
    auto interiors = XMLDocument::children(node, "interior");
    if (interiors.empty()) interiors = XMLDocument::children(node, "gml:interior");
    for (void* interior : interiors) {
        void* ringNode = XMLDocument::firstChildElement(interior);
        if (ringNode) {
            std::string ringName = XMLDocument::nodeName(ringNode);
            if (ringName == "LinearRing" || ringName == "gml:LinearRing") {
                auto ring = parseLinearRing(ringNode);
                if (ring) polygon->addInteriorRing(ring);
            }
        }
    }
    
    return polygon;
}

std::vector<double> GMLGeometryParser::parsePos(const std::string& posStr) {
    std::vector<double> coords;
    std::istringstream iss(posStr);
    double val;
    while (iss >> val) {
        coords.push_back(val);
    }
    return coords;
}

std::vector<std::vector<double>> GMLGeometryParser::parsePosList(const std::string& posListStr, int srsDimension) {
    std::vector<std::vector<double>> points;
    
    if (posListStr.empty()) return points;
    
    std::istringstream iss(posListStr);
    std::vector<double> coords;
    double val;
    while (iss >> val) {
        coords.push_back(val);
    }
    
    for (size_t i = 0; i + static_cast<size_t>(srsDimension - 1) < coords.size(); i += srsDimension) {
        std::vector<double> point;
        for (int j = 0; j < srsDimension; ++j) {
            point.push_back(coords[i + j]);
        }
        points.push_back(point);
    }
    
    return points;
}

std::shared_ptr<LinearRing> GMLGeometryParser::parseLinearRing(void* node) {
    if (!node) return nullptr;
    
    auto ring = std::make_shared<LinearRing>();
    
    std::string id = XMLDocument::attribute(node, "gml:id");
    ring->setId(id);
    
    void* posListNode = XMLDocument::child(node, "posList");
    if (!posListNode) {
        posListNode = XMLDocument::child(node, "gml:posList");
    }
    if (posListNode) {
        std::string posStr = XMLDocument::text(posListNode);
        std::string dimStr = XMLDocument::attribute(posListNode, "srsDimension");
        int dim = dimStr.empty() ? 3 : std::stoi(dimStr);
        auto points = parsePosList(posStr, dim);
        for (const auto& p : points) {
            ring->addPoint(p[0], p[1], p.size() > 2 ? p[2] : 0.0);
        }
    }
    
    if (ring->getPointsCount() == 0) {
        void* coordsNode = XMLDocument::child(node, "coordinates");
        if (coordsNode) {
            std::string coordsStr = XMLDocument::text(coordsNode);
            std::istringstream iss(coordsStr);
            double x, y, z = 0.0;
            while (iss >> x >> y >> z) {
                ring->addPoint(x, y, z);
            }
        }
    }
    
    return ring;
}

} // namespace citygml