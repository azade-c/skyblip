// The g-meter page: what the airframe is pulling now, and the worst of this flight.
#include "doctest/doctest.h"
#include "products/skyblip_go/pages/gmeter.h"
#include "test/support/glass_text.h"

using namespace skyblip;
using go::Glass;
using go::GMeterSnapshot;

namespace {

constexpr int kFieldCx = 100;
constexpr int kFieldCy = 82;

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

bool scale_drawn(const Glass& fb) { return fb.get_pixel(48, 30) && fb.get_pixel(152, 134); }

}  // namespace

TEST_CASE("gmeter: level flight puts the marker in the middle of the field") {
    Glass fb;
    draw_gmeter(fb, flying());

    CHECK(marker_at(fb, kFieldCx, kFieldCy));
    CHECK(reads_in(fb, "+1.0", 140, 0, 200, 20, 2));
}

TEST_CASE("gmeter: the three axes each read their own now, most and least") {
    GMeterSnapshot s = flying();
    s.now = flight::GLoad{2300, -400, 300};
    s.most = flight::GLoad{4200, 700, 300};
    s.least = flight::GLoad{-1300, -600, -400};
    Glass fb;
    draw_gmeter(fb, s);

    CHECK(reads_in(fb, "NRM", 0, 135, 30, 144));
    CHECK(reads_in(fb, "+2.3", 35, 135, 68, 144));
    CHECK(reads_in(fb, "+4.2", 78, 135, 110, 144));
    CHECK(reads_in(fb, "-1.3", 114, 135, 146, 144));

    CHECK(reads_in(fb, "LAT", 0, 145, 30, 154));
    CHECK(reads_in(fb, "L0.4", 35, 145, 68, 154));
    CHECK(reads_in(fb, "R0.7", 78, 145, 110, 154));
    CHECK(reads_in(fb, "L0.6", 114, 145, 146, 154));

    CHECK(reads_in(fb, "F/A", 0, 177, 30, 186));
    CHECK(reads_in(fb, "F0.3", 35, 177, 68, 186));
    CHECK(reads_in(fb, "A0.4", 114, 177, 146, 186));
}

// Pull g and the marker rises, pull left and it goes left.
TEST_CASE("gmeter: the marker moves the way the aircraft is loaded") {
    GMeterSnapshot pulling = flying();
    pulling.now = flight::GLoad{3000, 0, 0};
    pulling.most = flight::GLoad{3000, 0, 0};
    Glass up;
    draw_gmeter(up, pulling);
    CHECK(marker_at(up, kFieldCx, kFieldCy - 32));
    CHECK_FALSE(marker_at(up, kFieldCx, kFieldCy));

    GMeterSnapshot skidding = flying();
    skidding.now = flight::GLoad{1000, -500, 0};
    skidding.least = flight::GLoad{1000, -500, 0};
    Glass left;
    draw_gmeter(left, skidding);
    CHECK(marker_at(left, kFieldCx - 26, kFieldCy));
}

// Braking and acceleration have nothing to do with the wings, so they read apart from them.
TEST_CASE("gmeter: fore and aft is on its own strip, below the field") {
    GMeterSnapshot braking = flying();
    braking.now = flight::GLoad{1000, 0, -500};
    braking.least = flight::GLoad{1000, 0, -500};
    Glass fb;
    draw_gmeter(fb, braking);

    CHECK(fb.get_pixel(kFieldCx - 39, 165));
    CHECK_FALSE(fb.get_pixel(kFieldCx + 39, 165));
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
    CHECK(reads_in(fb, "----", 35, 135, 68, 144));
}
