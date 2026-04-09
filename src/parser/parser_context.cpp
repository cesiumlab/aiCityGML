#include "citygml/parser/parser_context.h"

namespace citygml {

ParserContext::ParserContext() = default;
ParserContext::~ParserContext() = default;

void ParserContext::pushNamespace(const std::string& prefix, const std::string& uri) {
    namespaces_[prefix] = uri;
}

std::optional<std::string> ParserContext::getNamespaceUri(const std::string& prefix) const {
    auto it = namespaces_.find(prefix);
    if (it != namespaces_.end()) {
        return it->second;
    }
    return std::nullopt;
}

} // namespace citygml