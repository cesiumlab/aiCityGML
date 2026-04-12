#include "core/citygml_base.h"

namespace citygml {

RelativeToTerrain parseRelativeToTerrain(const std::string& str) {
    if (str == "entirelyAboveTerrain") return RelativeToTerrain::EntirelyAboveTerrain;
    if (str == "substantiallyAboveTerrain") return RelativeToTerrain::SubstantiallyAboveTerrain;
    if (str == "substantiallyAboveAndBelowTerrain") return RelativeToTerrain::SubstantiallyAboveAndBelowTerrain;
    if (str == "substantiallyBelowTerrain") return RelativeToTerrain::SubstantiallyBelowTerrain;
    if (str == "entirelyBelowTerrain") return RelativeToTerrain::EntirelyBelowTerrain;
    return RelativeToTerrain::EntirelyAboveTerrain;
}

RelativeToWater parseRelativeToWater(const std::string& str) {
    if (str == "entirelyAboveWaterSurface") return RelativeToWater::EntirelyAboveWaterSurface;
    if (str == "substantiallyAboveWaterSurface") return RelativeToWater::SubstantiallyAboveWaterSurface;
    if (str == "substantiallyAboveAndBelowWaterSurface") return RelativeToWater::SubstantiallyAboveAndBelowWaterSurface;
    if (str == "substantiallyBelowWaterSurface") return RelativeToWater::SubstantiallyBelowWaterSurface;
    if (str == "entirelyBelowWaterSurface") return RelativeToWater::EntirelyBelowWaterSurface;
    return RelativeToWater::EntirelyAboveWaterSurface;
}

SpaceType parseSpaceType(const std::string& str) {
    if (str == "Closed" || str == "closed") return SpaceType::Closed;
    if (str == "Open" || str == "open") return SpaceType::Open;
    if (str == "SemiOpen" || str == "semiOpen") return SpaceType::SemiOpen;
    return SpaceType::Closed;
}

} // namespace citygml