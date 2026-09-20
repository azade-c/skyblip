// The page a bench reads when a burst went missing, in the units the counters keep.
#include "doctest/doctest.h"
#include "products/skyblip_go/pages/raw.h"
#include "test/support/glass_text.h"

using namespace skyblip::go;
using skyblip::reads_in;

namespace {

RawSnapshot bench_device() {
    RawSnapshot snap;
    snap.uptime_s = 1234;

    snap.gnss.health.baud = 115200;
    snap.gnss.health.config = skyblip::ports::GnssConfig::Ready;
    snap.gnss.health.overruns = 0;
    snap.gnss.health.sentences = 5210;
    snap.gnss.health.rejected = 6;
    snap.gnss.health.pps_latency_ms = 28;
    snap.gnss.health.reject = skyblip::gnss::FixReject::Stale;
    snap.gnss.health.identified = true;
    snap.gnss.nav_ms = 98;
    snap.gnss.nav_valid = true;
    snap.gnss.fix_valid = true;
    snap.gnss.sats = 22;
    snap.gnss.in_use = 12;
    snap.gnss.fix_mode = 3;
    snap.gnss.hdop_e2 = 90;
    snap.gnss.vdop_e2 = 150;
    snap.gnss.resid_m = 3;
    snap.gnss.resid_valid = true;
    snap.gnss.settled = true;
    snap.gnss.levels_live = true;
    snap.gnss.solutions = 5210;
    snap.gnss.utc = 12 * 3600 + 34 * 60 + 56;
    snap.gnss.utc_valid = true;
    snap.gnss.pps = PpsState::Lock;
    snap.gnss.pps_age_ms = 3;

    snap.radio.rx_ok = 30;
    snap.radio.rx_bad = 2;
    snap.radio.rx_unframed = 1;
    snap.radio.rx_miskeyed = 0;
    snap.radio.rx_noise = 16;
    snap.radio.rx_type = 4;
    snap.radio.rx_wait = 7;
    snap.radio.uplink_frames = 4;
    snap.radio.uplink_bad = 0;
    snap.radio.uplink_targets = 3;
    snap.radio.tx_ok = 19;
    snap.radio.tx_lost = 1;
    snap.radio.missed = 2;
    snap.radio.refused = 0;
    snap.radio.duty_permille = 5;
    snap.radio.tx_keyed_us = 609;
    snap.radio.tx_span_us = 5919;
    snap.radio.dwell_worst_us = 120;
    snap.radio.pps_worst_us = -12;
    snap.radio.holdover = 0;
    snap.radio.tracked = 4;
    snap.radio.alarm = 1;
    snap.radio.noise_dbm = -104;
    snap.radio.slot = skyblip::timing::SlotState::Slot0;
    snap.radio.freq_hz = skyblip::timing::kMband0Hz;
    snap.radio.tx_allowed = true;
    return snap;
}

}  // namespace

TEST_CASE("raw: the receiver block names the rate, the sentence phase and every refusal") {
    Glass fb;
    draw_raw(fb, bench_device());

    CHECK(reads_in(fb, "PORT 115200 READY", 0, 12, 200, 30));
    CHECK(reads_in(fb, "OVR 0", 0, 12, 200, 30));
    CHECK(reads_in(fb, "NAV 098MS", 0, 24, 200, 41));
    CHECK(reads_in(fb, "WIRE 28MS", 0, 24, 200, 41));
    CHECK(reads_in(fb, "SENT 5210", 0, 24, 200, 41));
    CHECK(reads_in(fb, "FIX OK SV 22 USE 12", 0, 35, 200, 52));
    CHECK(reads_in(fb, "HDOP 0.90 VDOP 1.50 RES 3M", 0, 46, 200, 63));
    CHECK(reads_in(fb, "REJ STALE N 6", 0, 57, 200, 74));
    CHECK(reads_in(fb, "PPS LOCK 3MS", 0, 68, 200, 85));
    CHECK(reads_in(fb, "UTC 12:34:56", 0, 68, 200, 85));
}

TEST_CASE("raw: the radio block names the dwell it is in and every verdict it has counted") {
    Glass fb;
    draw_raw(fb, bench_device());

    CHECK(reads_in(fb, "DWELL M0 868.2 TX Y", 0, 82, 200, 100));
    CHECK(reads_in(fb, "-104DBM", 100, 82, 200, 100));
    CHECK(reads_in(fb, "RX 30 CRC 2 SYNC 1 KEY 0", 0, 93, 200, 111));
    CHECK(reads_in(fb, "DEC 16 TYPE 4 WAIT 7", 0, 104, 200, 122));
    CHECK(reads_in(fb, "UPLINK 4/0/3 TRK 4 ALM 1", 0, 115, 200, 133));
    CHECK(reads_in(fb, "TX 19 LOST 1 MISS 2 HELD 0", 0, 126, 200, 144));
    CHECK(reads_in(fb, "DUTY 5/1000 KEYED 609US", 0, 137, 200, 155));
    CHECK(reads_in(fb, "SPAN 5919US SLOT 120US", 0, 148, 200, 166));
    CHECK(reads_in(fb, "PPS -12US HOLDOVER 0", 0, 159, 200, 177));
}

// A zero would read as a solution landing at the top of the second, which it never does.
TEST_CASE("raw: a solution nothing has dated reports no phase at all") {
    RawSnapshot snap = bench_device();
    snap.gnss.nav_valid = false;
    snap.gnss.utc_valid = false;
    snap.gnss.pps = PpsState::None;
    snap.gnss.fix_valid = false;
    snap.gnss.stage = skyblip::gnss::Stage::Blind;
    snap.gnss.resid_valid = false;
    snap.gnss.hdop_e2 = 0;
    Glass fb;
    draw_raw(fb, snap);

    CHECK(reads_in(fb, "NAV ---", 0, 24, 200, 41));
    CHECK(reads_in(fb, "FIX BLIND", 0, 35, 200, 52));
    CHECK(reads_in(fb, "HDOP --- VDOP 1.50 RES ---", 0, 46, 200, 63));
    CHECK(reads_in(fb, "PPS NONE", 0, 68, 200, 85));
    CHECK(reads_in(fb, "UTC --:--:--", 0, 68, 200, 85));
}
