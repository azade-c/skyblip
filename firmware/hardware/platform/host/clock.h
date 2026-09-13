#ifndef SKYBLIP_HARDWARE_PLATFORM_HOST_CLOCK_H
#define SKYBLIP_HARDWARE_PLATFORM_HOST_CLOCK_H

#include "hal/clock.h"

namespace skyblip::platform::host {

class Clock : public hal::Clock {
   public:
    uint32_t millis() const override { return static_cast<uint32_t>(us_ / 1000); }
    uint64_t micros() const override { return us_; }
    // INFO: fc 13sep26 millis() wraps and micros() does not (hal/clock.h), so a wrap is a step
    void set_millis(uint32_t ms) {
        const uint64_t elapsed_ms = us_ / 1000;
        us_ += static_cast<uint64_t>(ms - static_cast<uint32_t>(elapsed_ms)) * 1000;
    }
    void set_micros(uint64_t us) { us_ = us; }
    void advance(uint32_t ms) { us_ += static_cast<uint64_t>(ms) * 1000; }
    void advance_us(uint64_t us) { us_ += us; }

   private:
    uint64_t us_{0};
};

}

#endif
