#include <iostream>
#include <iomanip>
#include <string>
#include "parser/citygml_parser.h"

using namespace citygml;

struct TestCase {
    std::string fileName;
    std::string expectedSrsName;
};

int main() {
    TestCase tests[] = {
        {"testdata/2.gml",                   "EPSG:4547"},
        {"testdata/FZK-Haus.gml",            "urn:adv:crs:ETRS89_UTM32*DE_DHHN92_NH"},
        {"testdata/FZK-Haus-LoD0-KIT-IAI-KHH-B36-V1.gml", "urn:adv:crs:ETRS89_UTM32*DE_DHHN92_NH"},
        {"testdata/FZK-Haus-LoD1-KIT-IAI-KHH-B36-V1.gml", "urn:adv:crs:ETRS89_UTM32*DE_DHHN92_NH"},
        {"testdata/FZK-Haus-LoD2-KIT-IAI-KHH-B36-V1.gml", "urn:adv:crs:ETRS89_UTM32*DE_DHHN92_NH"},
        {"testdata/FZK-Haus-LoD3-KIT-IAI-KHH-B36-V1.gml", "urn:adv:crs:ETRS89_UTM32*DE_DHHN92_NH"},
        {"testdata/GeoBIM_BuildingsLOD3.gml", "EPSG:28992"},
        {"testdata/samplycity_bldg001.gml",  "urn:ogc:def:crs,crs:EPSG::25832,crs:EPSG::5783"},
        {"testdata/test_generic_attrs.gml",   "EPSG:4326"},
    };

    int passed = 0;
    int failed = 0;

    std::cout << "============================================================\n";
    std::cout << "         Testing quickGetFirstSrsName\n";
    std::cout << "============================================================\n\n";

    for (const auto& test : tests) {
        std::string result = CityGMLParser::quickGetFirstSrsName(test.fileName);

        bool ok = (result == test.expectedSrsName);
        if (ok) passed++; else failed++;

        std::cout << "File: " << test.fileName << "\n";
        std::cout << "  Expected: " << test.expectedSrsName << "\n";
        std::cout << "  Got:      " << (result.empty() ? "(empty)" : result) << "\n";
        std::cout << "  Result:   " << (ok ? "[PASS]" : "[FAIL]") << "\n";
        std::cout << "\n";
    }

    std::cout << "============================================================\n";
    std::cout << "  Total: " << (passed + failed) << "  |  Passed: " << passed << "  |  Failed: " << failed << "\n";
    std::cout << "============================================================\n";

    return failed > 0 ? 1 : 0;
}
