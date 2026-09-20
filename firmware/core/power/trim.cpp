#include "core/power/trim.h"

#include <algorithm>

namespace skyblip::power {

namespace {

bool near_the_float(uint16_t millivolts) {
    return millivolts + kCalibrationLimitMv >= kFloatReferenceMv &&
           millivolts <= kFloatReferenceMv + kCalibrationLimitMv;
}

}  // namespace

void FloatTrim::open_window(uint16_t millivolts, uint32_t now_ms) {
    holding_ = true;
    since_ms_ = now_ms;
    sum_mv_ = millivolts;
    samples_ = 1;
    low_mv_ = millivolts;
    high_mv_ = millivolts;
}

void FloatTrim::apply(const events::BatterySample& untrimmed, uint32_t now_ms) {
    if (!untrimmed.external_power) {
        cabled_ = false;
        holding_ = false;
        return;
    }

    if (!cabled_ || untrimmed.millivolts < session_low_mv_) {
        cabled_ = true;
        session_low_mv_ = untrimmed.millivolts;
        holding_ = false;
        return;
    }

    const bool climbed = untrimmed.millivolts - session_low_mv_ >= kClimbRiseMv;
    if (!climbed || !near_the_float(untrimmed.millivolts)) {
        holding_ = false;
        return;
    }

    if (!holding_) {
        open_window(untrimmed.millivolts, now_ms);
        return;
    }

    low_mv_ = std::min(untrimmed.millivolts, low_mv_);
    high_mv_ = std::max(untrimmed.millivolts, high_mv_);
    if (high_mv_ - low_mv_ > kPlateauSpreadMv) {
        open_window(untrimmed.millivolts, now_ms);
        return;
    }

    sum_mv_ += untrimmed.millivolts;
    samples_++;
    if (now_ms - since_ms_ < kPlateauHoldMs) return;

    const uint16_t held_mv = static_cast<uint16_t>((sum_mv_ + samples_ / 2) / samples_);
    offset_mv_ = static_cast<int16_t>(kFloatReferenceMv - static_cast<int32_t>(held_mv));
    learned_ = true;
    open_window(untrimmed.millivolts, now_ms);
}

}  // namespace skyblip::power
