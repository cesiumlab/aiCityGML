#include "xml_utils.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace citygml {

std::string XMLUtils::trim(const std::string& str) {
    auto start = std::find_if(str.begin(), str.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    });
    auto end = std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base();
    
    if (start >= end) return "";
    return std::string(start, end);
}

std::string XMLUtils::replace(const std::string& str, const std::string& from, const std::string& to) {
    std::string result = str;
    size_t pos = 0;
    while ((pos = result.find(from, pos)) != std::string::npos) {
        result.replace(pos, from.length(), to);
        pos += to.length();
    }
    return result;
}

std::vector<std::string> XMLUtils::split(const std::string& str, char delim) {
    std::vector<std::string> result;
    std::istringstream iss(str);
    std::string token;
    while (std::getline(iss, token, delim)) {
        result.push_back(token);
    }
    return result;
}

std::string XMLUtils::normalizeCoordinateString(const std::string& coords) {
    std::string result = coords;
    result = replace(result, ",", ".");
    result = replace(result, ";", " ");
    return result;
}

} // namespace citygml