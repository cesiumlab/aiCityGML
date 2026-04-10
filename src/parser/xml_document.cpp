#include "citygml/parser/xml_document.h"
#include "tinyxml2.h"
#include <vector>
#include <string>

namespace citygml {

static std::string getLocalName(const char* name) {
    if (!name) return "";
    std::string fullName(name);
    size_t pos = fullName.find(':');
    if (pos != std::string::npos) {
        return fullName.substr(pos + 1);
    }
    return fullName;
}

static bool nameMatches(const char* nodeName, const std::string& targetName) {
    if (!nodeName) return false;
    std::string nodeLocalName = getLocalName(nodeName);
    return nodeLocalName == targetName;
}

struct XMLDocument::Impl {
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLElement* rootElement = nullptr;
    std::string errorStr;
};

XMLDocument::XMLDocument() : pImpl_(std::make_unique<Impl>()) {}
XMLDocument::~XMLDocument() = default;

bool XMLDocument::loadFile(const char* filename) {
    if (pImpl_->doc.LoadFile(filename) != tinyxml2::XML_SUCCESS) {
        pImpl_->errorStr = pImpl_->doc.ErrorStr();
        return false;
    }
    pImpl_->rootElement = pImpl_->doc.RootElement();
    return true;
}

bool XMLDocument::parse(const char* xmlString) {
    if (pImpl_->doc.Parse(xmlString) != tinyxml2::XML_SUCCESS) {
        pImpl_->errorStr = pImpl_->doc.ErrorStr();
        return false;
    }
    pImpl_->rootElement = pImpl_->doc.RootElement();
    return true;
}

void* XMLDocument::root() const {
    return pImpl_->rootElement;
}

const char* XMLDocument::errorStr() const {
    return pImpl_->errorStr.c_str();
}

void* XMLDocument::child(void* node, const std::string& name) {
    if (!node) return nullptr;
    auto* elem = static_cast<tinyxml2::XMLElement*>(node);
    auto* child = elem->FirstChildElement(name.c_str());
    if (child) return child;
    child = elem->FirstChildElement();
    while (child) {
        if (nameMatches(child->Name(), name)) {
            return child;
        }
        child = child->NextSiblingElement();
    }
    return nullptr;
}

std::vector<void*> XMLDocument::children(void* node, const std::string& name) {
    std::vector<void*> result;
    if (!node) return result;
    auto* elem = static_cast<tinyxml2::XMLElement*>(node);
    for (auto* child = elem->FirstChildElement(name.c_str());
         child != nullptr;
         child = child->NextSiblingElement(name.c_str())) {
        result.push_back(child);
    }
    if (result.empty()) {
        for (auto* child = elem->FirstChildElement();
             child != nullptr;
             child = child->NextSiblingElement()) {
            if (nameMatches(child->Name(), name)) {
                result.push_back(child);
            }
        }
    }
    return result;
}

std::string XMLDocument::attribute(void* node, const std::string& name) {
    if (!node) return "";
    auto* elem = static_cast<tinyxml2::XMLElement*>(node);
    const char* attr = elem->Attribute(name.c_str());
    return attr ? std::string(attr) : "";
}

std::string XMLDocument::text(void* node) {
    if (!node) return "";
    auto* elem = static_cast<tinyxml2::XMLElement*>(node);
    const char* txt = elem->GetText();
    return txt ? std::string(txt) : "";
}

std::string XMLDocument::nodeName(void* node) {
    if (!node) return "";
    auto* elem = static_cast<tinyxml2::XMLElement*>(node);
    return elem->Name() ? std::string(elem->Name()) : "";
}

void* XMLDocument::firstChildElement(void* node, const std::string& name) {
    if (!node) return nullptr;
    auto* elem = static_cast<tinyxml2::XMLElement*>(node);
    if (name.empty()) {
        return elem->FirstChildElement();
    }
    return child(node, name);
}

void* XMLDocument::nextSiblingElement(void* node, const std::string& name) {
    if (!node) return nullptr;
    auto* elem = static_cast<tinyxml2::XMLElement*>(node);
    if (name.empty()) {
        return elem->NextSiblingElement();
    }
    auto* sibling = elem->NextSiblingElement(name.c_str());
    if (sibling) return sibling;
    sibling = elem->NextSiblingElement();
    while (sibling) {
        if (nameMatches(sibling->Name(), name)) {
            return sibling;
        }
        sibling = sibling->NextSiblingElement();
    }
    return nullptr;
}

std::string XMLDocument::attributeNS(void* node, const std::string& uri, const std::string& localName) {
    (void)uri;
    return attribute(node, localName);
}

std::string XMLDocument::getNamespace(void* node, const std::string& prefix) {
    if (!node) return "";
    auto* elem = static_cast<tinyxml2::XMLElement*>(node);
    const char* uriStr = elem->Attribute(("xmlns:" + prefix).c_str());
    return uriStr ? std::string(uriStr) : "";
}

}
