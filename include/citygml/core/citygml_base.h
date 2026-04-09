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

// ============================================================
// 枚举类型
// ============================================================

// LOD 级别
enum class LODLevel : int {
    LOD0 = 0,
    LOD1 = 1,
    LOD2 = 2,
    LOD3 = 3,
    LOD4 = 4
};

// 相对地形位置
enum class RelativeToTerrain {
    EntirelyAboveTerrain,
    SubstantiallyAboveTerrain,
    SubstantiallyAboveAndBelowTerrain,
    SubstantiallyBelowTerrain,
    EntirelyBelowTerrain
};

// 相对水面位置
enum class RelativeToWater {
    EntirelyAboveWaterSurface,
    SubstantiallyAboveWaterSurface,
    SubstantiallyAboveAndBelowWaterSurface,
    SubstantiallyBelowWaterSurface,
    EntirelyBelowWaterSurface,
    TemporarilyAboveAndBelowWaterSurface
};

// 空间类型
enum class SpaceType {
    Closed,
    Open,
    SemiOpen
};

// 几何类型
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

// ============================================================
// GML 标准类型
// ============================================================

// 日期时间
struct DateTime {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    double second = 0.0;
};

// 包络盒
struct Envelope {
    std::string id;
    std::string srsName;
    std::array<double, 3> lowerCorner;
    std::array<double, 3> upperCorner;
    
    Envelope() : lowerCorner({0, 0, 0}), upperCorner({0, 0, 0}) {}
    
    std::array<double, 3> center() const {
        return {
            (lowerCorner[0] + upperCorner[0]) * 0.5,
            (lowerCorner[1] + upperCorner[1]) * 0.5,
            (lowerCorner[2] + upperCorner[2]) * 0.5
        };
    }
    
    std::array<double, 3> size() const {
        return {
            upperCorner[0] - lowerCorner[0],
            upperCorner[1] - lowerCorner[1],
            upperCorner[2] - lowerCorner[2]
        };
    }
};

// 代码类型
struct Code {
    std::string value;
    std::optional<std::string> codeSpace;
};

// 变换矩阵
using TransformationMatrix4x4 = std::array<double, 16>;

// ADE 组件
class ADEComponent {
public:
    virtual ~ADEComponent() = default;
};

// ============================================================
// 枚举转换函数
// ============================================================

RelativeToTerrain parseRelativeToTerrain(const std::string& str);
RelativeToWater parseRelativeToWater(const std::string& str);
SpaceType parseSpaceType(const std::string& str);

} // namespace citygml