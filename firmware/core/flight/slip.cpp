#include "core/flight/slip.h"

#include "core/util/intmath.h"

namespace skyblip::flight {

bool slip_from_specific_force(int32_t right_mg, int32_t up_mg, int32_t aft_mg, int16_t& out_mg) {
    const int64_t square = static_cast<int64_t>(right_mg) * right_mg +
                           static_cast<int64_t>(up_mg) * up_mg +
                           static_cast<int64_t>(aft_mg) * aft_mg;
    const int32_t resultant_mg =
        static_cast<int32_t>(isqrt<uint64_t>(static_cast<uint64_t>(square)));
    if (resultant_mg < kSlipFloorMg) return false;
    out_mg = static_cast<int16_t>(-right_mg * 1000 / resultant_mg);
    return true;
}

void SlipBall::update(int32_t right_mg, int32_t up_mg, int32_t aft_mg, uint32_t at_ms) {
    int16_t sample_mg = 0;
    if (!slip_from_specific_force(right_mg, up_mg, aft_mg, sample_mg)) return;

    const int32_t sampled = sample_mg * kSlipDampingSamples;
    damped_acc_ = seen_ ? damped_acc_ + (sampled - damped_acc_) / kSlipDampingSamples : sampled;
    last_ms_ = at_ms;
    seen_ = true;
}

bool SlipBall::valid(uint32_t now_ms) const { return seen_ && now_ms - last_ms_ < kSlipStaleMs; }

int16_t SlipBall::mg() const {
    const int32_t half = damped_acc_ < 0 ? -kSlipDampingSamples / 2 : kSlipDampingSamples / 2;
    return static_cast<int16_t>((damped_acc_ + half) / kSlipDampingSamples);
}

}  // namespace skyblip::flight
