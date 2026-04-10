#include "citygml/parser/citygml_reader.h"
#include "citygml/parser/xml_document.h"
#include "citygml/parser/gml_envelope_parser.h"
#include "citygml/parser/gml_geometry_parser.h"
#include "citygml/core/citygml_object.h"
#include "citygml/core/citygml_base.h"
#include "citygml/core/citygml_feature.h"
#include <sstream>
#include <iomanip>

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

std::shared_ptr<CityObject> CityGMLReader::readCityObject(void* node) {
    if (!node) return nullptr;
    
    std::string nodeName = getLocalName(node);
    
    // 根据类型创建对象
    if (nodeName == "Building") {
        return readBuilding(node);
    }
    
    auto cityObject = std::make_shared<CityObject>(nodeName);
    
    parseNameAndDescription(node, cityObject);
    parseBoundedBy(node, cityObject);
    parseLODGeometries(node, cityObject);
    
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
    parseBoundedBy(node, building);
    parseCreationDate(node, building);
    parseRelativeToTerrain(node, building);
    parseGenericAttributes(node, building);
    parseBuildingAttributes(node, building);
    parseLODGeometries(node, building);

    return building;
}

void CityGMLReader::parseBoundedBy(void* node, std::shared_ptr<CityObject> obj) {
    void* boundedBy = XMLDocument::child(node, "boundedBy");
    if (!boundedBy) boundedBy = XMLDocument::child(node, "gml:boundedBy");
    if (!boundedBy) return;
    
    GMLEnvelopeParser parser;
    auto envelope = parser.parse(boundedBy, context_);
    if (envelope) {
        obj->setEnvelope(*envelope);
    }
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
    // 查找 gen:stringAttribute
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

    // 查找 gen:measureAttribute
    child = XMLDocument::firstChildElement(node, "measureAttribute");
    if (!child) child = XMLDocument::firstChildElement(node, "gen:measureAttribute");

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
        child = XMLDocument::nextSiblingElement(child, "measureAttribute");
        if (!child) child = XMLDocument::nextSiblingElement(child, "gen:measureAttribute");
    }
}

void CityGMLReader::parseBuildingAttributes(void* node, std::shared_ptr<CityObject> obj) {
    // 解析 bldg:yearOfConstruction
    void* yearNode = XMLDocument::child(node, "yearOfConstruction");
    if (!yearNode) yearNode = XMLDocument::child(node, "bldg:yearOfConstruction");
    // 注意: yearOfConstruction 是简单整数，不是属性

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
    // 解析 LOD0
    for (int lod = 0; lod <= 4; ++lod) {
        // 查找 MultiSurface
        std::string lodSurfaceName1 = "lod" + std::to_string(lod) + "MultiSurface";
        std::string lodSurfaceName2 = "bldg:lod" + std::to_string(lod) + "MultiSurface";

        void* lodGeom = XMLDocument::child(node, lodSurfaceName1);
        if (!lodGeom) lodGeom = XMLDocument::child(node, lodSurfaceName2);

        // 特殊处理 LOD0: lod0FootPrint, lod0RoofEdge
        if (!lodGeom && lod == 0) {
            lodGeom = XMLDocument::child(node, "lod0FootPrint");
            if (!lodGeom) lodGeom = XMLDocument::child(node, "bldg:lod0FootPrint");
        }
        if (!lodGeom && lod == 0) {
            lodGeom = XMLDocument::child(node, "lod0RoofEdge");
            if (!lodGeom) lodGeom = XMLDocument::child(node, "bldg:lod0RoofEdge");
        }

        if (lodGeom) {
            void* geometryNode = XMLDocument::firstChildElement(lodGeom);
            if (geometryNode) {
                auto geometry = geometryParser_->parseGeometry(geometryNode);
                if (geometry) {
                    obj->setLODGeometry(lod, geometry);
                }
            }
        }

        // 查找 Solid
        std::string lodSolidName1 = "lod" + std::to_string(lod) + "Solid";
        std::string lodSolidName2 = "bldg:lod" + std::to_string(lod) + "Solid";

        lodGeom = XMLDocument::child(node, lodSolidName1);
        if (!lodGeom) lodGeom = XMLDocument::child(node, lodSolidName2);
        if (lodGeom) {
            void* geometryNode = XMLDocument::firstChildElement(lodGeom);
            if (geometryNode) {
                auto geometry = geometryParser_->parseGeometry(geometryNode);
                if (geometry) {
                    obj->setLODGeometry(lod, geometry);
                }
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