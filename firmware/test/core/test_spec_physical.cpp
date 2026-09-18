// ADS-L 4 SRD-860 issue 2 Subparts B and C, clause by clause, against what this firmware puts on
// air.
#include <cstdint>

#include "core/fec/manchester.h"
#include "core/model/ownship.h"
#include "core/protocol/adsl.h"
#include "core/protocol/adsl_uplink.h"
#include "core/protocol/air.h"
#include "core/timing/slot.h"
#include "core/timing/transmit.h"
#include "doctest/doctest.h"
#include "hardware/parts/sx1262/sx1262.h"

using namespace skyblip;

namespace {
uint16_t chips_of(uint8_t byte) {
    uint8_t coded[2] = {0, 0};
    fec::manchester_encode(&byte, 1, coded);
    return static_cast<uint16_t>((coded[0] << 8) | coded[1]);
}

protocol::AdslPacket traffic_packet() {
    model::OwnState own{};
    own.fix_valid = true;
    own.utc_valid = true;
    own.climb_valid = true;
    own.lat_1e7 = 481234500;
    own.lon_1e7 = 81234500;
    own.alt_m = 1234;
    own.speed_q = 180;
    own.climb_e8 = -44;
    own.track_c9 = 256;
    own.hdop_e2 = 90;
    own.vdop_e2 = 150;
    own.flight_state = 2;
    protocol::AdslPacket p{};
    protocol::from_own(p, own, 0x123456, 6, 4, false);
    return p;
}
}  // namespace

// The clause's own Example 1: 1328922 (0x14471A) leaves as 0x1A, 0x47, 0x14.
TEST_CASE("ADS-L.4.SRD860.B.3: an integer rides the air least significant byte first") {
    uint8_t three[3] = {0, 0, 0};
    protocol::AdslPacket::set3(three, 0x14471Au);
    CHECK(three[0] == 0x1A);
    CHECK(three[1] == 0x47);
    CHECK(three[2] == 0x14);
    CHECK(protocol::AdslPacket::get3(three) == 0x14471Au);

    uint8_t four[4] = {0, 0, 0, 0};
    protocol::AdslPacket::set4(four, 0x89ABCDEFu);
    CHECK(four[0] == 0xEF);
    CHECK(four[3] == 0x89);
    CHECK(protocol::AdslPacket::get4(four) == 0x89ABCDEFu);
}

// Example 2's packing rule, read off the Meta block G.1 puts at payload offset 0.
TEST_CASE("ADS-L.4.SRD860.B.3: a field starts at the bit its offset names, counting from bit 0") {
    protocol::AdslPacket p{};
    p.init();
    p.TimeStamp = 0x2A;
    p.FlightState = 2;
    p.AcftCat = 0x15;
    p.Emergency = 5;
    const uint8_t* meta = &p.Address[4];
    CHECK(meta[0] == static_cast<uint8_t>(0x2A | (2u << 6)));
    CHECK(meta[1] == static_cast<uint8_t>(0x15 | (5u << 5)));
}

TEST_CASE("ADS-L.4.SRD860.B.3: a byte goes out most significant bit first") {
    CHECK(chips_of(0x80) == 0x6AAA);
    CHECK(chips_of(0x01) == 0xAAA9);
    CHECK(chips_of(0x80) == protocol::manchester_chips(0x80));
}

// The preamble is the radio's, so what the firmware hands it runs from the sync word to the last
// packet byte.
TEST_CASE("ADS-L.4.SRD860.B.4: a transmission is the sync word and the packet, nothing behind it") {
    protocol::AdslPacket p = traffic_packet();
    p.scramble();
    p.set_crc();

    uint8_t chips[protocol::kTxChipBytes] = {0};
    const size_t len =
        protocol::encode_mband(protocol::kAdslSyncWord, p.Data, protocol::kAdslFrameBytes, chips);
    CHECK(len == 2u * (protocol::kSyncWordBytes + protocol::kAdslFrameBytes));

    uint8_t decoded[protocol::kSyncWordBytes + protocol::kAdslFrameBytes] = {0};
    uint8_t err[sizeof(decoded)] = {0};
    fec::manchester_decode(chips, sizeof(decoded), decoded, err);
    for (uint8_t e : err) CHECK(e == 0);
    CHECK(decoded[1] == 0x72);
    CHECK(decoded[2] == 0x4B);
    CHECK(decoded[3] == protocol::kAdslFrameBytes);
    for (int i = 0; i < protocol::kAdslFrameBytes; i++)
        CHECK(decoded[protocol::kSyncWordBytes + i] == p.Data[i]);
}

