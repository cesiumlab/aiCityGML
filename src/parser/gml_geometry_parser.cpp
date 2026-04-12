#include "gml_geometry_parser.h"
#include "xml_document.h"
#include "parser_context.h"
#include "core/citygml_geometry.h"
#include <sstream>

namespace citygml {

GMLGeometryParser::GMLGeometryParser(std::shared_ptr<ParserContext> context)
    : context_(context) {}

GMLGeometryParser::~GMLGeometryParser() = default;

// 注册 Polygon 节点到上下文，供 xlink 引用解析使用
void GMLGeometryParser::registerPolygonNode(void* node, const std::string& gmlId) {
    if (!gmlId.empty()) {
        context_->registerPolygon(gmlId, node);
    }
}

// 根据 gml:id 查找并解析几何节点（支持 Polygon 和 MultiSurface）
std::shared_ptr<AbstractGeometry> GMLGeometryParser::resolveGeometryById(const std::string& id) {
    if (id.empty()) return nullptr;
    void* node = context_->getPolygonById(id);
    if (!node) return nullptr;
    return parseGeometry(node);
}

std::shared_ptr<AbstractGeometry> GMLGeometryParser::parseGeometry(void* node) {
    if (!node) return nullptr;

    std::string name = XMLDocument::nodeName(node);

    std::shared_ptr<AbstractGeometry> geom;
    if (name == "Point" || name == "gml:Point") {
        geom = parsePoint(node);
    }
    else if (name == "MultiPoint" || name == "gml:MultiPoint") {
        geom = parseMultiPoint(node);
    }
    else if (name == "MultiCurve" || name == "gml:MultiCurve") {
        geom = parseMultiCurve(node);
    }
    else if (name == "MultiSurface" || name == "gml:MultiSurface" ||
             name == "CompositeSurface" || name == "gml:CompositeSurface") {
        geom = parseMultiSurface(node);
    }
    else if (name == "Solid" || name == "gml:Solid" ||
             name == "CompositeSolid" || name == "gml:CompositeSolid") {
        geom = parseSolid(node);
    }
    else if (name == "Polygon" || name == "gml:Polygon") {
        geom = parsePolygon(node);
    }
    else if (name == "ImplicitGeometry" || name == "gml:ImplicitGeometry") {
        geom = parseImplicitGeometry(node);
    }

    if (geom) {
        std::string srs = XMLDocument::attribute(node, "srsName");
        if (!srs.empty()) {
            geom->setSrsName(srs);
            std::string dimStr = XMLDocument::attribute(node, "srsDimension");
            if (!dimStr.empty()) {
                try {
                    geom->setSrsDimension(std::stoi(dimStr));
                } catch (...) {}
            }
        }
    }

    return geom;
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

// 解析 xlink:href 引用，展开 Polygon 到 MultiSurface
void GMLGeometryParser::resolveXLinkReference(void* elemNode, std::shared_ptr<MultiSurface> multiSurface) {
    if (!elemNode || !multiSurface) return;

    // 使用 XMLDocument::attribute 直接查询，避免直接使用 tinyxml2 类型
    std::string href = XMLDocument::attribute(elemNode, "xlink_href");
    if (href.empty()) {
        href = XMLDocument::attribute(elemNode, "xlink:href");
    }
    if (href.empty()) {
        href = XMLDocument::attribute(elemNode, "href");
    }

    if (href.empty()) return;

    // 去掉 # 前缀，查找对应的 Polygon 节点
    if (!href.empty() && href[0] == '#') href.erase(0, 1);
    void* polygonNode = context_->getPolygonById(href);
    if (polygonNode) {
        auto polygon = parsePolygon(polygonNode);
        if (polygon) multiSurface->addGeometry(polygon);
    }
}

std::shared_ptr<MultiSurface> GMLGeometryParser::parseMultiSurface(void* node) {
    if (!node) return nullptr;

    auto multiSurface = std::make_shared<MultiSurface>();
    std::string nodeName = XMLDocument::nodeName(node);

    if (nodeName == "CompositeSurface" || nodeName == "gml:CompositeSurface") {
        multiSurface->setType(GeometryType::GT_CompositeSurface);
    }

    // 统一递归处理所有 surfaceMember（支持 Polygon / MultiSurface / CompositeSurface / Surface / xlink 引用混合）
    std::vector<void*> members = XMLDocument::children(node, "surfaceMember");
    for (void* member : members) {
        // surfaceMember 可能内嵌 Polygon，也可能本身是空元素含 xlink:href
        void* surfaceNode = XMLDocument::firstChildElement(member);

        if (surfaceNode) {
            std::string type = XMLDocument::nodeName(surfaceNode);

            if (type == "Polygon" || type == "gml:Polygon") {
                auto polygon = parsePolygon(surfaceNode);
                if (polygon) multiSurface->addGeometry(polygon);
            }
            else if (type == "MultiSurface" || type == "gml:MultiSurface" ||
                     type == "CompositeSurface" || type == "gml:CompositeSurface") {
                // 递归展开嵌套 MultiSurface/CompositeSurface 中的所有 Polygon
                auto childSurface = parseMultiSurface(surfaceNode);
                if (childSurface) {
                    for (size_t i = 0; i < childSurface->getGeometriesCount(); ++i) {
                        auto poly = childSurface->getGeometry(i);
                        if (poly) multiSurface->addGeometry(poly);
                    }
                }
            }
            else if (type == "Surface" || type == "gml:Surface") {
                // 处理 gml:Surface + gml:patches + gml:PolygonPatch 结构
                auto patches = XMLDocument::child(surfaceNode, "patches");
                if (!patches) patches = XMLDocument::child(surfaceNode, "gml:patches");
                if (patches) {
                    auto polygonPatches = XMLDocument::children(patches, "PolygonPatch");
                    for (void* patch : polygonPatches) {
                        auto polygon = parsePolygon(patch);
                        if (polygon) multiSurface->addGeometry(polygon);
                    }
                    // 尝试 gml:PolygonPatch（某些 GML 可能用全限定名）
                    if (polygonPatches.empty()) {
                        polygonPatches = XMLDocument::children(patches, "gml:PolygonPatch");
                        for (void* patch : polygonPatches) {
                            auto polygon = parsePolygon(patch);
                            if (polygon) multiSurface->addGeometry(polygon);
                        }
                    }
                }
            }
            else if (type == "TriangulatedSurface" || type == "gml:TriangulatedSurface") {
                // 处理 gml:TriangulatedSurface + gml:trianglePatches
                auto patches = XMLDocument::child(surfaceNode, "trianglePatches");
                if (!patches) patches = XMLDocument::child(surfaceNode, "gml:trianglePatches");
                if (patches) {
                    auto triangles = XMLDocument::children(patches, "Triangle");
                    for (void* tri : triangles) {
                        auto polygon = parsePolygon(tri);
                        if (polygon) multiSurface->addGeometry(polygon);
                    }
                }
            } else {
                // 有子元素但不是已知的表面类型：尝试解析 xlink:href 引用
                resolveXLinkReference(surfaceNode, multiSurface);
            }
        } else {
            // surfaceMember 没有元素子节点（可能是空元素含 xlink:href）
            resolveXLinkReference(member, multiSurface);
        }
    }

    // 处理 surfaceMembers 容器（某些 GML 使用此结构）
    void* surfaceMembers = XMLDocument::child(node, "surfaceMembers");
    if (!surfaceMembers) surfaceMembers = XMLDocument::child(node, "gml:surfaceMembers");
    if (surfaceMembers) {
        auto polygons = XMLDocument::children(surfaceMembers, "Polygon");
        for (void* polygonNode : polygons) {
            auto polygon = parsePolygon(polygonNode);
            if (polygon) multiSurface->addGeometry(polygon);
        }
        // surfaceMembers 中也可能包含 gml:Surface
        auto surfaces = XMLDocument::children(surfaceMembers, "Surface");
        for (void* surfaceNode : surfaces) {
            auto patches = XMLDocument::child(surfaceNode, "patches");
            if (patches) {
                auto polygonPatches = XMLDocument::children(patches, "PolygonPatch");
                for (void* patch : polygonPatches) {
                    auto polygon = parsePolygon(patch);
                    if (polygon) multiSurface->addGeometry(polygon);
                }
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

            if (type == "Shell" || type == "gml:Shell") {
                // gml:Shell 容器包含多个 surfaceMember
                auto members = XMLDocument::children(surfaceNode, "surfaceMember");
                auto multiSurface = std::make_shared<MultiSurface>();
                for (void* member : members) {
                    void* geomNode = XMLDocument::firstChildElement(member);
                    if (!geomNode) {
                        // 可能是 xlink:href
                        resolveXLinkReference(member, multiSurface);
                        continue;
                    }
                    std::string geomType = XMLDocument::nodeName(geomNode);
                    if (geomType == "Polygon" || geomType == "gml:Polygon") {
                        auto polygon = parsePolygon(geomNode);
                        if (polygon) multiSurface->addGeometry(polygon);
                    }
                    else if (geomType == "Surface" || geomType == "gml:Surface") {
                        auto patches = XMLDocument::child(geomNode, "patches");
                        if (!patches) patches = XMLDocument::child(geomNode, "gml:patches");
                        if (patches) {
                            auto polygonPatches = XMLDocument::children(patches, "PolygonPatch");
                            for (void* patch : polygonPatches) {
                                auto polygon = parsePolygon(patch);
                                if (polygon) multiSurface->addGeometry(polygon);
                            }
                        }
                    }
                    else if (geomType == "MultiSurface" || geomType == "gml:MultiSurface" ||
                             geomType == "CompositeSurface" || geomType == "gml:CompositeSurface") {
                        auto childMS = parseMultiSurface(geomNode);
                        if (childMS) {
                            for (size_t i = 0; i < childMS->getGeometriesCount(); ++i) {
                                auto poly = childMS->getGeometry(i);
                                if (poly) multiSurface->addGeometry(poly);
                            }
                        }
                    }
                    else {
                        resolveXLinkReference(geomNode, multiSurface);
                    }
                }
                if (!multiSurface->isEmpty()) solid->setOuterShell(multiSurface);
            }
            else if (type == "CompositeSurface" || type == "gml:CompositeSurface") {
                auto composite = parseMultiSurface(surfaceNode);
                if (composite) solid->setOuterShell(composite);
            }
            else if (type == "MultiSurface" || type == "gml:MultiSurface") {
                auto multiSurface = parseMultiSurface(surfaceNode);
                if (multiSurface) solid->setOuterShell(multiSurface);
            }
            else if (type == "Surface" || type == "gml:Surface") {
                // 处理 gml:Surface + gml:patches + gml:PolygonPatch 结构
                auto multiSurface = std::make_shared<MultiSurface>();
                auto patches = XMLDocument::child(surfaceNode, "patches");
                if (!patches) patches = XMLDocument::child(surfaceNode, "gml:patches");
                if (patches) {
                    auto polygonPatches = XMLDocument::children(patches, "PolygonPatch");
                    for (void* patch : polygonPatches) {
                        auto polygon = parsePolygon(patch);
                        if (polygon) multiSurface->addGeometry(polygon);
                    }
                    if (polygonPatches.empty()) {
                        polygonPatches = XMLDocument::children(patches, "gml:PolygonPatch");
                        for (void* patch : polygonPatches) {
                            auto polygon = parsePolygon(patch);
                            if (polygon) multiSurface->addGeometry(polygon);
                        }
                    }
                }
                if (!multiSurface->isEmpty()) solid->setOuterShell(multiSurface);
            }
            else if (type == "TriangulatedSurface" || type == "gml:TriangulatedSurface") {
                // 处理 gml:TriangulatedSurface + gml:trianglePatches
                auto multiSurface = std::make_shared<MultiSurface>();
                auto patches = XMLDocument::child(surfaceNode, "trianglePatches");
                if (!patches) patches = XMLDocument::child(surfaceNode, "gml:trianglePatches");
                if (patches) {
                    auto triangles = XMLDocument::children(patches, "Triangle");
                    for (void* tri : triangles) {
                        auto polygon = parsePolygon(tri);
                        if (polygon) multiSurface->addGeometry(polygon);
                    }
                }
                if (!multiSurface->isEmpty()) solid->setOuterShell(multiSurface);
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

    // 支持每个坐标一个 gml:pos 元素的格式（FZK-Haus 等 CityGML 文件常用）
    if (ring->getPointsCount() == 0) {
        std::vector<void*> posNodes = XMLDocument::children(node, "pos");
        if (posNodes.empty()) posNodes = XMLDocument::children(node, "gml:pos");
        for (void* posNode : posNodes) {
            std::string posStr = XMLDocument::text(posNode);
            if (!posStr.empty()) {
                auto coords = parsePos(posStr);
                if (coords.size() >= 2) {
                    double z = (coords.size() >= 3) ? coords[2] : 0.0;
                    ring->addPoint(coords[0], coords[1], z);
                }
            }
        }
    }

    return ring;
}

std::shared_ptr<ImplicitGeometry> GMLGeometryParser::parseImplicitGeometry(void* node) {
    if (!node) return nullptr;

    auto impl = std::make_shared<ImplicitGeometry>();

    // Parse gml:id
    std::string gmlId = XMLDocument::attribute(node, "gml:id");
    if (!gmlId.empty()) {
        impl->setId(gmlId);
    }

    // Parse mimeType
    void* mimeNode = XMLDocument::child(node, "mimeType");
    if (!mimeNode) mimeNode = XMLDocument::child(node, "app:mimeType");
    if (mimeNode) {
        impl->mimeType = XMLDocument::text(mimeNode);
    }

    // Parse transformationMatrix (16 doubles in row-major order)
    void* matrixNode = XMLDocument::child(node, "transformationMatrix");
    if (matrixNode) {
        std::string matrixStr = XMLDocument::text(matrixNode);
        std::istringstream iss(matrixStr);
        TransformationMatrix4x4 mat{};
        for (size_t i = 0; i < 16 && iss >> mat[i]; ++i) {}
        impl->transformationMatrix = mat;
    }

    // Parse referencePoint
    void* refPointNode = XMLDocument::child(node, "referencePoint");
    if (!refPointNode) refPointNode = XMLDocument::child(node, "gml:referencePoint");
    if (refPointNode) {
        void* pointNode = XMLDocument::firstChildElement(refPointNode);
        if (pointNode) {
            impl->referencePoint = parsePoint(pointNode);
        }
    }

    // Parse relativeGeometry (recursive)
    void* relGeomNode = XMLDocument::child(node, "relativeGeometry");
    if (!relGeomNode) relGeomNode = XMLDocument::child(node, "gml:relativeGeometry");
    if (relGeomNode) {
        void* geomNode = XMLDocument::firstChildElement(relGeomNode);
        if (geomNode) {
            impl->relativeGeometry = parseGeometry(geomNode);
        }
    }

    return impl;
}

} // namespace citygml