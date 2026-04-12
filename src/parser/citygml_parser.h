#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <array>

#include "core/citygml_appearance.h"
#include "core/citygml_geometry.h"

namespace citygml {

class CityModel;
class ParserContext;
class GMLGeometryParser;
class CityGMLReader;

// ============================================================
// ParseOptions - 解析选项
// ============================================================
struct ParseOptions {
    bool strictMode = false;
    bool ignoreErrors = true;
};

// ============================================================
// ParseResult - 解析结果
// ============================================================
struct ParseResult {
    bool success;
    std::string errorMessage;

    static ParseResult successResult() {
        return {true, ""};
    }

    static ParseResult error(const std::string& msg) {
        return {false, msg};
    }
};

// ============================================================
// CityGMLParser - CityGML 解析器主类
// ============================================================
class CityGMLParser {
public:
    CityGMLParser();
    ~CityGMLParser();

    ParseResult parse(const std::string& filePath,
                     std::shared_ptr<CityModel>& cityModel,
                     const ParseOptions& options = {});

    ParseResult parseString(const std::string& xmlContent,
                           std::shared_ptr<CityModel>& cityModel,
                           const ParseOptions& options = {});

    const std::vector<std::string>& getWarnings() const;

private:
    ParseResult initializeParser(const std::string& filePath);
    ParseResult parseCityModelNode(void* node, std::shared_ptr<CityModel>& cityModel);

    void collectPolygons(std::shared_ptr<AbstractGeometry> geom,
                         std::map<std::string, std::shared_ptr<Polygon>>& polygonMap);

    std::string stripFragment(const std::string& uri) const;

    AppearancePtr parseAppearance(void* node);
    X3DMaterialPtr parseX3DMaterial(void* node);
    ParameterizedTexturePtr parseParameterizedTexture(void* node);
    std::array<double, 4> parseColor(const std::string& colorStr);

    std::shared_ptr<ParserContext> context_;
    std::shared_ptr<GMLGeometryParser> geometryParser_;
    std::shared_ptr<CityGMLReader> cityGMLReader_;
    std::vector<std::string> warnings_;
};

// ============================================================
// Free function - 解析 CityGML 文件
// ============================================================
std::shared_ptr<CityModel> parseCityGML(const std::string& filePath,
                                        std::string* errorMsg = nullptr);

} // namespace citygml
