#ifndef SKYBLIP_CORE_FLIGHT_GLOAD_H
#define SKYBLIP_CORE_FLIGHT_GLOAD_H

#include <cstdint>

#include "core/flight/rate.h"

namespace skyblip::flight {

constexpr uint32_t kGLoadStaleMs = 2000;
constexpr int16_t kLevelFlightMg = 1000;

struct GLoad {
    int16_t normal_mg{0};
    int16_t lateral_mg{0};
    int16_t longitudinal_mg{0};
};

class GMeter {
   public:
    void observe(const SpecificForce& force, uint32_t at_ms);
    void reset();
    bool valid(uint32_t now_ms) const;

    const GLoad& now() const { return now_; }
    const GLoad& most() const { return most_; }
    const GLoad& least() const { return least_; }

   private:
    GLoad now_{};
    GLoad most_{kLevelFlightMg, 0, 0};
    GLoad least_{kLevelFlightMg, 0, 0};
    uint32_t last_ms_{0};
    bool seen_{false};
};

}  // namespace skyblip::flight

#endif
