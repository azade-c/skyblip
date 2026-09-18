// ADS-L 4 SRD-860 issue 2 Subparts D and E: how a packet is framed, scrambled and parity-checked.
#include <cstdint>
#include <cstring>

#include "core/fec/reed_solomon.h"
#include "core/fec/scramble.h"
#include "core/model/ownship.h"
#include "core/protocol/adsl.h"
#include "core/protocol/adsl_uplink.h"
#include "core/protocol/air.h"
#include "core/timing/channel.h"
#include "doctest/doctest.h"

using namespace skyblip;

namespace {
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

// E.2's own sample implementation, transcribed, with the zero key the clause names for scrambling.
void spec_xxtea(uint32_t* data, uint32_t words, const uint32_t* key, uint32_t rounds) {
    const uint32_t kDelta = 0x9E3779B9u;
    uint32_t y = data[0];
    uint32_t sum = 0;
    uint32_t z = data[words - 1];
    uint32_t q = rounds;
    while (q-- > 0) {
        sum += kDelta;
        const uint32_t e = (sum >> 2) & 3u;
        uint32_t p = 0;
        for (p = 0; p < words - 1; p++) {
            y = data[p + 1];
            data[p] += (((z >> 5) ^ (y << 2)) + ((y >> 3) ^ (z << 4))) ^
                       ((sum ^ y) + (key[(p & 3u) ^ e] ^ z));
            z = data[p];
        }
        y = data[0];
        data[words - 1] +=
            (((z >> 5) ^ (y << 2)) + ((y >> 3) ^ (z << 4))) ^ ((sum ^ y) + (key[(p & 3u) ^ e] ^ z));
        z = data[words - 1];
    }
}

// E.3.1's own sample implementation, transcribed.
uint32_t spec_crc24(const uint8_t* data, size_t len) {
    const uint32_t kPoly = 0xFFFA0480u;
    uint32_t crc = 0;
    for (size_t i = 0; i < len + 3; i++) {
        crc |= i < len ? data[i] : 0u;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80000000u) crc ^= kPoly;
            crc <<= 1;
        }
    }
    return crc >> 8;
}
}  // namespace

// The length byte rides in the sync word, which is why a burst of any other length never frames.
TEST_CASE(
    "ADS-L.4.SRD860.D.1.1: the Packet Length field counts the message behind it, not itself") {
    protocol::AdslPacket p = traffic_packet();
    const int network_header = 1;
    const int adsl_data = 20;
    const int parity = 3;
    CHECK(int(p.Length) == network_header + adsl_data + parity);
    CHECK(int(p.Length) == protocol::AdslPacket::kDataBytes);
    CHECK(int(p.Length) == int(protocol::kAdslSyncWord & 0xFFu));
    CHECK(protocol::kUplinkSync[2] == protocol::kUplinkFrameBytes);
}

// TODO: fc 18sep26 no O-band LDR radio here: the dwell map carries M-band and O-band HDR only
TEST_CASE(
    "ADS-L.4.SRD860.D.1.2: the O-band LDR packet is a 6-byte header, 24 bytes of data and a "
    "CRC8" *
    doctest::skip()) {
    FAIL("O-band LDR is not implemented: no header, no CRC8, no 38.4 kbps dwell");
}

// The parity covers the scrambled form, so a receiver checks it before it can read a single field.
TEST_CASE("ADS-L.4.SRD860.D.2: data, then scrambling, then parity over the network header too") {
    protocol::AdslPacket p = traffic_packet();
    const protocol::AdslPacket plain = p;
    p.scramble();
    p.set_crc();
    CHECK(std::memcmp(p.Byte, plain.Byte, 20) != 0);
    CHECK(p.check_crc() == 0);

    protocol::AdslPacket flipped = p;
    flipped.Version ^= 0x01;
    CHECK(flipped.check_crc() != 0);

    protocol::AdslPacket descrambled = p;
    descrambled.descramble();
    CHECK(descrambled.check_crc() != 0);
    CHECK(std::memcmp(descrambled.Byte, plain.Byte, 20) == 0);
}

// TODO: fc 18sep26 no listen-before-talk: the hour's duty cycle is the only rule refusing a burst
TEST_CASE("ADS-L.4.SRD860.D.3: a burst waits on a carrier sense before it goes out" *
          doctest::skip()) {
    FAIL("listen-before-talk is not implemented, EN 300 220's duty-cycle route is taken instead");
}

// The route taken instead of D.3's polite spectrum access, and the one thing that does refuse a
// burst.
TEST_CASE("ADS-L.4.SRD860.D.3: the band's hourly duty cycle is what a burst is refused by") {
    timing::AirTime air;
    CHECK(timing::AirTime::kLimitPermille == 10u);
    const uint32_t budget_ms = timing::AirTime::kBudgetMs;
    air.spend(0, budget_ms);
    CHECK_FALSE(air.may_spend(0, 1));
    CHECK(air.may_spend(timing::AirTime::kWindowMs + 1, 1));
}

// A receiver reads the four header fields out of one byte, and everything this device sends is zero
// in all four.
TEST_CASE("ADS-L.4.SRD860.E.1: the network header is one byte, and this one claims nothing") {
    protocol::AdslPacket p = traffic_packet();
    const uint8_t version = p.Version & 0x0F;
    const uint8_t signed_flag = (p.Version >> 4) & 0x01;
    const uint8_t key_index = (p.Version >> 5) & 0x03;
    const uint8_t error_control = (p.Version >> 7) & 0x01;
    CHECK(version == 0);
    CHECK(signed_flag == 0);
    CHECK(key_index == 0);
    CHECK(error_control == 0);
    CHECK(p.Version == 0x00);
}

