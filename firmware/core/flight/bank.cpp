#include "core/flight/bank.h"

#include <algorithm>

#include "core/util/intmath.h"

namespace skyblip::flight {

namespace {
constexpr int32_t kOne = 16384;
constexpr int32_t kTurn = 65536;
constexpr int32_t kCentiDegPerTurn = 36000;
constexpr int32_t kMilliGPerMetrePerSecondPerCentiDegNum = 178;
constexpr int32_t kMilliGPerMetrePerSecondPerCentiDegDen = 10000;

int16_t to_c16(int32_t cdeg) { return static_cast<int16_t>((cdeg * kTurn) / kCentiDegPerTurn); }

int32_t to_cdeg(int16_t c16) { return (static_cast<int32_t>(c16) * kCentiDegPerTurn) / kTurn; }
}  // namespace

int32_t centripetal_mg(int32_t speed_mps, int32_t turn_cdps) {
    return (speed_mps * turn_cdps * kMilliGPerMetrePerSecondPerCentiDegNum) /
           kMilliGPerMetrePerSecondPerCentiDegDen;
}

void BankAngle::observe(const BodyRate& rate, const SpecificForce& force, int32_t speed_mps,
                        int32_t turn_cdps, uint32_t at_ms) {
    const uint32_t dt_ms = seen_ && at_ms > last_ms_ ? at_ms - last_ms_ : 0;
    if (dt_ms > 0 && dt_ms <= kBankStepCapMs)
        bank_cdeg_ +=
            div_round(static_cast<int32_t>(rate.roll_cdps) * static_cast<int32_t>(dt_ms), 1000);

    const int16_t held = to_c16(bank_cdeg_);
    const int32_t lateral_mg = centripetal_mg(speed_mps, turn_cdps);
    const int32_t right_mg = force.right_mg - (lateral_mg * icos(held)) / kOne;
    const int32_t up_mg = force.up_mg - (lateral_mg * isin(held)) / kOne;

    if (right_mg != 0 || up_mg != 0) {
        const int32_t measured_cdeg = to_cdeg(iatan2(-right_mg, up_mg));
        bank_cdeg_ =
            seen_ ? bank_cdeg_ + (measured_cdeg - bank_cdeg_) / kBankSamples : measured_cdeg;
        seen_ = true;
    }

    bank_cdeg_ = std::clamp(bank_cdeg_, -kBankLimitCdeg, kBankLimitCdeg);
    last_ms_ = at_ms;
}

bool BankAngle::valid(uint32_t now_ms) const { return seen_ && now_ms - last_ms_ < kBankStaleMs; }

int16_t BankAngle::deg() const {
    const int32_t half = bank_cdeg_ < 0 ? -50 : 50;
    return static_cast<int16_t>((bank_cdeg_ + half) / 100);
}

}  // namespace skyblip::flight
