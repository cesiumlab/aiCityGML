#include "citygml/parser/gml_envelope_parser.h"
#include "citygml/parser/xml_document.h"
#include "citygml/parser/parser_context.h"
#include "citygml/core/citygml_base.h"
#include "citygml/core/citygml_geometry.h"
#include <sstream>
#include <iostream>

namespace citygml {

GMLEnvelopeParser::GMLEnvelopeParser() = default;
GMLEnvelopeParser::~GMLEnvelopeParser() = default;

std::shared_ptr<Envelope> GMLEnvelopeParser::parse(void* node, std::shared_ptr<ParserContext> context) {
    if (!node) return nullptr;

    void* envelopeNode = XMLDocument::child(node, "Envelope");
    if (!envelopeNode) {
        envelopeNode = XMLDocument::child(node, "gml:Envelope");
    }
    if (!envelopeNode) {
        envelopeNode = node;
    }

    std::string srsName = getAttribute(envelopeNode, "srsName");
    if (context) {
        context->setSrsName(srsName);
    }

    std::string srsDimStr = getAttribute(envelopeNode, "srsDimension");
    int srsDimension = 3;
    if (!srsDimStr.empty()) {
        srsDimension = std::stoi(srsDimStr);
    }

    std::string lowerCornerStr = getChildText(envelopeNode, "lowerCorner");
    if (lowerCornerStr.empty()) {
        lowerCornerStr = getChildText(envelopeNode, "gml:lowerCorner");
    }
    auto lowerCorner = parsePosList(lowerCornerStr, srsDimension);

    std::string upperCornerStr = getChildText(envelopeNode, "upperCorner");
    if (upperCornerStr.empty()) {
        upperCornerStr = getChildText(envelopeNode, "gml:upperCorner");
    }
    auto upperCorner = parsePosList(upperCornerStr, srsDimension);

    if (lowerCorner.empty() || upperCorner.empty()) {
        return nullptr;
    }

    return Envelope::create(
        lowerCorner[0].x(), lowerCorner[0].y(), lowerCorner[0].z(),
        upperCorner[0].x(), upperCorner[0].y(), upperCorner[0].z(),
        srsName
    );
}

std::vector<Point> GMLEnvelopeParser::parsePosList(const std::string& posListStr, int srsDimension) {
    std::vector<Point> points;
    
    if (posListStr.empty()) return points;
    
    std::istringstream iss(posListStr);
    std::vector<double> coords;
    double val;
    while (iss >> val) {
        coords.push_back(val);
    }
    
    for (size_t i = 0; i + static_cast<size_t>(srsDimension - 1) < coords.size(); i += srsDimension) {
        double x = coords[i];
        double y = coords[i + 1];
        double z = (srsDimension >= 3 && i + 2 < coords.size()) ? coords[i + 2] : 0.0;
        points.push_back(Point(x, y, z));
    }
    
    return points;
}

std::string GMLEnvelopeParser::getAttribute(void* node, const std::string& attrName) {
    return XMLDocument::attribute(node, attrName);
}

std::string GMLEnvelopeParser::getChildText(void* parent, const std::string& childName) {
    void* child = XMLDocument::child(parent, childName);
    if (!child) return "";
    return XMLDocument::text(child);
}

} // namespace citygml