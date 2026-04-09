#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>

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
    
private:
    std::string srsName_;
    std::map<std::string, std::string> namespaces_;
    std::string featureName_;
    std::vector<std::string> warnings_;
};

} // namespace citygml