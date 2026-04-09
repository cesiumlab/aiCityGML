#include "citygml/core/citygml_geometry.h"

namespace citygml {

const char* toString(GeometryType type) {
    switch (type) {
        case GeometryType::GT_Point: return "Point";
        case GeometryType::GT_MultiPoint: return "MultiPoint";
        case GeometryType::GT_Curve: return "Curve";
        case GeometryType::GT_LineString: return "LineString";
        case GeometryType::GT_MultiCurve: return "MultiCurve";
        case GeometryType::GT_Surface: return "Surface";
        case GeometryType::GT_Polygon: return "Polygon";
        case GeometryType::GT_MultiSurface: return "MultiSurface";
        case GeometryType::GT_CompositeSurface: return "CompositeSurface";
        case GeometryType::GT_Solid: return "Solid";
        case GeometryType::GT_MultiSolid: return "MultiSolid";
        case GeometryType::GT_CompositeSolid: return "CompositeSolid";
        case GeometryType::GT_Implicit: return "ImplicitGeometry";
        default: return "Unknown";
    }
}

} // namespace citygml