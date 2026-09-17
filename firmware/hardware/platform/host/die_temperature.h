#ifndef SKYBLIP_HARDWARE_PLATFORM_HOST_DIE_TEMPERATURE_H
#define SKYBLIP_HARDWARE_PLATFORM_HOST_DIE_TEMPERATURE_H

#include <cstdint>

#include "ports/die_temperature.h"

namespace skyblip::platform::host {

class DieTemperature : public ports::DieTemperature {
   public:
    bool read(int16_t& decicelsius) override {
        reads_++;
        if (!answers_) return false;
        decicelsius = value_;
        return true;
    }

    void hold(int16_t decicelsius) {
        value_ = decicelsius;
        answers_ = true;
    }
    void refuse() { answers_ = false; }
    int reads() const { return reads_; }

   private:
    int reads_{0};
    int16_t value_{0};
    bool answers_{true};
};

}  // namespace skyblip::platform::host

#endif
