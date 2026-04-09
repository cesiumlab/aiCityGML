#include "citygml/parser/citygml_reader.h"
#include "citygml/parser/xml_document.h"
#include "citygml/parser/gml_envelope_parser.h"
#include "citygml/parser/gml_geometry_parser.h"
#include "citygml/core/citygml_object.h"

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
    
    void* boundedBy = XMLDocument::child(node, "boundedBy");
    if (!boundedBy) {
        boundedBy = XMLDocument::child(node, "gml:boundedBy");
    }
    if (boundedBy) {
        GMLEnvelopeParser envelopeParser;
        auto envelope = envelopeParser.parse(boundedBy, context_);
        if (envelope) {
            cityModel->setEnvelope(*envelope);
        }
    }
    
    auto members = XMLDocument::children(node, "cityObjectMember");
    for (void* member : members) {
        auto cityObject = readCityObject(member);
        if (cityObject) {
            cityModel->addCityObject(cityObject);
        }
    }
    
    return cityModel;
}

std::shared_ptr<CityObject> CityGMLReader::readCityObject(void* node) {
    if (!node) return nullptr;
    
    std::string nodeName = getLocalName(node);
    
    if (nodeName == "Building" || nodeName == "bldg:Building") {
        return readBuilding(node);
    }
    
    auto cityObject = std::make_shared<CityObject>(nodeName);
    
    parseNameAndDescription(node, cityObject);
    
    auto boundedBy = parseBoundedBy(node);
    if (boundedBy) {
        cityObject->setEnvelope(*boundedBy);
    }
    
    parseLODGeometries(node, cityObject);
    
    return cityObject;
}

std::shared_ptr<CityObject> CityGMLReader::readBuilding(void* node) {
    auto building = std::make_shared<CityObject>("Building");
    
    parseNameAndDescription(node, building);
    
    auto boundedBy = parseBoundedBy(node);
    if (boundedBy) {
        building->setEnvelope(*boundedBy);
    }
    
    parseLODGeometries(node, building);
    
    return building;
}

std::shared_ptr<Envelope> CityGMLReader::parseBoundedBy(void* node) {
    void* boundedBy = XMLDocument::child(node, "boundedBy");
    if (!boundedBy) {
        boundedBy = XMLDocument::child(node, "gml:boundedBy");
    }
    if (!boundedBy) return nullptr;
    
    GMLEnvelopeParser parser;
    return parser.parse(boundedBy, context_);
}

void CityGMLReader::parseNameAndDescription(void* node, std::shared_ptr<CityObject> obj) {
    void* nameNode = XMLDocument::child(node, "name");
    if (!nameNode) {
        nameNode = XMLDocument::child(node, "gml:name");
    }
    if (nameNode) {
        std::string name = XMLDocument::text(nameNode);
        obj->setName(name);
    }
    
    void* descNode = XMLDocument::child(node, "description");
    if (!descNode) {
        descNode = XMLDocument::child(node, "gml:description");
    }
    if (descNode) {
        std::string desc = XMLDocument::text(descNode);
        obj->setDescription(desc);
    }
}

void CityGMLReader::parseLODGeometries(void* node, std::shared_ptr<CityObject> obj) {
    for (int lod = 1; lod <= 4; ++lod) {
        std::string lodSurfaceName = "lod" + std::to_string(lod) + "MultiSurface";
        void* lodSurface = XMLDocument::child(node, lodSurfaceName);
        if (!lodSurface) {
            lodSurface = XMLDocument::child(node, "bldg:" + lodSurfaceName);
        }
        if (lodSurface) {
            void* geometryNode = XMLDocument::firstChildElement(lodSurface);
            if (geometryNode) {
                auto geometry = geometryParser_->parseGeometry(geometryNode);
                if (geometry) {
                    obj->setLODGeometry(lod, geometry);
                }
            }
        }
        
        std::string lodSolidName = "lod" + std::to_string(lod) + "Solid";
        void* lodSolid = XMLDocument::child(node, lodSolidName);
        if (!lodSolid) {
            lodSolid = XMLDocument::child(node, "bldg:" + lodSolidName);
        }
        if (lodSolid) {
            void* geometryNode = XMLDocument::firstChildElement(lodSolid);
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