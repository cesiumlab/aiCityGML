#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

namespace citygml {

class XMLUtils {
public:
    XMLUtils() = default;
    ~XMLUtils() = default;
    
    static std::string trim(const std::string& str);
    static std::string replace(const std::string& str, const std::string& from, const std::string& to);
    static std::vector<std::string> split(const std::string& str, char delim);
    static std::string normalizeCoordinateString(const std::string& coords);
};

} // namespace citygml