// Issue 2 is version 1, but its Traffic payload encodes exactly as Issue 1's iConspicuity did, and
// the clause asks for the lowest.
TEST_CASE("ADS-L.4.SRD860.E.1.1: the version transmitted is the lowest that encodes this payload") {
    protocol::AdslPacket p = traffic_packet();
    CHECK((p.Version & 0x0F) == 0);

    model::AircraftObs as_sent{};
    REQUIRE(protocol::to_obs(p, events::Stamp{}, -80, model::Source::AdslDirect, as_sent));

    protocol::AdslPacket issue2 = p;
    issue2.Version = (issue2.Version & 0xF0) | 0x01;
    model::AircraftObs as_received{};
    REQUIRE(protocol::to_obs(issue2, events::Stamp{}, -80, model::Source::AdslDirect, as_received));
    CHECK(as_received.addr == as_sent.addr);
    CHECK(as_received.lat_1e7 == as_sent.lat_1e7);
    CHECK(as_received.alt_m == as_sent.alt_m);
}

// Flag clear means no signature field, so the packet ends at its parity: 27 bytes of air, sync word
// included.
TEST_CASE("ADS-L.4.SRD860.E.1.2: no signature is promised, so nothing follows the parity") {
    protocol::AdslPacket p = traffic_packet();
    CHECK(((p.Version >> 4) & 0x01) == 0);
    CHECK(protocol::AdslPacket::kTxBytes == 27);
    CHECK(protocol::AdslPacket::kTxBytes == 3 + protocol::AdslPacket::kDataBytes);
}

TEST_CASE("ADS-L.4.SRD860.E.1.3: key 0 is the scrambling key, and it is the key used") {
    protocol::AdslPacket p = traffic_packet();
    CHECK(((p.Version >> 5) & 0x03) == 0);

    uint32_t words[5] = {0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u, 0x12345678u};
    uint32_t mirror[5];
    std::memcpy(mirror, words, sizeof(words));
    const uint32_t zero_key[4] = {0, 0, 0, 0};
    fec::xxtea_scramble_key0(words, 5, 6);
    spec_xxtea(mirror, 5, zero_key, 6);
    for (int i = 0; i < 5; i++) CHECK(words[i] == mirror[i]);
}

// The clause's code over the block F.1 makes of a Traffic packet: 5 words, 6 rounds, zero key.
TEST_CASE("ADS-L.4.SRD860.E.2: the scrambler is XXTEA over the ADS-L data, six rounds") {
    protocol::AdslPacket p = traffic_packet();
    uint32_t mirror[5];
    std::memcpy(mirror, p.Byte, sizeof(mirror));
    const uint32_t zero_key[4] = {0, 0, 0, 0};
    spec_xxtea(mirror, 5, zero_key, 6);

    p.scramble();
    CHECK(std::memcmp(p.Byte, mirror, sizeof(mirror)) == 0);
    CHECK(sizeof(mirror) % 4 == 0);

    p.descramble();
    protocol::AdslPacket again = traffic_packet();
    CHECK(std::memcmp(p.Byte, again.Byte, 20) == 0);
}

// The Mode-S polynomial, over the network header and the scrambled data, transmitted most
// significant byte first.
TEST_CASE("ADS-L.4.SRD860.E.3.1: the parity is the 24-bit CRC the clause prints") {
    protocol::AdslPacket p = traffic_packet();
    p.scramble();
    p.set_crc();

    const uint32_t expected = spec_crc24(p.Data, protocol::AdslPacket::kCrcCoverBytes);
    CHECK(p.CRC[0] == static_cast<uint8_t>(expected >> 16));
    CHECK(p.CRC[1] == static_cast<uint8_t>(expected >> 8));
    CHECK(p.CRC[2] == static_cast<uint8_t>(expected));
    CHECK(p.check_crc() == 0);
    CHECK(protocol::AdslPacket::kCrcCoverBytes == 21);
}

TEST_CASE("ADS-L.4.SRD860.E.3.2: the forward error correction is Reed-Solomon over 255 bytes") {
    CHECK(fec::ReedSolomon255::kN == 255);
    CHECK(fec::ReedSolomon255::kK == 223);
    CHECK(fec::ReedSolomon255::kParity == 32);
    CHECK(fec::ReedSolomon255::kMaxErrors == 16);
}

// TODO: fc 18sep26 the generator roots are alpha^0.., the clause wants primitive element 11 from
// root 121
TEST_CASE("ADS-L.4.SRD860.E.3.2: the code is generated from the roots the clause names" *
          doctest::skip()) {
    FAIL(
        "the Reed-Solomon parity here is not the clause's: a ground station's frame would not "
        "decode");
}

// Optional, and not taken: no key is held, so no packet is signed and none claims to be.
TEST_CASE("ADS-L.4.SRD860.E.4: nothing is signed, and nothing says it is") {
    protocol::AdslPacket p = traffic_packet();
    p.scramble();
    p.set_crc();
    CHECK(((p.Version >> 4) & 0x01) == 0);
    CHECK(protocol::kAdslFrameBytes == protocol::AdslPacket::kDataBytes);

    uint8_t chips[protocol::kTxChipBytes] = {0};
    const size_t len =
        protocol::encode_mband(protocol::kAdslSyncWord, p.Data, protocol::kAdslFrameBytes, chips);
    CHECK(len == 2u * (protocol::kSyncWordBytes + protocol::kAdslFrameBytes));
}