// Both reserved bits of a Traffic transmission are set: header offset 38, and the one closing the
// payload.
TEST_CASE("ADS-L.4.SRD860.B.6: a reserved bit a sender filled changes nothing a receiver reads") {
    protocol::AdslPacket clean = traffic_packet();
    protocol::AdslPacket noisy = clean;
    noisy.Address[3] |= 0x40;
    noisy.Reserved = 1;

    model::AircraftObs from_clean{};
    model::AircraftObs from_noisy{};
    REQUIRE(protocol::to_obs(clean, events::Stamp{}, -80, model::Source::AdslDirect, from_clean));
    REQUIRE(protocol::to_obs(noisy, events::Stamp{}, -80, model::Source::AdslDirect, from_noisy));

    CHECK(from_noisy.addr == from_clean.addr);
    CHECK(from_noisy.addr_table == from_clean.addr_table);
    CHECK(from_noisy.lat_1e7 == from_clean.lat_1e7);
    CHECK(from_noisy.lon_1e7 == from_clean.lon_1e7);
    CHECK(from_noisy.alt_m == from_clean.alt_m);
    CHECK(from_noisy.speed_q == from_clean.speed_q);
    CHECK(from_noisy.climb_e8 == from_clean.climb_e8);
    CHECK(from_noisy.track_c9 == from_clean.track_c9);
    CHECK(from_noisy.aircraft_cat == from_clean.aircraft_cat);
}

TEST_CASE("ADS-L.4.SRD860.C.2: the M band is modulated as the table prints it") {
    CHECK(timing::kMband0Hz == 868200000u);
    CHECK(timing::kMband1Hz == 868400000u);
    CHECK(protocol::kMbandChannelBandwidthHz == 200000u);
    CHECK(protocol::kMbandChipRateBps == 100000u);
    CHECK(protocol::kMbandDeviationHz == 50000u);
    CHECK(protocol::kMbandGaussianBtE2 == 50u);
    CHECK(parts::sx::kResultingErpCentiDb <= 1400);
}

TEST_CASE("ADS-L.4.SRD860.C.2.1: a one is sent as 01 and a zero as 10") {
    CHECK(chips_of(0xFF) == 0x5555);
    CHECK(chips_of(0x00) == 0xAAAA);
    CHECK(chips_of(0x72) == 0x95A6);

    uint8_t frame[protocol::kAdslFrameBytes] = {0};
    for (int i = 0; i < protocol::kAdslFrameBytes; i++) frame[i] = static_cast<uint8_t>(i * 7 + 3);
    uint8_t chips[2 * protocol::kAdslFrameBytes] = {0};
    fec::manchester_encode(frame, sizeof(frame), chips);
    uint8_t back[sizeof(frame)] = {0};
    uint8_t err[sizeof(frame)] = {0};
    fec::manchester_decode(chips, sizeof(frame), back, err);
    for (size_t i = 0; i < sizeof(frame); i++) {
        CHECK(back[i] == frame[i]);
        CHECK(err[i] == 0);
    }
}

// The radio sends eight repetitions; the last four and both 1001s are the leading sync byte,
// Manchester-coded.
TEST_CASE("ADS-L.4.SRD860.C.2.3: the preamble is at most twelve 01s and then two 1001s") {
    const uint8_t lead = static_cast<uint8_t>(protocol::kAdslSyncWord >> 24);
    CHECK(chips_of(lead) == 0x5599);

    const int radio_repetitions = parts::sx::kPreambleChips / 2;
    const int lead_repetitions = 4;
    CHECK(radio_repetitions == 8);
    CHECK(radio_repetitions + lead_repetitions >= 5);
    CHECK(radio_repetitions + lead_repetitions <= 12);
}

TEST_CASE("ADS-L.4.SRD860.C.2.4: the sync word is 0x72 0x4B, Manchester-coded") {
    CHECK(static_cast<uint8_t>(protocol::kAdslSyncWord >> 16) == 0x72);
    CHECK(static_cast<uint8_t>(protocol::kAdslSyncWord >> 8) == 0x4B);

    uint8_t chips[protocol::kTxChipBytes] = {0};
    uint8_t frame[protocol::kAdslFrameBytes] = {0};
    protocol::encode_mband(protocol::kAdslSyncWord, frame, sizeof(frame), chips);
    CHECK(chips[2] == static_cast<uint8_t>(chips_of(0x72) >> 8));
    CHECK(chips[3] == static_cast<uint8_t>(chips_of(0x72)));
    CHECK(chips[4] == static_cast<uint8_t>(chips_of(0x4B) >> 8));
    CHECK(chips[5] == static_cast<uint8_t>(chips_of(0x4B)));
}

