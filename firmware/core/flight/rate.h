#ifndef SKYBLIP_CORE_FLIGHT_RATE_H
#define SKYBLIP_CORE_FLIGHT_RATE_H

#include <cstdint>

namespace skyblip::flight {

constexpr int32_t kRateFloorMg = 200;
constexpr uint32_t kRateStaleMs = 2000;
constexpr int32_t kTrimSamples = 64;

struct BodyRate {
    int16_t roll_cdps{0};
    int16_t pitch_cdps{0};
    int16_t yaw_cdps{0};
};

struct SpecificForce {
    int16_t right_mg{0};
    int16_t up_mg{0};
    int16_t aft_mg{0};
};

bool vertical_rate_cdps(const BodyRate& rate, const SpecificForce& force, int16_t& out_cdps);

class TurnRate {
   public:
    void observe(const BodyRate& rate, const SpecificForce& force, uint32_t at_ms);
    void trim_to(int16_t reference_cdps);
    bool valid(uint32_t now_ms) const;
    int16_t cdps() const;
    int16_t trim_cdps() const;

   private:
    int32_t measured_cdps_{0};
    int32_t trim_acc_{0};
    uint32_t last_ms_{0};
    bool seen_{false};
};

}  // namespace skyblip::flight

#endif
