#include "core/flight/rate.h"

#include "core/util/intmath.h"

namespace skyblip::flight {

bool vertical_rate_cdps(const BodyRate& rate, const SpecificForce& force, int16_t& out_cdps) {
    const int64_t square = static_cast<int64_t>(force.right_mg) * force.right_mg +
                           static_cast<int64_t>(force.up_mg) * force.up_mg +
                           static_cast<int64_t>(force.aft_mg) * force.aft_mg;
    const int32_t resultant_mg =
        static_cast<int32_t>(isqrt<uint64_t>(static_cast<uint64_t>(square)));
    if (resultant_mg < kRateFloorMg) return false;

    const int64_t along_down = static_cast<int64_t>(rate.roll_cdps) * force.aft_mg -
                               static_cast<int64_t>(rate.pitch_cdps) * force.right_mg +
                               static_cast<int64_t>(rate.yaw_cdps) * force.up_mg;
    out_cdps = static_cast<int16_t>(along_down / resultant_mg);
    return true;
}

void TurnRate::observe(const BodyRate& rate, const SpecificForce& force, uint32_t at_ms) {
    int16_t vertical_cdps = 0;
    if (!vertical_rate_cdps(rate, force, vertical_cdps)) return;
    measured_cdps_ = vertical_cdps;
    last_ms_ = at_ms;
    seen_ = true;
}

void TurnRate::trim_to(int16_t reference_cdps) {
    if (!seen_) return;
    const int32_t error = measured_cdps_ - reference_cdps - trim_cdps();
    trim_acc_ += error;
}

bool TurnRate::valid(uint32_t now_ms) const { return seen_ && now_ms - last_ms_ < kRateStaleMs; }

int16_t TurnRate::cdps() const { return static_cast<int16_t>(measured_cdps_ - trim_cdps()); }

int16_t TurnRate::trim_cdps() const {
    const int32_t half = trim_acc_ < 0 ? -kTrimSamples / 2 : kTrimSamples / 2;
    return static_cast<int16_t>((trim_acc_ + half) / kTrimSamples);
}

}  // namespace skyblip::flight
