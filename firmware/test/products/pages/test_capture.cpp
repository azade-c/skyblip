// The page that states the price of a capture before it takes it, and what it took.
#include "doctest/doctest.h"
#include "products/skyblip_go/pages/capture.h"
#include "test/support/glass_text.h"

using namespace skyblip::go;
using skyblip::reads_in;

namespace {

CaptureSnapshot disarmed_device() {
    CaptureSnapshot snap;
    snap.uptime_s = 812;
    snap.capture.available = true;
    snap.capture.pool_sectors = 330;
    snap.capture.price_sectors = 266;
    snap.capture.price_flights = 3;
    // 266 sectors of 170 slots at seven records a second is 6460 s, 1 h 47 m.
    snap.capture.keeps_s = 6460;
    return snap;
}

CaptureSnapshot armed_device() {
    CaptureSnapshot snap = disarmed_device();
    snap.capture.armed = true;
    snap.capture.session_id = 1785628800;
    snap.capture.records = 4213;
    snap.capture.dropped = 17;
    snap.capture.sectors = 25;
    snap.capture.keeps_s = 3600;
    return snap;
}

}  // namespace

TEST_CASE("capture page: a disarmed device states the price before it is paid") {
    Glass fb;
    draw_capture(fb, disarmed_device());

    CHECK(reads_in(fb, "CAPTURE", 0, 0, 200, 14));
    CHECK(reads_in(fb, "T+812", 100, 0, 200, 14));
    CHECK(reads_in(fb, "STATE OFF", 0, 14, 200, 40));
    CHECK(reads_in(fb, "TAKES 266 OF 330 SECTORS", 0, 28, 200, 56));
    CHECK(reads_in(fb, "EVICTS 3 FLIGHTS", 0, 42, 200, 70));
    CHECK(reads_in(fb, "KEEPS 1H47 ROLLING", 0, 56, 200, 84));
    CHECK(reads_in(fb, "PRESS TWICE TO ARM", 0, 170, 200, 200));
}

TEST_CASE("capture page: an armed device shows what it wrote, what it lost and what it holds") {
    Glass fb;
    draw_capture(fb, armed_device());

    CHECK(reads_in(fb, "STATE ARMED", 0, 14, 200, 40));
    CHECK(reads_in(fb, "SESSION 1785628800", 0, 28, 200, 56));
    CHECK(reads_in(fb, "WROTE 4213 DROP 17", 0, 42, 200, 70));
    CHECK(reads_in(fb, "SECTORS 25 KEEPS 1H00", 0, 56, 200, 84));
    CHECK(reads_in(fb, "PRESS TWICE TO STOP", 0, 170, 200, 200));
}

// A capture that stopped on its own says so: the pilot did not ask for it.
TEST_CASE("capture page: a capture the allocator refused says it stopped and why") {
    CaptureSnapshot snap = disarmed_device();
    snap.capture.stopped = skyblip::bus::CaptureStop::NoSectors;
    snap.capture.records = 45220;
    Glass fb;
    draw_capture(fb, snap);

    CHECK(reads_in(fb, "STATE STOPPED NO SECTORS", 0, 14, 200, 40));
    CHECK(reads_in(fb, "WROTE 45220 DROP 0", 0, 70, 200, 112));
}

// A storage fault the pilot cannot see is a corpus with holes nobody accounted for.
TEST_CASE("capture page: a refused flash write is on the glass, not only in a counter") {
    CaptureSnapshot snap = armed_device();
    snap.capture.faults = 3;
    snap.capture.unreadable_sectors = 1;
    Glass fb;
    draw_capture(fb, snap);

    CHECK(reads_in(fb, "FLASH FAULTS 3 UNREAD 1", 0, 84, 200, 126));
}

TEST_CASE("capture page: a device with no partition offers no gesture at all") {
    CaptureSnapshot snap;
    snap.capture.available = false;
    Glass fb;
    draw_capture(fb, snap);

    CHECK(reads_in(fb, "NO FLASH FITTED", 0, 170, 200, 200));
    CHECK_FALSE(reads_in(fb, "PRESS TWICE TO ARM", 0, 0, 200, 200));
}
