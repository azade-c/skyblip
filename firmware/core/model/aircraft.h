#ifndef SKYBLIP_CORE_MODEL_AIRCRAFT_H
#define SKYBLIP_CORE_MODEL_AIRCRAFT_H

#include <cstdint>

#include "core/events/stamp.h"

namespace skyblip::model {
enum class Source : uint8_t { AdslDirect, AdslUplink, Alptas, Own };

constexpr char source_letter(Source source) {
    switch (source) {
        case Source::AdslDirect: return 'A';
        case Source::AdslUplink: return 'U';
        case Source::Alptas: return 'F';
        case Source::Own: return 'O';
    }
    return '?';
}

struct AircraftObs {
    uint32_t addr;
    uint8_t addr_table;
    uint8_t aircraft_cat;
    uint8_t flight_state;
    uint8_t emergency;
    int32_t lat_1e7;
    int32_t lon_1e7;
    int32_t alt_m;
    int16_t climb_e8;
    uint16_t speed_q;
    uint16_t track_c9;
    events::Stamp received;
    // INFO: fc 13sep26 the instant this position was true, on ports::Clock, for the geometry to
    // align
    uint32_t at_ms;
    int8_t rssi_dbm;
    Source source;
    bool climb_valid;
    bool speed_valid;
    bool position_valid;
};

}  // namespace skyblip::model

#endif
