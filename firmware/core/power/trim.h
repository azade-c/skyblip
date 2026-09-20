#ifndef SKYBLIP_CORE_POWER_TRIM_H
#define SKYBLIP_CORE_POWER_TRIM_H

#include <cstdint>

#include "core/events/sensor.h"
#include "core/power/battery.h"

namespace skyblip::power {

constexpr uint16_t kFloatReferenceMv = kFullMv;
// INFO: fc 20sep26 a plateau this far over the session's low is the charger's, not the cell's
constexpr uint16_t kClimbRiseMv = 200;
constexpr uint16_t kPlateauSpreadMv = 20;
constexpr uint32_t kPlateauHoldMs = 120000;

static_assert(kClimbRiseMv > kPlateauSpreadMv,
              "a climb no bigger than the plateau's own noise is not a climb");

class FloatTrim {
   public:
    void apply(const events::BatterySample& untrimmed, uint32_t now_ms);

    bool learned() const { return learned_; }
    int16_t offset_mv() const { return offset_mv_; }

   private:
    void open_window(uint16_t millivolts, uint32_t now_ms);

    uint32_t since_ms_{0};
    uint32_t sum_mv_{0};
    uint16_t samples_{0};
    uint16_t low_mv_{0};
    uint16_t high_mv_{0};
    uint16_t session_low_mv_{0};
    int16_t offset_mv_{0};
    bool cabled_{false};
    bool holding_{false};
    bool learned_{false};
};

}  // namespace skyblip::power

#endif
