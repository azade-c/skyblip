#ifndef SKYBLIP_CORE_EVENTS_SENSOR_H
#define SKYBLIP_CORE_EVENTS_SENSOR_H

#include <cstdint>

namespace skyblip::events {
struct BaroSample {
    uint32_t pressure_mpa;
    uint32_t at_ms;
};

// The cell's terminal voltage, and whether something is feeding the charger.
// What that pair means is core/power's problem, not the board's.
struct BatterySample {
    uint16_t millivolts;
    bool external_power;
};

}  // namespace skyblip::events

#endif
