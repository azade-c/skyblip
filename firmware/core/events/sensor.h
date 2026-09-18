#ifndef SKYBLIP_CORE_EVENTS_SENSOR_H
#define SKYBLIP_CORE_EVENTS_SENSOR_H

#include <cstdint>

namespace skyblip::events {
struct BaroSample {
    uint32_t pressure_mpa;
    uint32_t at_ms;
};

struct AccelSample {
    int16_t right_mg;
    int16_t up_mg;
    int16_t aft_mg;
    uint32_t at_ms;
};

// Body rates in hundredths of a degree a second, named the way a pilot names
// them: roll is right wing down, pitch is nose up, yaw is nose right.
struct RateSample {
    int16_t roll_cdps;
    int16_t pitch_cdps;
    int16_t yaw_cdps;
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
