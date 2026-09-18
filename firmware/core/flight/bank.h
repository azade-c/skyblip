#ifndef SKYBLIP_CORE_FLIGHT_BANK_H
#define SKYBLIP_CORE_FLIGHT_BANK_H

#include <cstdint>

#include "core/flight/rate.h"

namespace skyblip::flight {

constexpr uint32_t kBankStaleMs = 2000;
constexpr int32_t kBankSamples = 8;
constexpr int32_t kBankLimitCdeg = 9000;
constexpr uint32_t kBankStepCapMs = 1000;

int32_t centripetal_mg(int32_t speed_mps, int32_t turn_cdps);

class BankAngle {
   public:
    void observe(const BodyRate& rate, const SpecificForce& force, int32_t speed_mps,
                 int32_t turn_cdps, uint32_t at_ms);
    bool valid(uint32_t now_ms) const;
    int16_t cdeg() const { return static_cast<int16_t>(bank_cdeg_); }
    int16_t deg() const;

   private:
    int32_t bank_cdeg_{0};
    uint32_t last_ms_{0};
    bool seen_{false};
};

}  // namespace skyblip::flight

#endif
