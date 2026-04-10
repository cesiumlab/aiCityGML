#include "citygml/citygml_parser.h"
#include "citygml/parser/xml_document.h"
#include "citygml/parser/gml_geometry_parser.h"
#include "citygml/parser/citygml_reader.h"
#include "citygml/parser/gml_envelope_parser.h"

namespace citygml {

CityGMLParser::CityGMLParser()
    : context_(std::make_shared<ParserContext>())
    , geometryParser_(std::make_shared<GMLGeometryParser>(context_))
    , cityGMLReader_(std::make_shared<CityGMLReader>(context_, geometryParser_))
{
}

CityGMLParser::~CityGMLParser() = default;

ParseResult CityGMLParser::parse(const std::string& filePath, std::shared_ptr<CityModel>& cityModel, const ParseOptions& options) {
    if (filePath.empty()) {
        return ParseResult::error("Empty file path");
    }
    
    XMLDocument doc;
    if (!doc.loadFile(filePath.c_str())) {
        return ParseResult::error(std::string("XML parse failed: ") + doc.errorStr());
    }
    
    void* root = doc.root();
    if (!root) {
        return ParseResult::error("No root node");
    }
    
    auto result = initializeParser(filePath);
    if (!result.success) {
        return result;
    }
    
    return parseCityModelNode(root, cityModel);
}

ParseResult CityGMLParser::parseString(const std::string& xmlContent, std::shared_ptr<CityModel>& cityModel, const ParseOptions& options) {
    if (xmlContent.empty()) {
        return ParseResult::error("Empty XML content");
    }
    
    XMLDocument doc;
    if (!doc.parse(xmlContent.c_str())) {
        return ParseResult::error(std::string("XML parse failed: ") + doc.errorStr());
    }
    
    void* root = doc.root();
    if (!root) {
        return ParseResult::error("No root node");
    }
    
    context_ = std::make_shared<ParserContext>();
    geometryParser_ = std::make_shared<GMLGeometryParser>(context_);
    cityGMLReader_ = std::make_shared<CityGMLReader>(context_, geometryParser_);
    
    return parseCityModelNode(root, cityModel);
}

ParseResult CityGMLParser::initializeParser(const std::string& filePath) {
    context_ = std::make_shared<ParserContext>();
    geometryParser_ = std::make_shared<GMLGeometryParser>(context_);
    cityGMLReader_ = std::make_shared<CityGMLReader>(context_, geometryParser_);
    
    return ParseResult::successResult();
}

ParseResult CityGMLParser::parseCityModelNode(void* node, std::shared_ptr<CityModel>& cityModel) {
    cityModel = std::make_shared<CityModel>();

    // 解析 ID (gml:id)
    std::string gmlId = XMLDocument::attribute(node, "gml:id");
    if (!gmlId.empty()) {
        cityModel->setId(gmlId);
    }

    // 解析名称 (gml:name)
    void* nameNode = XMLDocument::child(node, "name");
    if (!nameNode) nameNode = XMLDocument::child(node, "gml:name");
    if (nameNode) {
        cityModel->setName(XMLDocument::text(nameNode));
    }

    void* boundedBy = XMLDocument::child(node, "boundedBy");
    if (!boundedBy) {
        boundedBy = XMLDocument::child(node, "gml:boundedBy");
    }
    if (!boundedBy) {
        boundedBy = XMLDocument::child(node, "core:boundedBy");
    }

    if (boundedBy) {
        GMLEnvelopeParser envelopeParser;
        auto envelope = envelopeParser.parse(boundedBy, context_);
        if (envelope) {
            cityModel->setEnvelope(*envelope);
        }
    }

    std::vector<void*> memberList = XMLDocument::children(node, "cityObjectMember");
    if (memberList.empty()) {
        memberList = XMLDocument::children(node, "core:cityObjectMember");
    }

    for (size_t i = 0; i < memberList.size(); ++i) {
        void* objNode = XMLDocument::firstChildElement(memberList[i]);
        if (objNode) {
            auto cityObject = cityGMLReader_->readCityObject(objNode);
            if (cityObject) {
                cityModel->addCityObject(cityObject);
            }
        }
    }

    return ParseResult::successResult();
}

} // namespace citygml