// The g-meter page: what the airframe is pulling, and the worst of this flight.
#include "doctest/doctest.h"
#include "products/skyblip_go/pages/gmeter.h"
#include "test/support/glass_text.h"

using namespace skyblip;
using go::Glass;
using go::GMeterSnapshot;

namespace {

constexpr int kFieldCx = 82;
constexpr int kFieldCy = 82;
constexpr int kStripCx = 164;

GMeterSnapshot flying() {
    GMeterSnapshot s;
    s.fitted = true;
    s.valid = true;
    s.now = flight::GLoad{1000, 0, 0};
    s.most = flight::GLoad{1000, 0, 0};
    s.least = flight::GLoad{1000, 0, 0};
    return s;
}

bool marker_at(const Glass& fb, int x, int y) {
    return fb.get_pixel(x - 2, y - 2) && fb.get_pixel(x + 2, y + 2);
}

bool scale_drawn(const Glass& fb) { return fb.get_pixel(30, 30) && fb.get_pixel(134, 134); }

}  // namespace

TEST_CASE("gmeter: level flight puts the marker in the middle of the field") {
    Glass fb;
    draw_gmeter(fb, flying());

    CHECK(marker_at(fb, kFieldCx, kFieldCy));
    CHECK(reads_in(fb, "+1.0", 40, 138, 120, 156, 2));
}

TEST_CASE("gmeter: the three axes each read their own now, most and least") {
    GMeterSnapshot s = flying();
    s.now = flight::GLoad{2300, -400, 300};
    s.most = flight::GLoad{4200, 700, 300};
    s.least = flight::GLoad{-1300, -600, -400};
    Glass fb;
    draw_gmeter(fb, s);

    CHECK(reads_in(fb, "NRM", 0, 142, 30, 152));
    CHECK(reads_in(fb, "+2.3", 40, 138, 120, 156, 2));
    CHECK(reads_in(fb, "+4.2", 118, 142, 158, 152));
    CHECK(reads_in(fb, "-1.3", 156, 142, 196, 152));

    CHECK(reads_in(fb, "LAT", 0, 162, 30, 172));
    CHECK(reads_in(fb, "L0.4", 40, 158, 120, 176, 2));
    CHECK(reads_in(fb, "R0.7", 118, 162, 158, 172));
    CHECK(reads_in(fb, "L0.6", 156, 162, 196, 172));

    CHECK(reads_in(fb, "LON", 0, 182, 30, 192));
    CHECK(reads_in(fb, "ACC0.3", 30, 178, 120, 196, 2));
    CHECK(reads_in(fb, "ACC0.3", 118, 182, 158, 192));
    CHECK(reads_in(fb, "DEC0.4", 156, 182, 196, 192));
}

// The field shows where the load throws you: pull g and the marker sinks.
TEST_CASE("gmeter: the marker moves the way the aircraft is loaded") {
    GMeterSnapshot pulling = flying();
    pulling.now = flight::GLoad{3000, 0, 0};
    pulling.most = flight::GLoad{3000, 0, 0};
    Glass up;
    draw_gmeter(up, pulling);
    CHECK(marker_at(up, kFieldCx, kFieldCy + 32));
    CHECK_FALSE(marker_at(up, kFieldCx, kFieldCy));

    GMeterSnapshot pushing = flying();
    pushing.now = flight::GLoad{-1000, 0, 0};
    pushing.least = flight::GLoad{-1000, 0, 0};
    Glass over;
    draw_gmeter(over, pushing);
    CHECK(marker_at(over, kFieldCx, kFieldCy - 32));

    GMeterSnapshot skidding = flying();
    skidding.now = flight::GLoad{1000, -500, 0};
    skidding.least = flight::GLoad{1000, -500, 0};
    Glass left;
    draw_gmeter(left, skidding);
    CHECK(marker_at(left, kFieldCx - 26, kFieldCy));
}

// Braking throws you at the nose, which is the top of the strip.
TEST_CASE("gmeter: the longitudinal strip stands up, with the nose at the top") {
    GMeterSnapshot braking = flying();
    braking.now = flight::GLoad{1000, 0, -500};
    braking.least = flight::GLoad{1000, 0, -500};
    Glass fb;
    draw_gmeter(fb, braking);

    CHECK(fb.get_pixel(kStripCx, kFieldCy - 18));
    CHECK_FALSE(fb.get_pixel(kStripCx, kFieldCy + 18));

    GMeterSnapshot accelerating = flying();
    accelerating.now = flight::GLoad{1000, 0, 500};
    accelerating.most = flight::GLoad{1000, 0, 500};
    Glass takeoff;
    draw_gmeter(takeoff, accelerating);

    CHECK(takeoff.get_pixel(kStripCx, kFieldCy + 18));
    CHECK_FALSE(takeoff.get_pixel(kStripCx, kFieldCy - 18));
}

TEST_CASE("gmeter: the strip says which end is which, so no sign has to be remembered") {
    Glass fb;
    draw_gmeter(fb, flying());

    CHECK(reads_in(fb, "DEC", 152, 28, 180, 42));
    CHECK(reads_in(fb, "ACC", 152, 120, 180, 136));
}

TEST_CASE("gmeter: a unit with no sensor prints no scale to read nothing off") {
    GMeterSnapshot none;
    Glass fb;
    draw_gmeter(fb, none);

    CHECK(reads_in(fb, "NO SENSOR", 0, 60, 200, 100));
    CHECK_FALSE(scale_drawn(fb));
}

// The ball's cage is drawn empty for the same reason: the scale is true, the reading is not there.
TEST_CASE("gmeter: a sensor that has not reported yet leaves the field empty") {
    GMeterSnapshot waiting;
    waiting.fitted = true;
    Glass fb;
    draw_gmeter(fb, waiting);

    CHECK(scale_drawn(fb));
    CHECK_FALSE(marker_at(fb, kFieldCx, kFieldCy));
    CHECK(reads_in(fb, "----", 88, 142, 120, 152));
}
