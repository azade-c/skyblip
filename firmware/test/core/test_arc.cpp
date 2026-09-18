// Where an aircraft will be if it keeps doing what it is doing: the one path the radar draws and the alarm measures against.
#include "core/flight/arc.h"

#include "core/util/intmath.h"
#include "doctest/doctest.h"

using namespace skyblip;
using namespace skyblip::flight;

namespace {

constexpr uint32_t kStepMs = 1000;
constexpr uint16_t kThermalSpeedQ = 93;

uint16_t c9(int deg) { return static_cast<uint16_t>(((deg % 360 + 360) % 360) * 512 / 360); }

Motion flying(int track_deg, uint16_t speed_q, int16_t turn_dps) {
    Motion m{};
    m.speed_q = speed_q;
    m.track_c9 = c9(track_deg);
    m.turn_dps = turn_dps;
    m.turning = true;
    return m;
}

Position after(Motion m, int seconds, uint32_t step_ms = kStepMs) {
    Arc arc(m, step_ms);
    Position p = arc.here();
    for (uint32_t elapsed = 0; elapsed < static_cast<uint32_t>(seconds) * 1000u; elapsed += step_ms)
        p = arc.advance();
    return p;
}

}  // namespace

TEST_CASE("arc: an aircraft not turning flies a straight ray") {
    const Position p = after(flying(0, 120, 0), 60);
    CHECK(p.north_m == 1800);
    CHECK(p.east_m == 0);

    const Position east = after(flying(90, 120, 0), 60);
    CHECK(east.east_m == 1800);
    CHECK(east.north_m == 0);
}

TEST_CASE("arc: a turn flies the radius its rate and speed describe") {
    // 23.15 m/s at 12 deg/s is radius = speed / rate = 111 m, so 221 m across at the half turn.
    const Position half = after(flying(0, kThermalSpeedQ, 12), 15);
    const int32_t diameter = static_cast<int32_t>(idistance(half.north_m, half.east_m));
    CHECK(diameter > 213);
    CHECK(diameter < 229);
    CHECK(half.east_m > 213);
    CHECK(half.north_m < 8);
    CHECK(half.north_m > -8);
}

TEST_CASE("arc: a full circle comes back to where it started") {
    const Position full = after(flying(0, kThermalSpeedQ, 12), 30);
    CHECK(idistance(full.north_m, full.east_m) < 8);
}

TEST_CASE("arc: a left turn mirrors a right turn") {
    const Position right = after(flying(0, kThermalSpeedQ, 13), 7);
    const Position left = after(flying(0, kThermalSpeedQ, -13), 7);
    CHECK(right.north_m == left.north_m);
    CHECK(right.east_m == -left.east_m);
}

TEST_CASE("arc: the step is a cadence, not a shape") {
    const Motion m = flying(30, kThermalSpeedQ, 9);
    const Position coarse = after(m, 20, 2000);
    const Position fine = after(m, 20, 500);
    CHECK(idistance(coarse.north_m - fine.north_m, coarse.east_m - fine.east_m) < 10);
}

TEST_CASE("arc: a climb is carried with the path") {
    Motion m = flying(0, 120, 0);
    m.climb_e8 = 8 * 3;
    m.climbing = true;
    CHECK(after(m, 10).up_m == 30);
    m.climbing = false;
    CHECK(after(m, 10).up_m == 0);
}

TEST_CASE("arc: a turn rate no aeroplane holds is clamped, not believed") {
    CHECK(clamped_turn_dps(90) == kMaxTurnDps);
    CHECK(clamped_turn_dps(-90) == -kMaxTurnDps);
    CHECK(clamped_turn_dps(13) == 13);

    const Position wild = after(flying(0, kThermalSpeedQ, 200), 20);
    const Position clamped = after(flying(0, kThermalSpeedQ, kMaxTurnDps), 20);
    CHECK(wild.north_m == clamped.north_m);
    CHECK(wild.east_m == clamped.east_m);
}

TEST_CASE("arc: a target with no velocity of its own does not fly anywhere") {
    model::AircraftObs obs{};
    obs.speed_valid = false;
    obs.track_c9 = c9(180);
    const Position p = after(motion_of(obs, 0, false), 60);
    CHECK(p.north_m == 0);
    CHECK(p.east_m == 0);
}