TEST_CASE("ADS-L.4.SRD860.C.2.5: consecutive Traffic transmissions change frequency") {
    uint32_t previous = 0;
    for (uint32_t utc = 1785628800; utc < 1785628810; utc++) {
        const int slot = timing::Transmitter::slot_in(utc, /*airborne=*/true);
        const uint32_t freq = timing::Scheduler::slot_freq(slot);
        CHECK((freq == timing::kMband0Hz || freq == timing::kMband1Hz));
        if (previous != 0) CHECK(freq != previous);
        previous = freq;
    }
}

// TODO: fc 18sep26 no O-band LDR: one radio, and the dwell map spends the second on M-band and HDR
TEST_CASE("ADS-L.4.SRD860.C.3: the O-band LDR channel is 38.4 kbps behind sync word 0xB4 0x2B" *
          doctest::skip()) {
    FAIL("O-band LDR is not implemented: nothing tunes 869.525 MHz at 38.4 kbps");
}

TEST_CASE("ADS-L.4.SRD860.C.4: the O-band HDR channel is modulated as the table prints it") {
    CHECK(timing::kObandHz == 869525000u);
    CHECK(protocol::kUplinkChannelBandwidthHz == 250000u);
    CHECK(protocol::kUplinkChipRateBps == 200000u);
    CHECK(protocol::kUplinkDeviationHz == 50000u);
    CHECK(protocol::kUplinkGaussianBtE2 == 50u);
    // GMSK is GFSK at modulation index 0.5, which is what the table's implicit deviation says.
    CHECK(4u * protocol::kUplinkDeviationHz == protocol::kUplinkChipRateBps);
}

TEST_CASE("ADS-L.4.SRD860.C.4.2: the O-band HDR preamble is 10, twelve times") {
    CHECK(protocol::kUplinkPreambleChips == 24u);
    CHECK(protocol::kUplinkPreambleChips / 2u == 12u);
}

TEST_CASE("ADS-L.4.SRD860.C.4.3: the O-band HDR sync word is 0x2D 0xD4, uncoded") {
    CHECK(protocol::kUplinkSync[0] == 0x2D);
    CHECK(protocol::kUplinkSync[1] == 0xD4);

    uint8_t frame[protocol::kUplinkFrameBytes] = {0};
    frame[0] = 0xA5;
    uint8_t out[protocol::kUplinkBurstBytes] = {0};
    CHECK(protocol::encode_oband(frame, out) == static_cast<size_t>(protocol::kUplinkBurstBytes));
    CHECK(out[0] == 0x2D);
    CHECK(out[1] == 0xD4);
    CHECK(out[3] == 0xA5);
}

TEST_CASE("ADS-L.4.SRD860.C.5: own-ship transmits inside the Direct slot and nowhere else") {
    CHECK(timing::kDirectStart == 450);
    CHECK(timing::kDirectEnd == 1000);

    timing::ClockState clock{};
    clock.utc_valid = true;
    clock.pps_locked = true;

    for (uint32_t addr = 1; addr < 64; addr++) {
        timing::Transmitter tx;
        tx.configure(addr * 0x9E37u);
        for (uint32_t utc = 0; utc < 32; utc++) {
            for (int slot = 0; slot < 2; slot++) {
                const timing::SlotPlan plan =
                    timing::Scheduler::plan(timing::Scheduler::slot_start(slot), clock);
                const timing::Transmitter::Attempt a = tx.attempt(plan, utc, 1000, true, 0);
                if (!a.go) continue;
                CHECK(timing::Scheduler::in_direct_slot(a.at_ms));
                CHECK(a.at_ms + static_cast<int>(timing::Transmitter::kAirTimeMs) <=
                      timing::kDirectEnd);
            }
        }
    }
}

// The appendix's airborne transmitter: 12 to 14 dBm e.r.p. on the M band, and half a second of
// latency.
TEST_CASE("ADS-L.4.SRD860.APPENDIX: the transmitter sits inside the nominal power and latency") {
    CHECK(parts::sx::kResultingErpCentiDb >= 1200);
    CHECK(parts::sx::kResultingErpCentiDb <= 1400);
    CHECK(timing::Transmitter::kFixLagMaxMs <= 500);
}

TEST_CASE("ADS-L.4.SRD860.C.5: the Uplink slot is listened to on the O band, inside its edges") {
    CHECK(timing::kUplinkRxStart >= 200);
    CHECK(timing::kUplinkRxEnd <= 450);
    for (int phase = timing::kUplinkRxStart; phase < timing::kUplinkRxEnd; phase++) {
        CHECK(timing::Scheduler::band_at(phase) == timing::Band::O);
        CHECK(timing::Scheduler::freq_at(phase) == timing::kObandHz);
    }
    for (int phase = 0; phase < 200; phase++) CHECK_FALSE(timing::Scheduler::in_direct_slot(phase));
}
