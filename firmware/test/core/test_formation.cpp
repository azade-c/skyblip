// Aircraft flying together, told from geometry alone: no flying style is named, and nothing here
// quietens an alarm.
#include <cmath>

#include "core/traffic/formation.h"
#include "core/util/intmath.h"
#include "doctest/doctest.h"

using namespace skyblip;
using namespace skyblip::formation;

namespace {

uint16_t c9(int deg) { return static_cast<uint16_t>(((deg % 360 + 360) % 360) * 512 / 360); }

model::OwnState flying(int mps, int track_deg, uint32_t at_ms = 0) {
    model::OwnState o{};
    o.fix_valid = true;
    o.fix_ms = at_ms;
    o.lat_1e7 = 481000000;
    o.lon_1e7 = 81000000;
    o.alt_m = 1000;
    o.speed_q = static_cast<uint16_t>(mps * 4);
    o.track_c9 = c9(track_deg);
    return o;
}

model::AircraftObs neighbour(const model::OwnState& own, int north_m, int east_m, int up_m, int mps,
                             int track_deg, uint32_t at_ms) {
    model::AircraftObs t{};
    t.addr = 0x424242;
    t.addr_table = 6;
    t.position_valid = true;
    t.speed_valid = true;
    t.speed_q = static_cast<uint16_t>(mps * 4);
    t.track_c9 = c9(track_deg);
    t.alt_m = own.alt_m + up_m;
    t.lat_1e7 = own.lat_1e7 + static_cast<int32_t>(static_cast<int64_t>(north_m) * 1000000 / 11132);
    const int16_t ang =
        static_cast<int16_t>((static_cast<int64_t>(own.lat_1e7) * 65536) / 3600000000LL);
    const int64_t east_scaled = (static_cast<int64_t>(east_m) << 14) / icos(ang);
    t.lon_1e7 = own.lon_1e7 + static_cast<int32_t>(east_scaled * 1000000 / 11132);
    t.received.at_s = at_ms / 1000;
    t.at_ms = at_ms;
    return t;
}

}  // namespace

TEST_CASE("formation: a wingman holding station is offered after the steady window") {
    Tracker tracker;
    Report r{};
    for (uint32_t t = 1000; t <= 1000 + kSteadyMs + 1000; t += 1000) {
        const model::OwnState own = flying(40, 90, t);
        r = tracker.observe(own, neighbour(own, -60, -120, 10, 40, 90, t), t);
    }
    CHECK(r.state == State::Candidate);
    CHECK_FALSE(tracker.together(6, 0x424242));
    CHECK(tracker.members() == 0);

    tracker.admit(6, 0x424242);
    CHECK(tracker.together(6, 0x424242));
    CHECK(tracker.members() == 1);
}

TEST_CASE("formation: a contact crossing our path is never offered") {
    Tracker tracker;
    Report r{};
    for (int i = 0; i <= 10; i++) {
        const uint32_t t = 1000 + static_cast<uint32_t>(i) * 1000;
        const model::OwnState own = flying(40, 0, t);
        r = tracker.observe(own, neighbour(own, 600, 400 - 80 * i, 0, 40, 270, t), t);
    }
    CHECK(r.state == State::None);
}

TEST_CASE("formation: a member leaving is reported once, and it takes two fixes") {
    Tracker tracker;
    for (uint32_t t = 1000; t <= 1000 + kSteadyMs + 1000; t += 1000) {
        const model::OwnState own = flying(40, 90, t);
        tracker.observe(own, neighbour(own, -60, -120, 10, 40, 90, t), t);
    }
    tracker.admit(6, 0x424242);

    uint32_t t = 1000 + kSteadyMs + 2000;
    model::OwnState own = flying(40, 90, t);
    // One bad solution does not break a formation.
    CHECK(tracker.observe(own, neighbour(own, -60, -400, 10, 40, 90, t), t).state ==
          State::Together);
    CHECK(tracker.together(6, 0x424242));

    t += 1000;
    own = flying(40, 90, t);
    CHECK(tracker.observe(own, neighbour(own, -60, -700, 10, 40, 90, t), t).state == State::Broken);
    CHECK_FALSE(tracker.together(6, 0x424242));
    CHECK(tracker.members() == 0);

    t += 1000;
    own = flying(40, 90, t);
    CHECK(tracker.observe(own, neighbour(own, -60, -700, 10, 40, 90, t), t).state != State::Broken);
}

TEST_CASE("formation: a member flying out of range breaks, whatever it does next") {
    Tracker tracker;
    for (uint32_t t = 1000; t <= 1000 + kSteadyMs + 1000; t += 1000) {
        const model::OwnState own = flying(40, 90, t);
        tracker.observe(own, neighbour(own, -60, -120, 10, 40, 90, t), t);
    }
    tracker.admit(6, 0x424242);

    const uint32_t t = 1000 + kSteadyMs + 2000;
    const model::OwnState own = flying(40, 90, t);
    CHECK(tracker.observe(own, neighbour(own, -60, -1600, 10, 40, 90, t), t).state ==
          State::Broken);
    CHECK_FALSE(tracker.together(6, 0x424242));
}

TEST_CASE("formation: two gliders on one thermal circle are flying together") {
    Tracker tracker;
    Report r{};
    const int16_t turn = 12;
    for (int i = 0; i <= 8; i++) {
        const uint32_t t = 1000 + static_cast<uint32_t>(i) * 1000;
        const int track_deg = turn * i;
        model::OwnState own = flying(23, track_deg, t);
        own.turn_dps = turn;
        // Diametrically opposite on one circle: 222 m off the right wing, always.
        const double heading = track_deg * 3.14159265358979 / 180.0;
        const int north_m = static_cast<int>(-222.0 * std::sin(heading));
        const int east_m = static_cast<int>(222.0 * std::cos(heading));
        r = tracker.observe(own, neighbour(own, north_m, east_m, 0, 23, track_deg + 180, t), t);
    }
    CHECK(r.state == State::Candidate);
}

TEST_CASE("formation: a member is placed by the clock, and the hours run the right way") {
    CHECK(clock_of(1000, 0) == 12);
    CHECK(clock_of(0, 1000) == 3);
    CHECK(clock_of(-1000, 0) == 6);
    CHECK(clock_of(0, -1000) == 9);
    CHECK(clock_of(500, 500) == 2);
    CHECK(clock_of(-500, -500) == 8);
}

TEST_CASE("formation: a contact nobody has heard from is forgotten, membership and all") {
    Tracker tracker;
    for (uint32_t t = 1000; t <= 1000 + kSteadyMs + 1000; t += 1000) {
        const model::OwnState own = flying(40, 90, t);
        tracker.observe(own, neighbour(own, -60, -120, 10, 40, 90, t), t);
    }
    tracker.admit(6, 0x424242);
    CHECK(tracker.members() == 1);

    tracker.forget_stale(1000 + kSteadyMs + 1000 + kForgetMs + 1);
    CHECK(tracker.members() == 0);
    CHECK_FALSE(tracker.together(6, 0x424242));
}
