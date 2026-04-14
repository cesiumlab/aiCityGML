#include "citygml_reader.h"
#include "xml_document.h"
#include "gml_envelope_parser.h"
#include "gml_geometry_parser.h"
#include "parser_context.h"
#include "core/citygml_object.h"
#include "core/citygml_base.h"
#include "core/citygml_feature.h"
#include "core/citygml_surface.h"
#include "core/citygml_opening.h"
#include "building/citygml_building.h"
#include <sstream>
#include <iostream>

namespace citygml {

CityGMLReader::CityGMLReader(std::shared_ptr<ParserContext> context, std::shared_ptr<GMLGeometryParser> geometryParser)
    : context_(context), geometryParser_(geometryParser)
{
    namespaces_["core"] = "http://www.opengis.net/citygml/2.0";
    namespaces_["bldg"] = "http://www.opengis.net/citygml/building/2.0";
    namespaces_["gml"] = "http://www.opengis.net/gml";
    namespaces_["app"] = "http://www.opengis.net/citygml/appearance/2.0";
    namespaces_["gen"] = "http://www.opengis.net/citygml/generics/2.0";
}

CityGMLReader::~CityGMLReader() = default;

std::shared_ptr<CityModel> CityGMLReader::readCityModel(void* node) {
    if (!node) return nullptr;
    
    auto cityModel = std::make_shared<CityModel>();
    
    // 尝试多种可能的 boundedBy 名称
    void* boundedBy = XMLDocument::child(node, "boundedBy");
    if (!boundedBy) boundedBy = XMLDocument::child(node, "gml:boundedBy");
    if (boundedBy) {
        GMLEnvelopeParser envelopeParser;
        auto envelope = envelopeParser.parse(boundedBy, context_);
        if (envelope) {
            cityModel->setEnvelope(*envelope);
        }
    }
    
    // 解析 cityObjectMember
    std::vector<void*> members = XMLDocument::children(node, "cityObjectMember");
    if (members.empty()) {
        members = XMLDocument::children(node, "core:cityObjectMember");
    }
    
    for (void* member : members) {
        // 获取 cityObjectMember 的第一个子元素（Building 等）
        void* cityObjectNode = XMLDocument::firstChildElement(member);
        if (cityObjectNode) {
            auto cityObject = readCityObject(cityObjectNode);
            if (cityObject) {
                cityModel->addCityObject(cityObject);
            }
        }
    }
    
    return cityModel;
}

// 扫描节点中的所有 Polygon 元素并注册到上下文，支持 xlink 引用解析
// 采用迭代方式遍历，避免递归深度过大
static void scanAndRegisterPolygons(void* node, GMLGeometryParser& parser) {
    if (!node) return;

    // 使用显式栈实现迭代遍历，避免递归栈溢出
    std::vector<void*> stack;
    stack.push_back(node);

    while (!stack.empty()) {
        void* current = stack.back();
        stack.pop_back();

        std::string nodeName = XMLDocument::nodeName(current);

        // 注册 Polygon
        if (nodeName == "Polygon" || nodeName == "gml:Polygon") {
            std::string gmlId = XMLDocument::attribute(current, "gml:id");
            if (gmlId.empty()) gmlId = XMLDocument::attribute(current, "id");
            if (!gmlId.empty()) {
                parser.registerPolygonNode(current, gmlId);
            }
        }

        // 注册 MultiSurface（支持 xlink:href 引用 MultiSurface）
        if (nodeName == "MultiSurface" || nodeName == "gml:MultiSurface" ||
            nodeName == "CompositeSurface" || nodeName == "gml:CompositeSurface") {
            std::string gmlId = XMLDocument::attribute(current, "gml:id");
            if (gmlId.empty()) gmlId = XMLDocument::attribute(current, "id");
            if (!gmlId.empty()) {
                parser.registerPolygonNode(current, gmlId);
            }
        }

        // 将所有子元素压入栈中
        void* child = XMLDocument::firstChildElement(current);
        while (child) {
            stack.push_back(child);
            child = XMLDocument::nextSiblingElement(child);
        }
    }
}

std::shared_ptr<CityObject> CityGMLReader::readCityObject(void* node) {
    if (!node) return nullptr;

    std::string nodeName = getLocalName(node);

    // 根据类型创建对象
    if (nodeName == "Building") {
        scanAndRegisterPolygons(node, *geometryParser_);
        return readBuilding(node);
    }

    auto cityObject = std::make_shared<CityObject>(nodeName);

    parseNameAndDescription(node, cityObject);
    parseBoundedBy(node, cityObject);
    parseLODGeometries(node, cityObject);
    parseImplicitRepresentation(node, cityObject);

    return cityObject;
}

std::shared_ptr<CityObject> CityGMLReader::readBuilding(void* node) {
    auto building = std::make_shared<CityObject>("Building");

    // 解析 ID (gml:id)
    std::string gmlId = XMLDocument::attribute(node, "gml:id");
    if (!gmlId.empty()) {
        building->setId(gmlId);
    }

    parseNameAndDescription(node, building);
    // 解析 bldg:interiorRoom
    parseInteriorRooms(node, building);
    // 解析 bldg:address
    parseAddresses(node, building);
    parseCreationDate(node, building);
    parseRelativeToTerrain(node, building);
    parseGenericAttributes(node, building);
    parseBuildingAttributes(node, building);
    parseLODGeometries(node, building);
    parseImplicitRepresentation(node, building);
    parseBoundedBy(node, building);

    return building;
}

void CityGMLReader::parseInteriorRooms(void* node, std::shared_ptr<CityObject> obj) {
    // 查找 bldg:interiorRoom, bldg:buildingRoom 或对应的无命名空间版本
    void* interiorRoom = XMLDocument::child(node, "bldg:interiorRoom");
    if (!interiorRoom) interiorRoom = XMLDocument::child(node, "interiorRoom");
    if (!interiorRoom) interiorRoom = XMLDocument::child(node, "bldg:buildingRoom");
    if (!interiorRoom) interiorRoom = XMLDocument::child(node, "buildingRoom");

    while (interiorRoom) {
        // interiorRoom 的第一个子元素是 Room 或 BuildingRoom
        void* roomNode = XMLDocument::firstChildElement(interiorRoom);
        if (roomNode) {
            std::string roomType = getLocalName(roomNode);
            if (roomType == "Room" || roomType == "BuildingRoom") {
                auto room = std::make_shared<CityObject>("BuildingRoom");

                // 解析 gml:id
                std::string gmlId = XMLDocument::attribute(roomNode, "gml:id");
                if (!gmlId.empty()) {
                    room->setId(gmlId);
                }

                // 解析 gml:name
                void* nameNode = XMLDocument::child(roomNode, "gml:name");
                if (nameNode) {
                    room->setName(XMLDocument::text(nameNode));
                }

                // 解析 gml:description
                void* descNode = XMLDocument::child(roomNode, "gml:description");
                if (descNode) {
                    room->setDescription(XMLDocument::text(descNode));
                }

                // 解析 LOD 几何体
                parseLODGeometries(roomNode, room);

                // 解析 bldg:interiorFurniture (家具)
                parseInteriorFurniture(roomNode, room);

                obj->addChildCityObject(room);
            }
        }

        // 查找下一个 interiorRoom 兄弟元素（使用通用方式，不依赖命名空间前缀）
        void* next = XMLDocument::nextSiblingElement(interiorRoom, "interiorRoom");
        if (!next) next = XMLDocument::nextSiblingElement(interiorRoom, "bldg:interiorRoom");
        if (!next) next = XMLDocument::nextSiblingElement(interiorRoom, "buildingRoom");
        if (!next) next = XMLDocument::nextSiblingElement(interiorRoom, "bldg:buildingRoom");
        interiorRoom = next;
    }
}

void CityGMLReader::parseInteriorFurniture(void* roomNode, std::shared_ptr<CityObject> room) {
    // 查找 bldg:interiorFurniture 或 interiorFurniture
    void* interiorFurniture = XMLDocument::child(roomNode, "bldg:interiorFurniture");
    if (!interiorFurniture) interiorFurniture = XMLDocument::child(roomNode, "interiorFurniture");

    while (interiorFurniture) {
        // interiorFurniture 的第一个子元素是 BuildingFurniture
        void* furnitureNode = XMLDocument::firstChildElement(interiorFurniture);
        if (furnitureNode) {
            std::string furnitureType = getLocalName(furnitureNode);
            if (furnitureType == "BuildingFurniture") {
                auto furniture = std::make_shared<CityObject>("BuildingFurniture");

                // 解析 gml:id
                std::string gmlId = XMLDocument::attribute(furnitureNode, "gml:id");
                if (!gmlId.empty()) {
                    furniture->setId(gmlId);
                }

                // 解析 gml:name
                void* nameNode = XMLDocument::child(furnitureNode, "gml:name");
                if (nameNode) {
                    furniture->setName(XMLDocument::text(nameNode));
                }

                // 解析 gml:description
                void* descNode = XMLDocument::child(furnitureNode, "gml:description");
                if (descNode) {
                    furniture->setDescription(XMLDocument::text(descNode));
                }

                // 解析 bldg:function
                void* funcNode = XMLDocument::child(furnitureNode, "bldg:function");
                if (!funcNode) funcNode = XMLDocument::child(furnitureNode, "function");
                if (funcNode) {
                    furniture->setDescription(XMLDocument::text(funcNode));
                }

                // 解析 LOD 几何体（主要是 lod4Geometry）
                parseLODGeometries(furnitureNode, furniture);

                room->addChildCityObject(furniture);
            }
        }

        // 查找下一个 interiorFurniture 兄弟元素
        void* next = XMLDocument::nextSiblingElement(interiorFurniture, "interiorFurniture");
        if (!next) next = XMLDocument::nextSiblingElement(interiorFurniture, "bldg:interiorFurniture");
        interiorFurniture = next;
    }
}

void CityGMLReader::parseAddresses(void* node, std::shared_ptr<CityObject> obj) {
    // 查找 bldg:address 或 address
    void* addressNode = XMLDocument::child(node, "bldg:address");
    if (!addressNode) addressNode = XMLDocument::child(node, "address");

    while (addressNode) {
        // address 的第一个子元素是 core:Address
        void* coreAddrNode = XMLDocument::firstChildElement(addressNode);
        if (coreAddrNode) {
            std::string addrType = getLocalName(coreAddrNode);
            if (addrType == "Address") {
                auto address = std::make_shared<Address>();

                // 解析 gml:id
                std::string gmlId = XMLDocument::attribute(coreAddrNode, "gml:id");
                if (!gmlId.empty()) {
                    address->setId(gmlId);
                }

                // 解析 core:xalAddress
                void* xalNode = XMLDocument::child(coreAddrNode, "core:xalAddress");
                if (!xalNode) xalNode = XMLDocument::child(coreAddrNode, "xalAddress");
                if (xalNode) {
                    // 提取完整的 xAL 地址文本
                    std::string xalAddress = XMLDocument::text(xalNode);
                    address->setXalAddress(xalAddress);
                }

                obj->addAddress(address);
            }
        }

        addressNode = XMLDocument::nextSiblingElement(addressNode, "bldg:address");
        if (!addressNode) addressNode = XMLDocument::nextSiblingElement(addressNode, "address");
    }
}

void CityGMLReader::parseBoundedBy(void* node, std::shared_ptr<CityObject> obj) {
    // 循环处理所有 boundedBy/boundary 子元素（每个 ThematicSurface 一个）
    void* boundedBy = nullptr;
    // 尝试多种前缀和名称组合
    const char* names[] = {"boundary", "boundedBy"};
    const char* prefixes[] = {"bldg:", "con:", ""};

    for (const char* name : names) {
        for (const char* prefix : prefixes) {
            boundedBy = XMLDocument::child(node, std::string(prefix) + name);
            if (boundedBy) break;
        }
        if (boundedBy) break;
    }

    while (boundedBy) {
        parseBoundedByElement(boundedBy, obj);

        // 查找下一个 boundary/boundedBy 兄弟元素
        void* next = XMLDocument::nextSiblingElement(boundedBy, "boundary");
        if (!next) next = XMLDocument::nextSiblingElement(boundedBy, "boundedBy");
        boundedBy = next;
    }
}

void CityGMLReader::parseBoundedByElement(void* boundedByNode, std::shared_ptr<CityObject> obj) {
    // 获取第一个子元素（主题表面类型）
    void* surfaceNode = XMLDocument::firstChildElement(boundedByNode);
    if (!surfaceNode) return;

    std::string surfaceType = getLocalName(surfaceNode);

    // 创建对应的表面对象
    std::shared_ptr<AbstractThematicSurface> surface = createThematicSurface(surfaceType);
    if (!surface) return;

    // 解析 gml:id
    std::string gmlId = XMLDocument::attribute(surfaceNode, "gml:id");
    if (!gmlId.empty()) {
        surface->setId(gmlId);
    }

    // 解析 gml:name
    void* nameNode = XMLDocument::child(surfaceNode, "gml:name");
    if (nameNode) {
        surface->setName(XMLDocument::text(nameNode));
    }

    // 解析 gml:description
    void* descNode = XMLDocument::child(surfaceNode, "gml:description");
    if (descNode) {
        surface->setDescription(XMLDocument::text(descNode));
    }

    // 解析几何体（lodXMultiSurface），优先取最高 LOD 的非空几何
    std::shared_ptr<MultiSurface> foundMultiSurface;
    int foundLod = -1;
    for (int lod = 4; lod >= 0; --lod) {
        std::string lodNames[] = {
            "lod" + std::to_string(lod) + "MultiSurface",
            "bldg:lod" + std::to_string(lod) + "MultiSurface",
            "con:lod" + std::to_string(lod) + "MultiSurface"
        };

        for (const auto& lodName : lodNames) {
            void* geomNode = XMLDocument::child(surfaceNode, lodName);
            if (geomNode) {
                void* geometryNode = XMLDocument::firstChildElement(geomNode);
                if (geometryNode) {
                    auto geometry = geometryParser_->parseGeometry(geometryNode);
                    if (geometry) {
                        foundMultiSurface = std::dynamic_pointer_cast<MultiSurface>(geometry);
                        foundLod = lod;
                    }
                }
                break;
            }
        }
        if (foundMultiSurface && foundMultiSurface->getGeometriesCount() > 0) {
            break;  // 找到最高 LOD 的非空几何，停止搜索
        }
        foundMultiSurface.reset();  // 重置，继续找更低的 LOD
        foundLod = -1;
    }
    if (foundMultiSurface && foundLod >= 0) {
        surface->setMultiSurface(foundMultiSurface);
    }

    // 解析开口（bldg:opening）
    parseOpenings(surfaceNode, surface);

    obj->addBoundedBySurface(surface);
}

std::shared_ptr<AbstractThematicSurface> CityGMLReader::createThematicSurface(const std::string& type) {
    if (type == "RoofSurface") return std::make_shared<RoofSurface>();
    if (type == "WallSurface") return std::make_shared<WallSurface>();
    if (type == "GroundSurface") return std::make_shared<GroundSurface>();
    if (type == "ClosureSurface") return std::make_shared<ClosureSurface>();
    if (type == "InteriorWallSurface") return std::make_shared<InteriorWallSurface>();
    if (type == "CeilingSurface") return std::make_shared<CeilingSurface>();
    if (type == "FloorSurface") return std::make_shared<FloorSurface>();
    if (type == "OuterCeilingSurface") return std::make_shared<OuterCeilingSurface>();
    if (type == "OuterFloorSurface") return std::make_shared<OuterFloorSurface>();
    // 通用处理
    return std::make_shared<AbstractThematicSurface>();
}

void CityGMLReader::parseOpenings(void* parentNode, std::shared_ptr<AbstractThematicSurface> surface) {
    // 查找 bldg:opening 或 opening
    void* openingNode = XMLDocument::child(parentNode, "bldg:opening");
    if (!openingNode) openingNode = XMLDocument::child(parentNode, "opening");

    while (openingNode) {
        void* openingChild = XMLDocument::firstChildElement(openingNode);
        if (openingChild) {
            std::string openingType = getLocalName(openingChild);
            auto opening = createOpening(openingType);

            if (opening) {
                // 解析 gml:id
                std::string gmlId = XMLDocument::attribute(openingChild, "gml:id");
                if (!gmlId.empty()) {
                    opening->setId(gmlId);
                }

                // 解析几何体
                for (int lod = 0; lod <= 4; ++lod) {
                    std::string lodNames[] = {
                        "lod" + std::to_string(lod) + "MultiSurface",
                        "bldg:lod" + std::to_string(lod) + "MultiSurface",
                        "con:lod" + std::to_string(lod) + "MultiSurface"
                    };

                    for (const auto& lodName : lodNames) {
                        void* geomNode = XMLDocument::child(openingChild, lodName);
                        if (geomNode) {
                            void* geometryNode = XMLDocument::firstChildElement(geomNode);
                            if (geometryNode) {
                                auto geometry = geometryParser_->parseGeometry(geometryNode);
                                if (geometry) {
                                    opening->setMultiSurface(std::dynamic_pointer_cast<MultiSurface>(geometry));
                                }
                            }
                            break;
                        }
                    }
                }

                surface->addOpening(opening);
            }
        }

        openingNode = XMLDocument::nextSiblingElement(openingNode, "bldg:opening");
        if (!openingNode) openingNode = XMLDocument::nextSiblingElement(openingNode, "opening");
    }
}

std::shared_ptr<AbstractOpening> CityGMLReader::createOpening(const std::string& type) {
    if (type == "Door") return std::make_shared<Door>();
    if (type == "Window") return std::make_shared<Window>();
    return std::make_shared<AbstractOpening>();
}

void CityGMLReader::parseNameAndDescription(void* node, std::shared_ptr<CityObject> obj) {
    void* nameNode = XMLDocument::child(node, "name");
    if (!nameNode) nameNode = XMLDocument::child(node, "gml:name");
    if (nameNode) {
        obj->setName(XMLDocument::text(nameNode));
    }

    void* descNode = XMLDocument::child(node, "description");
    if (!descNode) descNode = XMLDocument::child(node, "gml:description");
    if (descNode) {
        obj->setDescription(XMLDocument::text(descNode));
    }
}

void CityGMLReader::parseCreationDate(void* node, std::shared_ptr<CityObject> obj) {
    void* dateNode = XMLDocument::child(node, "creationDate");
    if (!dateNode) dateNode = XMLDocument::child(node, "core:creationDate");
    if (!dateNode) return;

    std::string dateStr = XMLDocument::text(dateNode);
    if (dateStr.empty()) return;

    // 解析日期格式: YYYY-MM-DD
    DateTime dt;
    std::istringstream iss(dateStr);
    char delimiter;
    iss >> dt.year >> delimiter >> dt.month >> delimiter >> dt.day;
    if (dt.year > 0) {
        obj->setCreationDate(dt);
    }
}

void CityGMLReader::parseRelativeToTerrain(void* node, std::shared_ptr<CityObject> obj) {
    void* relNode = XMLDocument::child(node, "relativeToTerrain");
    if (!relNode) relNode = XMLDocument::child(node, "core:relativeToTerrain");
    if (!relNode) return;

    std::string val = XMLDocument::text(relNode);
    if (val.empty()) return;

    RelativeToTerrain rel = ::citygml::parseRelativeToTerrain(val);
    obj->setRelativeToTerrain(rel);
}

void CityGMLReader::parseGenericAttributes(void* node, std::shared_ptr<CityObject> obj) {
    // ============================================================
    // gen:stringAttribute - 字符串属性
    // ============================================================
    void* child = XMLDocument::firstChildElement(node, "stringAttribute");
    if (!child) child = XMLDocument::firstChildElement(node, "gen:stringAttribute");

    while (child) {
        std::string name = XMLDocument::attribute(child, "name");
        void* valueNode = XMLDocument::child(child, "value");
        if (!valueNode) valueNode = XMLDocument::child(child, "gen:value");
        if (!valueNode) valueNode = XMLDocument::firstChildElement(child);

        std::string value = XMLDocument::text(valueNode);
        if (!name.empty() && !value.empty()) {
            auto attr = std::make_shared<GenericAttributeString>(name, value);
            obj->addGenericAttribute(attr);
        }
        child = XMLDocument::nextSiblingElement(child, "stringAttribute");
        if (!child) child = XMLDocument::nextSiblingElement(child, "gen:stringAttribute");
    }

    // ============================================================
    // gen:measureAttribute - 带单位的数值属性
    // ============================================================
    child = XMLDocument::firstChildElement(node, "measureAttribute");
    if (!child) child = XMLDocument::firstChildElement(node, "gen:measureAttribute");

    while (child) {
        std::string name = XMLDocument::attribute(child, "name");
        void* valueNode = XMLDocument::child(child, "value");
        if (!valueNode) valueNode = XMLDocument::child(child, "gen:value");
        if (!valueNode) valueNode = XMLDocument::firstChildElement(child);

        if (valueNode) {
            std::string valueStr = XMLDocument::text(valueNode);
            std::string uom = XMLDocument::attribute(valueNode, "uom");
            if (!name.empty() && !valueStr.empty()) {
                try {
                    double value = std::stod(valueStr);
                    // 带单位的数值存储，保留 uom 信息
                    auto attr = std::make_shared<GenericAttributeDouble>(name, value, uom);
                    obj->addGenericAttribute(attr);
                } catch (...) {}
            }
        }
        child = XMLDocument::nextSiblingElement(child, "measureAttribute");
        if (!child) child = XMLDocument::nextSiblingElement(child, "gen:measureAttribute");
    }

    // ============================================================
    // gen:intAttribute - 整型属性
    // ============================================================
    child = XMLDocument::firstChildElement(node, "intAttribute");
    if (!child) child = XMLDocument::firstChildElement(node, "gen:intAttribute");

    while (child) {
        std::string name = XMLDocument::attribute(child, "name");
        void* valueNode = XMLDocument::child(child, "value");
        if (!valueNode) valueNode = XMLDocument::child(child, "gen:value");
        if (!valueNode) valueNode = XMLDocument::firstChildElement(child);

        std::string valueStr = XMLDocument::text(valueNode);
        if (!name.empty() && !valueStr.empty()) {
            try {
                int value = std::stoi(valueStr);
                auto attr = std::make_shared<GenericAttributeInt>(name, value);
                obj->addGenericAttribute(attr);
            } catch (...) {}
        }
        child = XMLDocument::nextSiblingElement(child, "intAttribute");
        if (!child) child = XMLDocument::nextSiblingElement(child, "gen:intAttribute");
    }

    // ============================================================
    // gen:doubleAttribute - 双精度浮点属性
    // ============================================================
    child = XMLDocument::firstChildElement(node, "doubleAttribute");
    if (!child) child = XMLDocument::firstChildElement(node, "gen:doubleAttribute");

    while (child) {
        std::string name = XMLDocument::attribute(child, "name");
        void* valueNode = XMLDocument::child(child, "value");
        if (!valueNode) valueNode = XMLDocument::child(child, "gen:value");
        if (!valueNode) valueNode = XMLDocument::firstChildElement(child);

        std::string valueStr = XMLDocument::text(valueNode);
        if (!name.empty() && !valueStr.empty()) {
            try {
                double value = std::stod(valueStr);
                auto attr = std::make_shared<GenericAttributeDouble>(name, value);
                obj->addGenericAttribute(attr);
            } catch (...) {}
        }
        child = XMLDocument::nextSiblingElement(child, "doubleAttribute");
        if (!child) child = XMLDocument::nextSiblingElement(child, "gen:doubleAttribute");
    }

    // ============================================================
    // gen:booleanAttribute - 布尔属性
    // ============================================================
    child = XMLDocument::firstChildElement(node, "booleanAttribute");
    if (!child) child = XMLDocument::firstChildElement(node, "gen:booleanAttribute");

    while (child) {
        std::string name = XMLDocument::attribute(child, "name");
        void* valueNode = XMLDocument::child(child, "value");
        if (!valueNode) valueNode = XMLDocument::child(child, "gen:value");
        if (!valueNode) valueNode = XMLDocument::firstChildElement(child);

        std::string valueStr = XMLDocument::text(valueNode);
        if (!name.empty() && !valueStr.empty()) {
            // 布尔值存储为字符串 "true"/"false"
            std::string boolStr;
            if (valueStr == "true" || valueStr == "1" || valueStr == "True" || valueStr == "TRUE") {
                boolStr = "true";
            } else {
                boolStr = "false";
            }
            auto attr = std::make_shared<GenericAttributeString>(name, boolStr);
            obj->addGenericAttribute(attr);
        }
        child = XMLDocument::nextSiblingElement(child, "booleanAttribute");
        if (!child) child = XMLDocument::nextSiblingElement(child, "gen:booleanAttribute");
    }

    // ============================================================
    // gen:dateAttribute - 日期属性
    // ============================================================
    child = XMLDocument::firstChildElement(node, "dateAttribute");
    if (!child) child = XMLDocument::firstChildElement(node, "gen:dateAttribute");

    while (child) {
        std::string name = XMLDocument::attribute(child, "name");
        void* valueNode = XMLDocument::child(child, "value");
        if (!valueNode) valueNode = XMLDocument::child(child, "gen:value");
        if (!valueNode) valueNode = XMLDocument::firstChildElement(child);

        std::string value = XMLDocument::text(valueNode);
        if (!name.empty() && !value.empty()) {
            // 日期存储为 ISO 格式字符串
            auto attr = std::make_shared<GenericAttributeString>(name, value);
            obj->addGenericAttribute(attr);
        }
        child = XMLDocument::nextSiblingElement(child, "dateAttribute");
        if (!child) child = XMLDocument::nextSiblingElement(child, "gen:dateAttribute");
    }

    // ============================================================
    // gen:uriAttribute - URI/URL 属性
    // ============================================================
    child = XMLDocument::firstChildElement(node, "uriAttribute");
    if (!child) child = XMLDocument::firstChildElement(node, "gen:uriAttribute");

    while (child) {
        std::string name = XMLDocument::attribute(child, "name");
        void* valueNode = XMLDocument::child(child, "value");
        if (!valueNode) valueNode = XMLDocument::child(child, "gen:value");
        if (!valueNode) valueNode = XMLDocument::firstChildElement(child);

        std::string value = XMLDocument::text(valueNode);
        if (!name.empty() && !value.empty()) {
            auto attr = std::make_shared<GenericAttributeURI>(name, value);
            obj->addGenericAttribute(attr);
        }
        child = XMLDocument::nextSiblingElement(child, "uriAttribute");
        if (!child) child = XMLDocument::nextSiblingElement(child, "gen:uriAttribute");
    }

    // ============================================================
    // gen:linkAttribute - 链接属性 (XLink href)
    // ============================================================
    child = XMLDocument::firstChildElement(node, "linkAttribute");
    if (!child) child = XMLDocument::firstChildElement(node, "gen:linkAttribute");

    while (child) {
        std::string name = XMLDocument::attribute(child, "name");
        void* valueNode = XMLDocument::child(child, "value");
        if (!valueNode) valueNode = XMLDocument::child(child, "gen:value");
        if (!valueNode) valueNode = XMLDocument::firstChildElement(child);

        if (valueNode) {
            // 优先从 href 属性获取链接
            std::string href = XMLDocument::attribute(valueNode, "href");
            if (href.empty()) {
                href = XMLDocument::text(valueNode);
            }
            if (!name.empty() && !href.empty()) {
                auto attr = std::make_shared<GenericAttributeURI>(name, href);
                obj->addGenericAttribute(attr);
            }
        }
        child = XMLDocument::nextSiblingElement(child, "linkAttribute");
        if (!child) child = XMLDocument::nextSiblingElement(child, "gen:linkAttribute");
    }
}

void CityGMLReader::parseBuildingAttributes(void* node, std::shared_ptr<CityObject> obj) {
    // 解析 bldg:class
    void* classNode = XMLDocument::child(node, "class");
    if (!classNode) classNode = XMLDocument::child(node, "bldg:class");
    if (classNode) {
        std::string val = XMLDocument::text(classNode);
        if (!val.empty()) obj->setBuildingClass(val);
    }

    // 解析 bldg:function
    void* funcNode = XMLDocument::child(node, "function");
    if (!funcNode) funcNode = XMLDocument::child(node, "bldg:function");
    if (funcNode) {
        std::string val = XMLDocument::text(funcNode);
        if (!val.empty()) obj->setBuildingFunction(val);
    }

    // 解析 bldg:usage
    void* usageNode = XMLDocument::child(node, "usage");
    if (!usageNode) usageNode = XMLDocument::child(node, "bldg:usage");
    if (usageNode) {
        std::string val = XMLDocument::text(usageNode);
        if (!val.empty()) obj->setBuildingUsage(val);
    }

    // 解析 bldg:roofType
    void* roofNode = XMLDocument::child(node, "roofType");
    if (!roofNode) roofNode = XMLDocument::child(node, "bldg:roofType");
    if (roofNode) {
        std::string val = XMLDocument::text(roofNode);
        if (!val.empty()) obj->setRoofType(val);
    }

    // 解析 bldg:measuredHeight
    void* heightNode = XMLDocument::child(node, "measuredHeight");
    if (!heightNode) heightNode = XMLDocument::child(node, "bldg:measuredHeight");
    if (heightNode) {
        std::string val = XMLDocument::text(heightNode);
        if (!val.empty()) {
            try {
                double h = std::stod(val);
                obj->setMeasuredHeight(h);
            } catch (...) {}
        }
    }

    // 解析 bldg:yearOfConstruction
    void* yearNode = XMLDocument::child(node, "yearOfConstruction");
    if (!yearNode) yearNode = XMLDocument::child(node, "bldg:yearOfConstruction");
    if (yearNode) {
        std::string val = XMLDocument::text(yearNode);
        if (!val.empty()) {
            try {
                int year = std::stoi(val);
                obj->setYearOfConstruction(year);
            } catch (...) {}
        }
    }

    // 解析 bldg:storeysAboveGround
    void* storeysAboveNode = XMLDocument::child(node, "storeysAboveGround");
    if (!storeysAboveNode) storeysAboveNode = XMLDocument::child(node, "bldg:storeysAboveGround");
    if (storeysAboveNode) {
        std::string val = XMLDocument::text(storeysAboveNode);
        if (!val.empty()) {
            try {
                int storeys = std::stoi(val);
                obj->setStoreysAboveGround(storeys);
            } catch (...) {}
        }
    }

    // 解析 bldg:storeysBelowGround
    void* storeysBelowNode = XMLDocument::child(node, "storeysBelowGround");
    if (!storeysBelowNode) storeysBelowNode = XMLDocument::child(node, "bldg:storeysBelowGround");
    if (storeysBelowNode) {
        std::string val = XMLDocument::text(storeysBelowNode);
        if (!val.empty()) {
            try {
                int storeys = std::stoi(val);
                obj->setStoreysBelowGround(storeys);
            } catch (...) {}
        }
    }
}

void CityGMLReader::parseLODGeometries(void* node, std::shared_ptr<CityObject> obj) {
    // 辅助函数：尝试解析 lodGeom，可能内嵌几何，也可能只有 xlink:href 属性
    auto tryParseMultiSurface = [&](void* lodGeom, std::shared_ptr<MultiSurface>& outMS) -> bool {
        if (!lodGeom) return false;
        void* geometryNode = XMLDocument::firstChildElement(lodGeom);
        if (geometryNode) {
            auto geometry = geometryParser_->parseGeometry(geometryNode);
            if (geometry) {
                outMS = std::dynamic_pointer_cast<MultiSurface>(geometry);
                return outMS != nullptr;
            }
        } else {
            // lodGeom 是空元素，可能只有 xlink:href 属性
            std::string href = XMLDocument::attribute(lodGeom, "xlink_href");
            if (href.empty()) href = XMLDocument::attribute(lodGeom, "xlink:href");
            if (!href.empty()) {
                if (href[0] == '#') href.erase(0, 1);
                auto geometry = geometryParser_->resolveGeometryById(href);
                if (geometry) {
                    outMS = std::dynamic_pointer_cast<MultiSurface>(geometry);
                    return outMS != nullptr;
                }
            }
        }
        return false;
    };

    auto tryParseSolid = [&](void* lodGeom, std::shared_ptr<Solid>& outSolid) -> bool {
        if (!lodGeom) return false;
        void* geometryNode = XMLDocument::firstChildElement(lodGeom);
        if (geometryNode) {
            auto geometry = geometryParser_->parseGeometry(geometryNode);
            if (geometry) {
                outSolid = std::dynamic_pointer_cast<Solid>(geometry);
                return outSolid != nullptr;
            }
        }
        return false;
    };

    // 解析 LOD0 - 特殊处理: FootPrint, RoofEdge, MultiSurface
    // lod0FootPrint
    {
        void* lodGeom = XMLDocument::child(node, "lod0FootPrint");
        if (!lodGeom) lodGeom = XMLDocument::child(node, "bldg:lod0FootPrint");
        std::shared_ptr<MultiSurface> ms;
        if (tryParseMultiSurface(lodGeom, ms)) {
            obj->setLod0FootPrint(ms);
        }
    }
    // lod0RoofEdge
    {
        void* lodGeom = XMLDocument::child(node, "lod0RoofEdge");
        if (!lodGeom) lodGeom = XMLDocument::child(node, "bldg:lod0RoofEdge");
        std::shared_ptr<MultiSurface> ms;
        if (tryParseMultiSurface(lodGeom, ms)) {
            obj->setLod0RoofEdge(ms);
        }
    }
    // lod0MultiSurface
    {
        void* lodGeom = XMLDocument::child(node, "lod0MultiSurface");
        if (!lodGeom) lodGeom = XMLDocument::child(node, "bldg:lod0MultiSurface");
        std::shared_ptr<MultiSurface> ms;
        if (tryParseMultiSurface(lodGeom, ms)) {
            obj->setLod0MultiSurface(ms);
        }
    }

    // 解析 LOD 1-4 - MultiSurface
    for (int lod = 1; lod <= 4; ++lod) {
        std::string lodSurfaceName1 = "lod" + std::to_string(lod) + "MultiSurface";
        std::string lodSurfaceName2 = "bldg:lod" + std::to_string(lod) + "MultiSurface";
        // Furniture 使用 lod4Geometry 而不是 lod4MultiSurface
        std::string lodGeomName = "lod" + std::to_string(lod) + "Geometry";
        std::string lodGeomName2 = "bldg:lod" + std::to_string(lod) + "Geometry";

        void* lodGeom = XMLDocument::child(node, lodSurfaceName1);
        if (!lodGeom) lodGeom = XMLDocument::child(node, lodSurfaceName2);
        if (!lodGeom) lodGeom = XMLDocument::child(node, lodGeomName);
        if (!lodGeom) lodGeom = XMLDocument::child(node, lodGeomName2);

        std::shared_ptr<MultiSurface> ms;
        if (tryParseMultiSurface(lodGeom, ms)) {
            switch (lod) {
                case 1: obj->setLod1MultiSurface(ms); break;
                case 2: obj->setLod2MultiSurface(ms); break;
                case 3: obj->setLod3MultiSurface(ms); break;
                case 4: obj->setLod4MultiSurface(ms); break;
            }
        }
    }

    // 解析 LOD 1-4 - Solid
    for (int lod = 1; lod <= 4; ++lod) {
        std::string lodSolidName1 = "lod" + std::to_string(lod) + "Solid";
        std::string lodSolidName2 = "bldg:lod" + std::to_string(lod) + "Solid";
        // Furniture 使用 lodGeometry 也可以包含 Solid
        std::string lodGeomName = "lod" + std::to_string(lod) + "Geometry";
        std::string lodGeomName2 = "bldg:lod" + std::to_string(lod) + "Geometry";

        void* lodGeom = XMLDocument::child(node, lodSolidName1);
        if (!lodGeom) lodGeom = XMLDocument::child(node, lodSolidName2);
        if (!lodGeom) lodGeom = XMLDocument::child(node, lodGeomName);
        if (!lodGeom) lodGeom = XMLDocument::child(node, lodGeomName2);

        std::shared_ptr<Solid> solid;
        if (tryParseSolid(lodGeom, solid)) {
            switch (lod) {
                case 1: obj->setLod1Solid(solid); break;
                case 2: obj->setLod2Solid(solid); break;
                case 3: obj->setLod3Solid(solid); break;
                case 4: obj->setLod4Solid(solid); break;
            }
        }
    }
}

void CityGMLReader::parseImplicitRepresentation(void* node, std::shared_ptr<CityObject> obj) {
    // Parse lod1ImplicitRepresentation
    {
        void* implRepNode = XMLDocument::child(node, "lod1ImplicitRepresentation");
        if (!implRepNode) implRepNode = XMLDocument::child(node, "bldg:lod1ImplicitRepresentation");
        if (!implRepNode) implRepNode = XMLDocument::child(node, "gen:lod1ImplicitRepresentation");
        if (implRepNode) {
            void* implNode = XMLDocument::firstChildElement(implRepNode);
            if (implNode) {
                auto geom = geometryParser_->parseGeometry(implNode);
                if (geom) obj->setLod1ImplicitRepresentation(geom);
            }
        }
    }
    // Parse lod2ImplicitRepresentation
    {
        void* implRepNode = XMLDocument::child(node, "lod2ImplicitRepresentation");
        if (!implRepNode) implRepNode = XMLDocument::child(node, "bldg:lod2ImplicitRepresentation");
        if (!implRepNode) implRepNode = XMLDocument::child(node, "gen:lod2ImplicitRepresentation");
        if (implRepNode) {
            void* implNode = XMLDocument::firstChildElement(implRepNode);
            if (implNode) {
                auto geom = geometryParser_->parseGeometry(implNode);
                if (geom) obj->setLod2ImplicitRepresentation(geom);
            }
        }
    }
    // Parse lod3ImplicitRepresentation
    {
        void* implRepNode = XMLDocument::child(node, "lod3ImplicitRepresentation");
        if (!implRepNode) implRepNode = XMLDocument::child(node, "bldg:lod3ImplicitRepresentation");
        if (!implRepNode) implRepNode = XMLDocument::child(node, "gen:lod3ImplicitRepresentation");
        if (implRepNode) {
            void* implNode = XMLDocument::firstChildElement(implRepNode);
            if (implNode) {
                auto geom = geometryParser_->parseGeometry(implNode);
                if (geom) obj->setLod3ImplicitRepresentation(geom);
            }
        }
    }
}

std::string CityGMLReader::getLocalName(void* node) {
    if (!node) return "";
    std::string fullName = XMLDocument::nodeName(node);
    
    size_t pos = fullName.find(':');
    if (pos != std::string::npos) {
        return fullName.substr(pos + 1);
    }
    return fullName;
}

} // namespace citygml