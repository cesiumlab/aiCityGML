#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <array>
#include <optional>
#include <variant>
#include <functional>

namespace citygml {

// LOD Level
enum class LODLevel : int {
    LOD0 = 0,
    LOD1 = 1,
    LOD2 = 2,
    LOD3 = 3,
    LOD4 = 4
};

// Relative to Terrain
enum class RelativeToTerrain {
    EntirelyAboveTerrain,
    SubstantiallyAboveTerrain,
    SubstantiallyAboveAndBelowTerrain,
    SubstantiallyBelowTerrain,
    EntirelyBelowTerrain
};

// Relative to Water
enum class RelativeToWater {
    EntirelyAboveWaterSurface,
    SubstantiallyAboveWaterSurface,
    SubstantiallyAboveAndBelowWaterSurface,
    SubstantiallyBelowWaterSurface,
    EntirelyBelowWaterSurface,
    TemporarilyAboveAndBelowWaterSurface
};

// Space Type
enum class SpaceType {
    Closed,
    Open,
    SemiOpen
};

// Geometry Type
enum class GeometryType {
    GT_Unknown,
    GT_Point,
    GT_MultiPoint,
    GT_Curve,
    GT_LineString,
    GT_MultiCurve,
    GT_Surface,
    GT_Polygon,
    GT_MultiSurface,
    GT_CompositeSurface,
    GT_Solid,
    GT_MultiSolid,
    GT_CompositeSolid,
    GT_Implicit
};

// DateTime
struct DateTime {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    double second = 0.0;
};

// Envelope
struct Envelope {
    std::string id;
    std::string srsName;
    std::array<double, 3> lowerCorner;
    std::array<double, 3> upperCorner;
    
    Envelope() : lowerCorner({0, 0, 0}), upperCorner({0, 0, 0}) {}
    
    Envelope(double lx, double ly, double lz, double ux, double uy, double uz)
        : lowerCorner({lx, ly, lz}), upperCorner({ux, uy, uz}) {}
    
    std::array<double, 3> getCenter() const {
        return {
            (lowerCorner[0] + upperCorner[0]) * 0.5,
            (lowerCorner[1] + upperCorner[1]) * 0.5,
            (lowerCorner[2] + upperCorner[2]) * 0.5
        };
    }
    
    std::array<double, 3> getSize() const {
        return {
            upperCorner[0] - lowerCorner[0],
            upperCorner[1] - lowerCorner[1],
            upperCorner[2] - lowerCorner[2]
        };
    }
    
    static std::shared_ptr<Envelope> create(double lx, double ly, double lz, double ux, double uy, double uz, const std::string& srs = "") {
        auto env = std::make_shared<Envelope>(lx, ly, lz, ux, uy, uz);
        env->srsName = srs;
        return env;
    }
};

// Code
struct Code {
    std::string value;
    std::optional<std::string> codeSpace;
};

// Transformation Matrix
using TransformationMatrix4x4 = std::array<double, 16>;

// ADE Component
class ADEComponent {
public:
    virtual ~ADEComponent() = default;
};

RelativeToTerrain parseRelativeToTerrain(const std::string& str);
RelativeToWater parseRelativeToWater(const std::string& str);
SpaceType parseSpaceType(const std::string& str);

} // namespace citygml