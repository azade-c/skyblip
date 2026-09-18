#ifndef SKYBLIP_CORE_FLIGHT_SLIP_H
#define SKYBLIP_CORE_FLIGHT_SLIP_H

#include <cstdint>

namespace skyblip::flight {

constexpr int32_t kSlipFloorMg = 200;
constexpr int32_t kSlipDampingSamples = 8;
constexpr uint32_t kSlipStaleMs = 2000;

bool slip_from_specific_force(int32_t right_mg, int32_t up_mg, int32_t aft_mg, int16_t& out_mg);

class SlipBall {
   public:
    void update(int32_t right_mg, int32_t up_mg, int32_t aft_mg, uint32_t at_ms);
    bool valid(uint32_t now_ms) const;
    int16_t mg() const;

   private:
    int32_t damped_acc_{0};
    uint32_t last_ms_{0};
    bool seen_{false};
};

}  // namespace skyblip::flight

#endif
