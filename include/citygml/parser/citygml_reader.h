#pragma once

#include <string>
#include <memory>
#include <map>
#include "citygml/core/citygml_object.h"
#include "citygml/parser/gml_geometry_parser.h"

namespace citygml {

class ParserContext;
struct Envelope;

class CityGMLReader {
public:
    CityGMLReader(std::shared_ptr<ParserContext> context, std::shared_ptr<GMLGeometryParser> geometryParser);
    ~CityGMLReader();
    
    std::shared_ptr<CityObject> readCityObject(void* node);
    std::shared_ptr<CityObject> readBuilding(void* node);
    std::shared_ptr<CityModel> readCityModel(void* node);
    
private:
    void parseBoundedBy(void* node, std::shared_ptr<CityObject> obj);
    void parseNameAndDescription(void* node, std::shared_ptr<CityObject> obj);
    void parseLODGeometries(void* node, std::shared_ptr<CityObject> obj);
    std::string getLocalName(void* node);
    
    std::shared_ptr<ParserContext> context_;
    std::shared_ptr<GMLGeometryParser> geometryParser_;
    std::map<std::string, std::string> namespaces_;
};

} // namespace citygml