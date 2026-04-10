#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <optional>
#include <array>
#include <sstream>

namespace citygml {

class CityModel;
class GMLGeometryParser;
class CityGMLReader;
class AbstractAppearance;
class X3DMaterial;
class ParameterizedTexture;
class AbstractGeometry;
class Polygon;

struct ParseResult {
    bool success = false;
    std::string errorMessage;
    std::string warningMessage;

    static ParseResult successResult() { return {true, "", ""}; }
    static ParseResult error(const std::string& msg) { return {false, msg, ""}; }
    static ParseResult warning(const std::string& msg, const std::string& warn) { return {false, msg, warn}; }
};

struct ParseOptions {
    bool parseAppearance = true;
    bool parseTexture = true;
    bool validateSchema = false;
    int maxLODLevel = 4;

    static ParseOptions defaultOptions() { return {}; }
};

class ParserContext {
public:
    ParserContext();
    ~ParserContext();

    void setSrsName(const std::string& srs) { srsName_ = srs; }
    const std::string& getSrsName() const { return srsName_; }

    void pushNamespace(const std::string& prefix, const std::string& uri);
    std::optional<std::string> getNamespaceUri(const std::string& prefix) const;

    void setFeatureName(const std::string& name) { featureName_ = name; }
    const std::string& getFeatureName() const { return featureName_; }

    void addWarning(const std::string& warn) { warnings_.push_back(warn); }
    const std::vector<std::string>& getWarnings() const { return warnings_; }

private:
    std::string srsName_;
    std::map<std::string, std::string> namespaces_;
    std::string featureName_;
    std::vector<std::string> warnings_;
};

class CityGMLParser {
public:
    CityGMLParser();
    ~CityGMLParser();

    ParseResult parse(const std::string& filePath, std::shared_ptr<CityModel>& cityModel, const ParseOptions& options = ParseOptions::defaultOptions());
    ParseResult parseString(const std::string& xmlContent, std::shared_ptr<CityModel>& cityModel, const ParseOptions& options = ParseOptions::defaultOptions());

    std::shared_ptr<ParserContext> getContext() const { return context_; }

private:
    std::shared_ptr<ParserContext> context_;
    std::shared_ptr<GMLGeometryParser> geometryParser_;
    std::shared_ptr<CityGMLReader> cityGMLReader_;

    ParseResult initializeParser(const std::string& filePath);
    ParseResult parseCityModelNode(void* node, std::shared_ptr<CityModel>& cityModel);

    std::shared_ptr<AbstractAppearance> parseAppearance(void* node);
    std::shared_ptr<X3DMaterial> parseX3DMaterial(void* node);
    std::shared_ptr<ParameterizedTexture> parseParameterizedTexture(void* node);
    std::array<double, 4> parseColor(const std::string& colorStr);

    void collectPolygons(
        std::shared_ptr<AbstractGeometry> geom,
        std::map<std::string, std::shared_ptr<Polygon>>& polygonMap);

    std::string stripFragment(const std::string& uri) const;
};

} // namespace citygml
