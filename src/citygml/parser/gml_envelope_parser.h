#pragma once

#include <string>
#include <vector>
#include <memory>

namespace citygml {

struct Envelope;
class Point;
class ParserContext;

class GMLEnvelopeParser {
public:
    GMLEnvelopeParser();
    ~GMLEnvelopeParser();
    
    std::shared_ptr<Envelope> parse(void* node, std::shared_ptr<ParserContext> context);
    
private:
    std::vector<Point> parsePosList(const std::string& posListStr, int srsDimension);
    std::string getAttribute(void* node, const std::string& attrName);
    std::string getChildText(void* parent, const std::string& childName);
};

} // namespace citygml