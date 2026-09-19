#ifndef SKYBLIP_CORE_TRAFFIC_RANGE_H
#define SKYBLIP_CORE_TRAFFIC_RANGE_H

#include <cstdint>

#include "core/model/aircraft.h"
#include "core/model/ownship.h"
#include "core/traffic/table.h"

namespace skyblip::traffic {

struct RangeRow {
    uint32_t addr{0};
    model::Source source{model::Source::AdslDirect};
    int32_t ground_m{0};
    int32_t up_m{0};
};

// INFO: fc 19sep26 ground distance, what the radar ring and the alarm mean by distance
bool range_to(const model::OwnState& own, const model::AircraftObs& obs, RangeRow& out);

// Nearest first, at most cap rows. Returns how many were filled.
int rank_by_range(const TrafficTable& table, const model::OwnState& own, RangeRow* out, int cap);

}  // namespace skyblip::traffic

#endif
