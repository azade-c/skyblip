#include "core/traffic/sanity.h"

#include "core/model/aircraft.h"
#include "core/model/ownship.h"
#include "core/protocol/nmea_out.h"
#include "core/util/intmath.h"

namespace skyblip::traffic {

int32_t plausible_range_m(model::Source source) {
    return source == model::Source::AdslUplink ? kMaxRelayedRangeM : kMaxPlausibleRangeM;
}

Plausibility range_check(const model::OwnState& own, const model::AircraftObs& obs,
                         int32_t& slant_m) {
    int32_t north_m = 0, east_m = 0, up_m = 0;
    if (!protocol::relative_ned(own, obs, north_m, east_m, up_m)) return Plausibility::NoReference;

    // INFO: fc 19sep26 slant, not ground: a bad altitude field throws a target straight up
    const int32_t ground_m = static_cast<int32_t>(idistance(north_m, east_m));
    slant_m = static_cast<int32_t>(idistance(ground_m, up_m));
    return slant_m > plausible_range_m(obs.source) ? Plausibility::TooFar
                                                   : Plausibility::Believable;
}

}  // namespace skyblip::traffic
