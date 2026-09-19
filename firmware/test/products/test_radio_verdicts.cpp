// Three facts the tape spelled DEC alike, told apart through the whole product.
#include <cstring>

#include "core/events/rf.h"
#include "core/model/aircraft.h"
#include "core/model/band.h"
#include "core/model/ownship.h"
#include "core/protocol/air.h"
#include "core/protocol/alptas.h"
#include "core/radio/log.h"
#include "doctest/doctest.h"
#include "hardware/parts/sx1262/model.h"
#include "test/support/product_rig.h"

using namespace skyblip;

namespace {

model::AircraftObs neighbour(const model::OwnState& own) {
    model::AircraftObs obs{};
    obs.addr = 0xC5D804;
    obs.addr_table = 6;
    obs.aircraft_cat = 4;
    obs.flight_state = 2;
    obs.lat_1e7 = own.lat_1e7 + 10000;
    obs.lon_1e7 = own.lon_1e7;
    obs.alt_m = 900;
    obs.speed_q = 160;
    obs.track_c9 = 256;
    obs.speed_valid = true;
    obs.position_valid = true;
    return obs;
}

// A neighbour two metres away, as the chip hands one over: past the shared sync window.
void hear_alptas(Rig& rig, const uint8_t* frame) {
    uint8_t chips[protocol::kTxChipBytes];
    const size_t chip_len = protocol::encode_mband(protocol::kAlptasSyncWord, frame,
                                                   protocol::kAlptasFrameBytes, chips);

    events::RfEvent event{};
    event.type = events::RfEventType::RxDone;
    event.band = model::Band::M;
    event.rssi_dbm = -15;
    event.rssi_valid = true;
    event.len = models::Sx1262::deliver_after_sync(chips, static_cast<uint16_t>(chip_len),
                                                   protocol::kSharedSync, protocol::kSharedSyncBits,
                                                   event.data.data(), protocol::kRxChipBytes);
    rig.product.bus().rf.push(event);
}

// Own-ship's own bursts are the other half of the tape, and a case about receptions ignores them.
radio::Event heard_verdict(Rig& rig) {
    const radio::Log& log = rig.state().radio_log;
    for (int i = 0; i < log.count(); i++) {
        const radio::Event event = log.newest(i).event;
        if (event != radio::Event::Transmitted && event != radio::Event::Lost &&
            event != radio::Event::Held && event != radio::Event::Unarmed)
            return event;
    }
    return radio::Event::Transmitted;
}

void fly(Rig& rig, uint32_t& t, uint32_t seconds) { rig.seconds(t, seconds, 25000, 900); }

void settle(Rig& rig, uint32_t& t) {
    rig.run(t, t + 200);
    t += 250;
}

}  // namespace

TEST_CASE("radio verdicts: a burst heard before own-ship has a fix is a wait, not a failure") {
    Rig rig;
    REQUIRE(rig.setup() == Status::Ok);
    uint32_t t = 0;
    settle(rig, t);
    REQUIRE_FALSE(rig.state().own.fix_valid);

    model::AircraftObs obs = neighbour(rig.state().own);
    obs.lat_1e7 = 481234567;
    obs.lon_1e7 = 87654321;
    uint8_t frame[protocol::kAlptasFrameBytes];
    REQUIRE(protocol::alptas_encode(frame, obs, Rig::kUtcBase, 481000000, 87000000) == Status::Ok);

    hear_alptas(rig, frame);
    settle(rig, t);

    CHECK(heard_verdict(rig) == radio::Event::Unattempted);
}

// SoftRF built with USE_INTERLEAVING sends Air V6 between its V7 frames, and we read only V7.
TEST_CASE("radio verdicts: a message type this firmware does not implement says so") {
    Rig rig;
    REQUIRE(rig.setup() == Status::Ok);
    uint32_t t = 0;
    fly(rig, t, 3);

    const model::OwnState& own = rig.state().own;
    uint8_t frame[protocol::kAlptasFrameBytes];
    REQUIRE(protocol::alptas_encode(frame, neighbour(own), own.utc, own.lat_1e7, own.lon_1e7) ==
            Status::Ok);
    // The message type is the low nibble of byte 3, in clear, before any decrypt stage.
    frame[3] = static_cast<uint8_t>(frame[3] & 0xF0);
    protocol::alptas_set_crc(frame);

    hear_alptas(rig, frame);
    settle(rig, t);

    CHECK(heard_verdict(rig) == radio::Event::Unsupported);
}

TEST_CASE("radio verdicts: a frame the plausibility gate refuses is still a decode failure") {
    Rig rig;
    REQUIRE(rig.setup() == Status::Ok);
    uint32_t t = 0;
    fly(rig, t, 3);

    const model::OwnState& own = rig.state().own;
    uint8_t frame[protocol::kAlptasFrameBytes];
    REQUIRE(protocol::alptas_encode(frame, neighbour(own), own.utc + 100, own.lat_1e7,
                                    own.lon_1e7) == Status::Ok);

    hear_alptas(rig, frame);
    settle(rig, t);

    CHECK(heard_verdict(rig) == radio::Event::Undecoded);
}

TEST_CASE("radio verdicts: the frame we can read is read, and names its aircraft") {
    Rig rig;
    REQUIRE(rig.setup() == Status::Ok);
    uint32_t t = 0;
    fly(rig, t, 3);

    const model::OwnState& own = rig.state().own;
    uint8_t frame[protocol::kAlptasFrameBytes];
    REQUIRE(protocol::alptas_encode(frame, neighbour(own), own.utc, own.lat_1e7, own.lon_1e7) ==
            Status::Ok);

    hear_alptas(rig, frame);
    settle(rig, t);

    CHECK(heard_verdict(rig) == radio::Event::Received);
}
