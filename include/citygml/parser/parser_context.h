#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>

namespace citygml {

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

    // Polygon 注册表，支持 xlink:href 引用解析
    void registerPolygon(const std::string& id, void* node);
    void* getPolygonById(const std::string& id) const;

private:
    std::string srsName_;
    std::map<std::string, std::string> namespaces_;
    std::string featureName_;
    std::vector<std::string> warnings_;
    // 按 gml:id 索引的 Polygon 节点指针映射表
    std::map<std::string, void*> polygonRegistry_;
};

} // namespace citygml