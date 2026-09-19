#ifndef SKYBLIP_HARDWARE_MODEL_BME280_H
#define SKYBLIP_HARDWARE_MODEL_BME280_H

#include "core/flight/atmosphere.h"

namespace skyblip::models {

class Bme280 {
   public:
    void set_altitude_mm(int32_t mm) { pressure_mpa_ = flight::alt_mm_to_pressure_mpa(mm); }

    void set_pressure_mpa(uint32_t mpa) { pressure_mpa_ = mpa; }

    uint32_t pressure_mpa() const { return pressure_mpa_; }

    void set_temperature_decicelsius(int16_t deci) { temperature_decicelsius_ = deci; }

    int16_t temperature_decicelsius() const { return temperature_decicelsius_; }

   private:
    uint32_t pressure_mpa_{flight::kIsaSeaLevelPa * 1000};
    int16_t temperature_decicelsius_{flight::kIsaSeaLevelDeciCelsius};
};

}  // namespace skyblip::models

#endif
