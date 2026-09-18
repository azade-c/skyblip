// ADS-L 4 SRD-860 issue 2 G.2 to G.5 and B.5: the payloads issue 2 added, and what this firmware
// does with each.
#include <cstdint>

#include "core/model/aircraft.h"
#include "core/protocol/adsl_uplink.h"
#include "core/timing/slot.h"
#include "doctest/doctest.h"

using namespace skyblip;

namespace {
model::AircraftObs target(uint32_t addr) {
    model::AircraftObs t{};
    t.addr = addr;
    t.addr_table = 6;
    t.aircraft_cat = 1;
    t.flight_state = 2;
    t.lat_1e7 = 481234500;
    t.lon_1e7 = 81234500;
    t.alt_m = 1200;
    t.speed_q = 160;
    t.speed_valid = true;
    t.position_valid = true;
    return t;
}
}  // namespace

// TODO: fc 18sep26 no Status payload is transmitted, so G.2.1 to G.2.11 have nothing to encode
TEST_CASE("ADS-L.4.SRD860.G.2: a Status payload declares this device's bands, systems and version" *
          doctest::skip()) {
    FAIL("the Status payload is not implemented: no type 3 is ever transmitted");
}

// TODO: fc 18sep26 soft versioning needs G.2's Max Protocol Version, which is neither sent nor
// tracked
TEST_CASE("ADS-L.4.SRD860.B.5: the version transmitted is one every close neighbour can read" *
          doctest::skip()) {
    FAIL(
        "no peer's Max Protocol Version is tracked, so nothing lowers the version for a neighbour");
}

// The uplink this firmware carries is the ground station's own, on the channel and in the slot G.3
// names.
TEST_CASE(
    "ADS-L.4.SRD860.G.3: the Traffic Uplink rides the O-band HDR channel in the Uplink slot") {
    CHECK(timing::kObandHz == 869525000u);
    CHECK(protocol::kUplinkChipRateBps == 200000u);
    CHECK(timing::kUplinkRxStart >= 200);
    CHECK(timing::kUplinkRxEnd <= 450);
    // A whole burst fits the slot it is transmitted in, with the dwell's guard either side.
    CHECK(timing::kGroundEmitStart + static_cast<int>(protocol::kUplinkBurstUs / 1000) <=
          timing::kGroundEmitEnd);
}

// A ground station is one transmitter reaching every aircraft in range, so what it relays is
// checked, not trusted.
TEST_CASE("ADS-L.4.SRD860.G.3: a relayed target that is not an aircraft is refused, not drawn") {
    protocol::AdslUplink uplink;
    model::AircraftObs targets[4] = {target(0x300001), target(0), target(0x300003),
                                     target(0x300004)};
    targets[3].lat_1e7 = 1800000000;

    uint8_t frame[protocol::AdslUplink::kFrameBytes] = {0};
    REQUIRE(uplink.encode(targets, 4, 0, frame) == Status::Ok);

    model::AircraftObs out[4]{};
    protocol::AdslUplink::DecodeStats stats{};
    REQUIRE(uplink.decode(frame, out, 4, stats) == Status::Ok);
    CHECK(stats.targets == 2);
    CHECK(stats.rejected == 2);
    for (int i = 0; i < stats.targets; i++) CHECK(out[i].source == model::Source::AdslUplink);
}

// TODO: fc 18sep26 the uplink frame here is a house format: 16-byte records, no verbatim Traffic
// payload
TEST_CASE(
    "ADS-L.4.SRD860.G.3: an uplink record is a 30-bit address and the Traffic payload, "
    "verbatim" *
    doctest::skip()) {
    const int record_bits = 30 + 2 + 120;
    CHECK(record_bits == 152);
    CHECK(protocol::AdslUplink::kRecordBytes * 8 == record_bits);
    CHECK(protocol::AdslUplink::kMaxTargets == 10);
}

// TODO: fc 18sep26 an uplink record carries no timestamp, so its age cannot be checked at all
TEST_CASE("ADS-L.4.SRD860.G.3: a relayed Traffic payload older than 10 seconds is not forwarded" *
          doctest::skip()) {
    FAIL("no timestamp travels with a relayed target, so nothing here can refuse a stale one");
}

// TODO: fc 18sep26 no FIS-B: DO-358B ground uplink messages are neither received nor decoded
TEST_CASE("ADS-L.4.SRD860.G.4: a FIS-B Uplink payload carries a DO-358B ground uplink message" *
          doctest::skip()) {
    FAIL("the FIS-B payload is not implemented: type 5 is dropped with every other unknown type");
}

// TODO: fc 18sep26 no Remote Identification: EN 4709-002 is a drone payload this aircraft device
// never sends
TEST_CASE("ADS-L.4.SRD860.G.5: a Remote Identification payload carries an EN 4709-002 block" *
          doctest::skip()) {
    FAIL("the Remote Identification payload is not implemented: type 6 is dropped as unknown");
}
