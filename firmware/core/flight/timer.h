#ifndef SKYBLIP_CORE_FLIGHT_TIMER_H
#define SKYBLIP_CORE_FLIGHT_TIMER_H

#include <cstdint>

#include "core/flight/state.h"

namespace skyblip::flight {

class FlightTimer {
   public:
    void update(FlightState state, uint32_t now_ms);

    bool flown() const { return flown_; }
    bool running() const { return running_; }
    uint32_t seconds() const { return seconds_; }

   private:
    uint32_t takeoff_ms_{0};
    uint32_t carried_s_{0};
    uint32_t seconds_{0};
    bool flown_{false};
    bool running_{false};
};

}  // namespace skyblip::flight

#endif
