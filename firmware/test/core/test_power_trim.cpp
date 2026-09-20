// A charger in constant voltage holds a known value, so the difference is this unit's error.
#include "core/events/sensor.h"
#include "core/power/battery.h"
#include "core/power/trim.h"
#include "doctest/doctest.h"

using namespace skyblip;
using namespace skyblip::power;

namespace {

constexpr uint32_t kSecondMs = 1000;

events::BatterySample sample(uint16_t millivolts, bool external_power) {
    return events::BatterySample{millivolts, external_power};
}

uint32_t feed(FloatTrim& trim, uint16_t millivolts, bool external_power, uint32_t from_ms,
              uint32_t span_ms) {
    uint32_t now = from_ms;
    for (; now <= from_ms + span_ms; now += kSecondMs)
        trim.apply(sample(millivolts, external_power), now);
    return now;
}

// A whole charge: constant current well under the float, then the float itself.
uint32_t charge(FloatTrim& trim, uint16_t float_mv, uint32_t from_ms) {
    const uint32_t climbed =
        feed(trim, static_cast<uint16_t>(float_mv - kClimbRiseMv), true, from_ms, 10 * kSecondMs);
    return feed(trim, float_mv, true, climbed, kPlateauHoldMs);
}

}  // namespace

TEST_CASE("trim: a unit reading low learns the millivolts it is out by") {
    FloatTrim trim;
    CHECK_FALSE(trim.learned());

    charge(trim, 4160, 0);
    CHECK(trim.learned());
    CHECK(int(trim.offset_mv()) == 40);

    FloatTrim high;
    charge(high, 4250, 0);
    CHECK(int(high.offset_mv()) == -50);
}

TEST_CASE("trim: the trim it learns is the one the settings bound accepts") {
    FloatTrim trim;
    charge(trim, kFloatReferenceMv - kCalibrationLimitMv, 0);
    CHECK(trim.learned());
    CHECK(int(trim.offset_mv()) == int(kCalibrationLimitMv));

    // Past the bound a reading is wired wrong rather than out of trim.
    FloatTrim broken;
    charge(broken, kFloatReferenceMv - kCalibrationLimitMv - 100, 0);
    CHECK_FALSE(broken.learned());
}

// A topped-off pack drifting down under its own load is flat too, and is no reference.
TEST_CASE("trim: a plateau it did not climb into teaches it nothing") {
    FloatTrim trim;
    feed(trim, 4180, true, 0, 4 * kPlateauHoldMs);
    CHECK_FALSE(trim.learned());
}

TEST_CASE("trim: no cable, no reference") {
    FloatTrim trim;
    const uint32_t climbed = feed(trim, 3990, true, 0, 10 * kSecondMs);
    feed(trim, 4190, false, climbed, 4 * kPlateauHoldMs);
    CHECK_FALSE(trim.learned());
}

TEST_CASE("trim: unplugging forgets the climb") {
    FloatTrim trim;
    const uint32_t climbed = feed(trim, 3970, true, 0, 10 * kSecondMs);
    const uint32_t unplugged = feed(trim, 3970, false, climbed, 2 * kSecondMs);
    feed(trim, 4170, true, unplugged, 2 * kPlateauHoldMs);
    CHECK_FALSE(trim.learned());
}

TEST_CASE("trim: a reading that is still moving is not a plateau") {
    FloatTrim trim;
    uint32_t now = feed(trim, 3900, true, 0, 10 * kSecondMs);
    for (uint16_t mv = 4100; mv <= 4200; mv++) {
        trim.apply(sample(mv, true), now);
        now += kSecondMs;
    }
    CHECK_FALSE(trim.learned());

    feed(trim, 4200, true, now, kPlateauHoldMs);
    CHECK(trim.learned());
    // A window opened on the way up is worth half the spread it is allowed.
    CHECK(int(trim.offset_mv()) <= int(kPlateauSpreadMv) / 2);
    CHECK(int(trim.offset_mv()) >= -int(kPlateauSpreadMv) / 2);
}

TEST_CASE("trim: two minutes of plateau, and not a second less") {
    FloatTrim trim;
    const uint32_t climbed = feed(trim, 3950, true, 0, 10 * kSecondMs);
    feed(trim, 4150, true, climbed, kPlateauHoldMs - 10 * kSecondMs);
    CHECK_FALSE(trim.learned());

    feed(trim, 4150, true, climbed, kPlateauHoldMs);
    CHECK(trim.learned());
    CHECK(int(trim.offset_mv()) == 50);
}
