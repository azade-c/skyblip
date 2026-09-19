#include "core/flight/gload.h"

#include <algorithm>

namespace skyblip::flight {

namespace {
void stretch(int16_t sample, int16_t& most, int16_t& least) {
    most = std::max(most, sample);
    least = std::min(least, sample);
}
}  // namespace

void GMeter::observe(const SpecificForce& force, uint32_t at_ms) {
    now_.normal_mg = force.up_mg;
    now_.lateral_mg = static_cast<int16_t>(-force.right_mg);
    now_.longitudinal_mg = static_cast<int16_t>(-force.aft_mg);

    if (!seen_) {
        most_ = now_;
        least_ = now_;
    }
    stretch(now_.normal_mg, most_.normal_mg, least_.normal_mg);
    stretch(now_.lateral_mg, most_.lateral_mg, least_.lateral_mg);
    stretch(now_.longitudinal_mg, most_.longitudinal_mg, least_.longitudinal_mg);

    last_ms_ = at_ms;
    seen_ = true;
}

void GMeter::reset() {
    most_ = now_;
    least_ = now_;
}

bool GMeter::valid(uint32_t now_ms) const { return seen_ && now_ms - last_ms_ < kGLoadStaleMs; }

}  // namespace skyblip::flight
