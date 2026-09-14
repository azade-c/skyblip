#ifndef SKYBLIP_HARDWARE_PLATFORM_ZEPHYR_BARO_H
#define SKYBLIP_HARDWARE_PLATFORM_ZEPHYR_BARO_H
#if defined(__ZEPHYR__)

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#include "core/util/result.h"

namespace skyblip::platform::zephyr {

class Baro {
   public:
    explicit Baro(const struct device* dev) : dev_(dev) {}

    bool ready() const { return device_is_ready(dev_); }

    bool read_pressure_mpa(uint32_t& out_mpa) {
        if (sensor_sample_fetch(dev_) != 0) return false;
        struct sensor_value press{};
        if (sensor_channel_get(dev_, SENSOR_CHAN_PRESS, &press) != 0) return false;
        const int64_t mpa = millipascals(press);
        if (mpa < kMinPlausibleMpa || mpa > kMaxPlausibleMpa) return false;
        out_mpa = static_cast<uint32_t>(mpa);
        return true;
    }

   private:
    static constexpr int64_t kMilliPaPerPa = 1000;
    static constexpr int64_t kMilliPaPerKiloPa = 1000 * kMilliPaPerPa;
    static constexpr int64_t kMinPlausibleMpa = 1000 * kMilliPaPerPa;
    static constexpr int64_t kMaxPlausibleMpa = 200000 * kMilliPaPerPa;

    // INFO: fc 14sep26 sensor_value.val2 is micro-kPa, which is the millipascal itself
    static int64_t millipascals(const struct sensor_value& kpa) {
        return static_cast<int64_t>(kpa.val1) * kMilliPaPerKiloPa + kpa.val2;
    }

    const struct device* dev_;
};

}  // namespace skyblip::platform::zephyr
#endif  // __ZEPHYR__
#endif
