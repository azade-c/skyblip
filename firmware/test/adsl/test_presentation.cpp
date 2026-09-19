// ADS-L 4 SRD-860 issue 2 Subpart F: the ADS-L header, who a packet is from and what it carries.
#include <cstdint>
#include <initializer_list>

#include "core/model/ownship.h"
#include "core/protocol/adsl.h"
#include "core/settings/address.h"
#include "doctest/doctest.h"
#include "products/skyblip_go/settings.h"

using namespace skyblip;

namespace {
protocol::AdslPacket traffic_packet(uint32_t addr = 0x123456, uint8_t table = 6,
                                    bool stealth = false) {
    model::OwnState own{};
    own.fix_valid = true;
    own.utc_valid = true;
    own.lat_1e7 = 481234500;
    own.lon_1e7 = 81234500;
    own.alt_mm = 1234000;
    own.speed_mm_s = 45000;  // 180 quarter-m/s on the wire
    own.hdop_e2 = 90;
    own.flight_state = 2;
    protocol::AdslPacket p{};
    protocol::from_own(p, own, addr, table, 4, stealth);
    return p;
}
}  // namespace

// 40 bits of header and 120 of Traffic payload: 20 bytes, which is the block E.2 scrambles whole.
TEST_CASE("ADS-L.4.SRD860.F.1: the ADS-L data is a 40-bit header and a payload behind it") {
    const int header_bits = 40;
    const int traffic_payload_bits = 120;
    CHECK((header_bits + traffic_payload_bits) / 8 == 20);
    CHECK(sizeof(protocol::AdslPacket{}.Byte) == 20);
    // The clause's rule for a payload width: 24 + N*32 bits.
    CHECK((traffic_payload_bits - 24) % 32 == 0);
}

TEST_CASE("ADS-L.4.SRD860.F.2: the header is a type byte, 30 bits of sender, reserved, relay") {
    protocol::AdslPacket p = traffic_packet();
    CHECK(p.Type == 0x02);
    CHECK(p.address() == 0x123456u);
    CHECK(p.addr_table() == 6);
    CHECK_FALSE(p.is_relay());

    p.set_relay();
    CHECK(p.is_relay());
    CHECK((p.Address[3] & 0x80) != 0);

    // One little-endian word: the AMT in bits 0 to 5, the address in bits 6 to 29.
    CHECK((protocol::AdslPacket::get4(p.Address) & 0x3Fu) == 6u);
    CHECK(((protocol::AdslPacket::get4(p.Address) >> 6) & 0x00FFFFFFu) == 0x123456u);
}

TEST_CASE("ADS-L.4.SRD860.F.2.1: a Traffic payload is type 2, in the broadcast half of the range") {
    protocol::AdslPacket p = traffic_packet();
    CHECK(p.Type == 0x02);
    CHECK(p.is_position());
    CHECK(p.Type < 0x80);
}

// Issue 2 puts Status, Remote ID and the uplinks on this band, and none of them is an aeroplane.
TEST_CASE("ADS-L.4.SRD860.F.2.1: a payload that is not Traffic never becomes an aircraft") {
    model::AircraftObs obs{};
    for (uint8_t type : {uint8_t(0x00), uint8_t(0x03), uint8_t(0x04), uint8_t(0x05), uint8_t(0x06),
                         uint8_t(0x42), uint8_t(0x82)}) {
        protocol::AdslPacket p = traffic_packet();
        p.Type = type;
        CHECK_FALSE(p.is_position());
        CHECK_FALSE(protocol::to_obs(p, events::Stamp{}, -80, model::Source::AdslDirect, obs));
        CHECK_FALSE(obs.position_valid);
    }
}

TEST_CASE("ADS-L.4.SRD860.F.2.2: the sender address is a 6-bit table and 24 bits of address") {
    protocol::AdslPacket p{};
    p.init();
    p.set_addr_table(63);
    p.set_address(0x00FFFFFFu);
    CHECK(p.addr_table() == 63);
    CHECK(p.address() == 0x00FFFFFFu);

    // Table 9 and up split the address: an 8-bit manufacturer prefix, then a 16-bit base.
    p.set_addr_table(9);
    p.set_address(0xAB1234u);
    CHECK(p.addr_table() == 9);
    CHECK((p.address() >> 16) == 0xABu);
    CHECK((p.address() & 0xFFFFu) == 0x1234u);
    CHECK(p.address_and_type() == ((9u << 24) | 0xAB1234u));

    go::Settings s = go::defaults(0x123456);
    s.addr_table = 64;
    CHECK(go::validate(s) == Status::OutOfRange);
    s.addr_table = 63;
    CHECK(go::validate(s) == Status::Ok);
}

// Table 5 is the aircraft's Mode-S code, and it has to match what a 1090 receiver sees.
TEST_CASE("ADS-L.4.SRD860.F.2.3: an ICAO address goes on the air exactly as it was configured") {
    CHECK(settings::safe_air_address(0xDD1234u, 5) == 0xDD1234u);
    CHECK(settings::safe_air_address(0x3C0A11u, 5) == 0x3C0A11u);
    CHECK(traffic_packet(0xDD1234u, 5).address() == 0xDD1234u);
    CHECK(traffic_packet(0xDD1234u, 5).addr_table() == 5);
    // A self-minted address is the only one this device is allowed to move off a crowded prefix.
    CHECK(settings::safe_air_address(0xDD1234u, 0) != 0xDD1234u);
}

TEST_CASE("ADS-L.4.SRD860.F.2.4: privacy mode selects table 0, the random one") {
    protocol::AdslPacket open = traffic_packet(0x3C0A11u, 5, /*stealth=*/false);
    protocol::AdslPacket hidden = traffic_packet(0x3C0A11u, 5, /*stealth=*/true);
    CHECK(open.addr_table() == 5);
    CHECK(hidden.addr_table() == 0);
    CHECK(settings::kAddrTableSelfMintedMax == 4);
}

// TODO: fc 18sep26 privacy reuses the device address, so the identity it hides is still constant
TEST_CASE("ADS-L.4.SRD860.F.2.4: a privacy address is drawn at random once per start-up" *
          doctest::skip()) {
    FAIL("privacy keeps the configured address: nothing is drawn at start-up, so it never changes");
}

// TODO: fc 18sep26 validate() takes any address under any table, ICAO included
TEST_CASE("ADS-L.4.SRD860.F.2.3: a table the configured address does not belong to is refused" *
          doctest::skip()) {
    FAIL(
        "settings accept table 5 with an address no registry issued, and the clause asks for that "
        "inconsistency to be refused");
}

// Every ADS-L data block has to be scramblable, and XXTEA works on whole 32-bit words.
TEST_CASE("ADS-L.4.SRD860.F.2.5: header plus payload is a whole number of 32-bit words") {
    const int header_bytes = 5;
    const int traffic_payload_bytes = 15;
    CHECK((header_bytes + traffic_payload_bytes) % 4 == 0);
    // The clause's own list of valid payload sizes: 3, 7, 11, 15, and on by fours.
    CHECK((traffic_payload_bytes - 3) % 4 == 0);
}
