#include "gml_geometry_parser.h"
#include "xml_document.h"
#include "parser_context.h"
#include "core/citygml_geometry.h"
#include <sstream>
#include <algorithm>

namespace citygml {

// 反转 Polygon 的绕行方向（用于处理 OrientableSurface orientation="-"）
// 反转所有环的点顺序：exteriorRing + 所有 interiorRings
static std::shared_ptr<Polygon> flipPolygonWinding(std::shared_ptr<Polygon> polygon) {
    if (!polygon) return nullptr;

    auto flipped = std::make_shared<Polygon>();
    flipped->setId(polygon->getId());

    if (auto ext = polygon->getExteriorRing()) {
        auto flippedRing = std::make_shared<LinearRing>();
        flippedRing->setId(ext->getId());
        const auto& pts = ext->getPoints();
        for (auto it = pts.rbegin(); it != pts.rend(); ++it) {
            flippedRing->addPoint(*it);
        }
        flipped->setExteriorRing(flippedRing);
    }

    for (size_t i = 0; i < polygon->getInteriorRingsCount(); ++i) {
        if (auto ring = polygon->getInteriorRing(i)) {
            auto flippedRing = std::make_shared<LinearRing>();
            flippedRing->setId(ring->getId());
            const auto& pts = ring->getPoints();
            for (auto it = pts.rbegin(); it != pts.rend(); ++it) {
                flippedRing->addPoint(*it);
            }
            flipped->addInteriorRing(flippedRing);
        }
    }

    if (auto mat = polygon->getMaterial()) {
        flipped->setAppearance(mat);
    }
    if (auto tex = polygon->getTexture()) {
        flipped->setAppearance(tex);
    }

    return flipped;
}

GMLGeometryParser::GMLGeometryParser(std::shared_ptr<ParserContext> context)
    : context_(context) {}

GMLGeometryParser::~GMLGeometryParser() = default;

// 注册 Polygon 节点到上下文，供 xlink 引用解析使用
void GMLGeometryParser::registerPolygonNode(void* node, const std::string& gmlId) {
    if (!gmlId.empty()) {
        context_->registerPolygon(gmlId, node);
    }
}

// 根据 gml:id 查找并返回几何对象
// 优先返回缓存中已解析的对象，避免同一 Polygon 被重复解析
std::shared_ptr<AbstractGeometry> GMLGeometryParser::resolveGeometryById(const std::string& id) {
    if (id.empty()) return nullptr;

    // 1. 先检查缓存中是否已有解析后的 Polygon
    if (auto cachedPolygon = context_->getParsedPolygon(id)) {
        return cachedPolygon;
    }

    // 2. 检查缓存中是否已有解析后的 MultiSurface
    if (auto cachedMS = context_->getParsedMultiSurface(id)) {
        return cachedMS;
    }

    // 3. 缓存都没有，通过 XML 节点解析
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

// 解析 xlink:href 引用，展开几何到 MultiSurface
void GMLGeometryParser::resolveXLinkReference(void* elemNode, std::shared_ptr<MultiSurface> multiSurface) {
    if (!elemNode || !multiSurface) return;

    std::string href = XMLDocument::attribute(elemNode, "xlink_href");
    if (href.empty()) {
        href = XMLDocument::attribute(elemNode, "xlink:href");
    }
    if (href.empty()) {
        href = XMLDocument::attribute(elemNode, "href");
    }

    if (href.empty()) return;

    if (!href.empty() && href[0] == '#') href.erase(0, 1);
    std::shared_ptr<AbstractGeometry> geom = resolveGeometryById(href);
    if (!geom) {
        return;
    }

    // Polygon: 直接添加
    if (auto polygon = std::dynamic_pointer_cast<Polygon>(geom)) {
        multiSurface->addGeometry(polygon);
        return;
    }
    // MultiSurface / CompositeSurface / TriangulatedSurface: 展开所有 Polygon
    if (auto ms = std::dynamic_pointer_cast<MultiSurface>(geom)) {
        for (size_t i = 0; i < ms->getGeometriesCount(); ++i) {
            if (auto poly = ms->getGeometry(i)) {
                multiSurface->addGeometry(poly);
            }
        }
        return;
    }
    // Solid: 展开其 outer shell 中的所有 Polygon
    if (auto solid = std::dynamic_pointer_cast<Solid>(geom)) {
        if (auto shell = solid->getOuterShell()) {
            for (size_t i = 0; i < shell->getGeometriesCount(); ++i) {
                if (auto poly = shell->getGeometry(i)) {
                    multiSurface->addGeometry(poly);
                }
            }
        }
    }
}

std::shared_ptr<MultiSurface> GMLGeometryParser::parseMultiSurface(void* node) {
    if (!node) return nullptr;

    // 检查是否有 gml:id，如果有则尝试从缓存获取
    std::string nodeId = XMLDocument::attribute(node, "gml:id");
    if (!nodeId.empty()) {
        if (auto cached = context_->getParsedMultiSurface(nodeId)) {
            return cached;
        }
    }

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
                if (polygon) {
                    multiSurface->addGeometry(polygon);
                }
            }
            else if (type == "MultiSurface" || type == "gml:MultiSurface" ||
                     type == "CompositeSurface" || type == "gml:CompositeSurface") {
                // 递归展开嵌套 MultiSurface/CompositeSurface 中的所有 Polygon
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
            }
            else if (type == "OrientableSurface" || type == "gml:OrientableSurface") {
                // gml:OrientableSurface 包装一个 baseSurface，通过 xlink:href 引用 Polygon
                // orientation="-" 表示反转法线方向
                std::string orientation = XMLDocument::attribute(surfaceNode, "orientation");
                bool flipWinding = (orientation == "-");

                // 查找 baseSurface 节点
                void* baseSurface = XMLDocument::child(surfaceNode, "baseSurface");
                if (!baseSurface) baseSurface = XMLDocument::child(surfaceNode, "gml:baseSurface");

                if (baseSurface) {
                    // 尝试解析 baseSurface（可能内嵌 Polygon，也可能是 xlink:href 引用）
                    void* geomNode = XMLDocument::firstChildElement(baseSurface);
                    std::shared_ptr<AbstractGeometry> baseGeom;

                    if (geomNode) {
                        std::string geomType = XMLDocument::nodeName(geomNode);
                        if (geomType == "Polygon" || geomType == "gml:Polygon") {
                            baseGeom = parsePolygon(geomNode);
                        } else {
                            // baseSurface 内嵌了其他几何类型（如另一个 MultiSurface）
                            baseGeom = parseGeometry(geomNode);
                        }
                    } else {
                        // baseSurface 是空元素，只有 xlink:href
                        std::string href = XMLDocument::attribute(baseSurface, "xlink_href");
                        if (href.empty()) href = XMLDocument::attribute(baseSurface, "xlink:href");
                        if (href.empty()) href = XMLDocument::attribute(baseSurface, "href");
                        if (!href.empty()) {
                            if (href[0] == '#') href.erase(0, 1);
                            baseGeom = resolveGeometryById(href);
                        }
                    }

                    // 展开 baseGeom 中的所有 Polygon
                    if (auto poly = std::dynamic_pointer_cast<Polygon>(baseGeom)) {
                        if (flipWinding) poly = flipPolygonWinding(poly);
                        if (poly) multiSurface->addGeometry(poly);
                    } else if (auto ms = std::dynamic_pointer_cast<MultiSurface>(baseGeom)) {
                        for (size_t i = 0; i < ms->getGeometriesCount(); ++i) {
                            auto p = ms->getGeometry(i);
                            if (p) {
                                if (flipWinding) p = flipPolygonWinding(std::dynamic_pointer_cast<Polygon>(p));
                                if (p) multiSurface->addGeometry(p);
                            }
                        }
                    } else if (auto solid = std::dynamic_pointer_cast<Solid>(baseGeom)) {
                        // Solid: 展开其 outer shell
                        if (auto shell = solid->getOuterShell()) {
                            for (size_t i = 0; i < shell->getGeometriesCount(); ++i) {
                                auto p = shell->getGeometry(i);
                                if (p) {
                                    if (flipWinding) p = flipPolygonWinding(std::dynamic_pointer_cast<Polygon>(p));
                                    if (p) multiSurface->addGeometry(p);
                                }
                            }
                        }
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

    // 注册到缓存
    if (!nodeId.empty()) {
        context_->registerParsedGeometry(nodeId, multiSurface);
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

    std::string id = XMLDocument::attribute(node, "gml:id");

    // 如果已经缓存过，直接返回缓存的对象（避免同一 Polygon 被重复解析）
    if (!id.empty()) {
        if (auto cached = context_->getParsedPolygon(id)) {
            return cached;
        }
    }

    auto polygon = std::make_shared<Polygon>();
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

    // 注册到缓存，避免后续 xlink 引用时重复解析
    if (!id.empty()) {
        context_->registerParsedGeometry(id, polygon);
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
