// Two projected paths and the volume neither may enter: the one geometry the alarm grades and the glass draws.
#include "core/traffic/conflict.h"

#include "doctest/doctest.h"

using namespace skyblip;
using namespace skyblip::traffic;
using skyblip::flight::Motion;

namespace {

constexpr uint16_t kCruiseSpeedQ = 120;
constexpr uint16_t kThermalSpeedQ = 93;

uint16_t c9(int deg) { return static_cast<uint16_t>(((deg % 360 + 360) % 360) * 512 / 360); }

Motion at(int32_t north_m, int32_t east_m, int32_t up_m, int track_deg, uint16_t speed_q,
          int16_t turn_dps = 0) {
    Motion m{};
    m.north_m = north_m;
    m.east_m = east_m;
    m.up_m = up_m;
    m.track_c9 = c9(track_deg);
    m.speed_q = speed_q;
    m.turn_dps = turn_dps;
    m.turning = true;
    return m;
}

}  // namespace

TEST_CASE("conflict: the volume grows with how far ahead the question is asked, and stops") {
    CHECK(protection_radius_m(0) == kProtectionRadiusM);
    CHECK(protection_vert_m(0) == kProtectionVertM);
    CHECK(protection_radius_m(60) > protection_radius_m(0));
    CHECK(protection_vert_m(60) > protection_vert_m(0));
    // Uncapped growth turns every neighbour holding station into a conflict.
    CHECK(protection_radius_m(kHorizonS) == kProtectionRadiusM + kMaxSpreadM);
    CHECK(protection_vert_m(kHorizonS) == kProtectionVertM + kMaxVertSpreadM);
    CHECK(protection_radius_m(600) == protection_radius_m(kHorizonS));
}

TEST_CASE("conflict: a head-on breaches at the time the arithmetic says") {
    // 1800 m apart, 30 m/s each, so the volume is entered around 28 s.
    const Conflict c =
        first_breach(at(0, 0, 0, 0, kCruiseSpeedQ), at(1800, 0, 0, 180, kCruiseSpeedQ));
    CHECK(c.breaches);
    CHECK(c.at_s >= 24);
    CHECK(c.at_s <= 30);
    CHECK(c.closest_m < kProtectionRadiusM);
}

TEST_CASE("conflict: a target running away never breaches") {
    const Conflict c = first_breach(at(0, 0, 0, 0, kCruiseSpeedQ), at(400, 0, 0, 0, kCruiseSpeedQ));
    CHECK(!c.breaches);
    CHECK(c.closest_m >= 390);
}

TEST_CASE("conflict: aircraft holding station never breach, however close they sit") {
    const Conflict c =
        first_breach(at(0, 0, 0, 90, kCruiseSpeedQ), at(0, 300, 0, 90, kCruiseSpeedQ));
    CHECK(!c.breaches);
}

TEST_CASE("conflict: vertical separation keeps a crossing out of the volume") {
    const Conflict below =
        first_breach(at(0, 0, 0, 0, kCruiseSpeedQ), at(900, 0, -400, 180, kCruiseSpeedQ));
    CHECK(!below.breaches);
    const Conflict level =
        first_breach(at(0, 0, 0, 0, kCruiseSpeedQ), at(900, 0, 0, 180, kCruiseSpeedQ));
    CHECK(level.breaches);
}

TEST_CASE("conflict: two aircraft on one circle never breach, offset circles do") {
    // Both thermalling right at 12 deg/s: a 111 m radius, a 30 s circle.
    const Motion own = at(0, 0, 0, 0, kThermalSpeedQ, 12);
    const Motion abreast = at(0, 222, 0, 180, kThermalSpeedQ, 12);
    CHECK(!first_breach(own, abreast).breaches);

    // Cores 75 m apart, 34 degrees out of phase: what a turn-rate match used to silence.
    const Motion offset = at(75, 180, 0, 214, kThermalSpeedQ, 12);
    CHECK(first_breach(own, offset).breaches);
}

TEST_CASE("conflict: a turning target is judged on its arc, not on its tangent") {
    // Passing 600 m right at 36 m/s: straight it clears, on a 2 deg/s arc it arrives.
    const Motion own = at(0, 0, 0, 0, kCruiseSpeedQ);
    const Motion straight = at(1800, 600, 0, 180, 144, 0);
    const Motion turning = at(1800, 600, 0, 180, 144, 2);
    CHECK(!first_breach(own, straight).breaches);
    const Conflict c = first_breach(own, turning);
    CHECK(c.breaches);
    CHECK(c.at_s >= 24);
    CHECK(c.at_s <= 40);
}

TEST_CASE("conflict: the closest approach is reported even when nothing breaches") {
    const Conflict c =
        first_breach(at(0, 0, 0, 0, kCruiseSpeedQ), at(2000, 600, 0, 180, kCruiseSpeedQ));
    CHECK(!c.breaches);
    CHECK(c.closest_m > 0);
    CHECK(c.closest_at_s > 0);
    CHECK(c.closest_at_s <= kHorizonS);
}
