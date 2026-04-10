#include "citygml/parser/xml_document.h"
#include "tinyxml2.h"
#include <vector>
#include <string>
#include <cstring>

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
    return getLocalName(nodeName) == targetName;
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
    // 直接遍历所有子元素，按 local name 精确匹配，避免迭代器状态错位
    auto* child = elem->FirstChildElement();
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
    // 始终遍历所有子元素，按 local name 精确匹配
    for (auto* child = elem->FirstChildElement(); child != nullptr; child = child->NextSiblingElement()) {
        if (nameMatches(child->Name(), name)) {
            result.push_back(child);
        }
    }
    return result;
}

std::string XMLDocument::attribute(void* node, const std::string& name) {
    if (!node) return "";
    auto* elem = static_cast<tinyxml2::XMLElement*>(node);
    // 优先直接查找
    const char* attr = elem->Attribute(name.c_str());
    if (attr) return std::string(attr);
    // 回退：遍历所有属性，按 local name 匹配（支持 xlink:href 等命名空间前缀）
    for (auto* a = elem->FirstAttribute(); a != nullptr; a = a->Next()) {
        if (nameMatches(a->Name(), name)) {
            return std::string(a->Value());
        }
    }
    return "";
}

std::string XMLDocument::text(void* node) {
    if (!node) return "";
    auto* elem = static_cast<tinyxml2::XMLElement*>(node);
    // 优先直接获取文本节点（处理 <gen:value>纯文本</gen:value>）
    const char* txt = elem->GetText();
    if (txt && strlen(txt) > 0) {
        return std::string(txt);
    }
    // 递归收集所有直接子 Text 节点
    std::string result;
    for (auto* child = elem->FirstChild(); child; child = child->NextSibling()) {
        if (child->ToText()) {
            result += child->ToText()->Value();
        }
    }
    if (!result.empty()) return result;
    // 处理 <gen:value xlink:href="..."/> 这种只有属性没有文本的情况
    // 尝试 xlink:href（CityGML 标准方式）
    const char* href = elem->Attribute("xlink:href");
    if (!href) href = elem->Attribute("xlink_href"); // TinyXML2 将冒号转为下划线
    if (!href) href = elem->Attribute("href");
    if (href && strlen(href) > 0) {
        return std::string(href);
    }
    // 遍历所有属性，查找任意名为 href 或 *:href 的属性
    for (auto* a = elem->FirstAttribute(); a != nullptr; a = a->Next()) {
        std::string attrName = a->Name();
        if (attrName == "href" || attrName == "xlink:href" || attrName == "xlink_href" ||
            attrName.find(":href") != std::string::npos) {
            if (a->Value() && strlen(a->Value()) > 0) {
                return std::string(a->Value());
            }
        }
    }
    return "";
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
    // TinyXML2 的 FirstChildElement(name) 是状态迭代器，
    // 会影响后续 FirstChildElement() 的起始位置，导致遍历错位。
    // 因此直接遍历所有子元素，按 local name 精确匹配。
    auto* child = elem->FirstChildElement();
    while (child) {
        if (nameMatches(child->Name(), name)) {
            return child;
        }
        child = child->NextSiblingElement();
    }
    return nullptr;
}

void* XMLDocument::nextSiblingElement(void* node, const std::string& name) {
    if (!node) return nullptr;
    auto* elem = static_cast<tinyxml2::XMLElement*>(node);
    if (name.empty()) {
        return elem->NextSiblingElement();
    }
    // 始终遍历兄弟元素，按 local name 精确匹配（不依赖 TinyXML2 的命名过滤迭代器）
    auto* sibling = elem->NextSiblingElement();
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
