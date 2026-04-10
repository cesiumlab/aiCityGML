#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstddef>

struct XMLElement;

namespace citygml {

// ============================================================
// XMLDocument - TinyXML2 封装类
// ============================================================

class XMLDocument {
public:
    XMLDocument();
    ~XMLDocument();
    
    bool loadFile(const char* filename);
    bool parse(const char* xmlString);
    
    void* root() const;
    const char* errorStr() const;
    
    static void* child(void* node, const std::string& name);
    static std::vector<void*> children(void* node, const std::string& name);
    static std::string attribute(void* node, const std::string& name);
    static std::string text(void* node);
    static std::string nodeName(void* node);
    static void* firstChildElement(void* node, const std::string& name = "");
    static void* nextSiblingElement(void* node, const std::string& name = "");
    static std::string attributeNS(void* node, const std::string& uri, const std::string& localName);
    static std::string getNamespace(void* node, const std::string& prefix);
    
private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace citygml