// What a pilot-admitted formation does to the annunciator, and the one thing it cannot silence.
#include "core/traffic/formation.h"
#include "core/util/intmath.h"
#include "doctest/doctest.h"
#include "test/support/screen_rig.h"

using namespace skyblip;

namespace {

uint16_t c9(int deg) { return static_cast<uint16_t>(((deg % 360 + 360) % 360) * 512 / 360); }

model::AircraftObs contact(const model::OwnState& own, int north_m, int east_m, int up_m, int mps,
                           int track_deg, uint32_t at_ms) {
    model::AircraftObs t{};
    t.addr = 0x515151;
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
    t.source = model::Source::AdslDirect;
    return t;
}

struct Flight {
    Rig rig;

    Flight() {
        rig.state.own.fix_valid = true;
        rig.state.own.lat_1e7 = 481000000;
        rig.state.own.lon_1e7 = 81000000;
        rig.state.own.alt_m = 1000;
        rig.state.own.speed_q = 40 * 4;
        rig.state.own.track_c9 = c9(90);
    }

    void hear(int north_m, int east_m, int up_m, int mps, int track_deg, uint32_t now_ms) {
        rig.state.own.fix_ms = now_ms;
        const model::AircraftObs obs =
            contact(rig.state.own, north_m, east_m, up_m, mps, track_deg, now_ms);
        rig.state.traffic.update(obs, now_ms / 1000);
        rig.alarm_service.tick(now_ms);
    }

    const traffic::Target* target() {
        const int at = rig.state.traffic.find(6, 0x515151);
        return at < 0 ? nullptr : rig.state.traffic.at(at);
    }

    uint32_t hold_station(uint32_t from_ms) {
        uint32_t t = from_ms;
        for (; t <= from_ms + formation::kSteadyMs + 2000; t += 1000)
            hear(-60, -120, 10, 40, 90, t);
        return t;
    }
};

}  // namespace

TEST_CASE("formation: a neighbour holding station is offered, and admitting it takes the level") {
    Flight flight;
    const uint32_t after = flight.hold_station(1000);

    CHECK(flight.rig.state.formation.offered);
    CHECK(flight.rig.state.formation.offer_clock == 5);
    CHECK(flight.rig.state.alarm_level == traffic::Level::Info);

    flight.rig.alarm_service.admit_formation();
    CHECK(flight.rig.alarm_service.formation_members() == 1);

    flight.hear(-60, -120, 10, 40, 90, after);
    CHECK(flight.target()->in_formation);
    CHECK(flight.rig.state.alarm_level == traffic::Level::None);
    CHECK_FALSE(flight.rig.state.formation.offered);
}

TEST_CASE("formation: a member on a collision course alarms, and stops being a member") {
    Flight flight;
    const uint32_t after = flight.hold_station(1000);
    flight.rig.alarm_service.admit_formation();
    flight.hear(-60, -120, 10, 40, 90, after);
    REQUIRE(flight.target()->in_formation);
    REQUIRE(flight.rig.state.alarm_level == traffic::Level::None);

    // The same aircraft, now 400 m off the nose coming the other way.
    flight.hear(0, 400, 0, 40, 270, after + 1000);
    CHECK(flight.rig.state.alarm_level == traffic::Level::Urgent);
    CHECK_FALSE(flight.target()->in_formation);
    CHECK(flight.rig.alarm_service.formation_members() == 0);
}

TEST_CASE("formation: a member that leaves says so once") {
    Flight flight;
    const uint32_t after = flight.hold_station(1000);
    flight.rig.alarm_service.admit_formation();
    flight.hear(-60, -120, 10, 40, 90, after);
    CHECK_FALSE(flight.rig.state.formation.split);

    flight.hear(-60, -400, 10, 40, 90, after + 1000);
    CHECK_FALSE(flight.rig.state.formation.split);
    flight.hear(-60, -700, 10, 40, 90, after + 2000);
    CHECK(flight.rig.state.formation.split);
    CHECK(flight.rig.alarm_service.formation_members() == 0);
}

TEST_CASE("formation: refusing the offer leaves the contact as traffic") {
    Flight flight;
    const uint32_t after = flight.hold_station(1000);
    REQUIRE(flight.rig.state.formation.offered);

    flight.rig.alarm_service.release_formation();
    CHECK_FALSE(flight.rig.state.formation.offered);
    CHECK(flight.rig.alarm_service.formation_members() == 0);

    flight.hear(-60, -120, 10, 40, 90, after);
    CHECK_FALSE(flight.target()->in_formation);
    CHECK(flight.rig.state.alarm_level == traffic::Level::Info);
}
