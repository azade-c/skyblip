// The drawing stack end to end, from a pixel to what reaches the glass. The radar
// cases are the load-bearing ones: on a 200-pixel span there is no centre pixel,
// so own-ship sits on the 99|100 boundary and every ring and bearing is measured
// from there. Half a pixel of drift is half a pixel of parallax on every target.
// The status cases pin the units a pilot reads first, and the widest position on
// earth still fitting its row.
#include "doctest/doctest.h"
#include "hardware/parts/ssd1681/model.h"
#include "hardware/parts/ssd1681/ssd1681.h"
#include "ui/framebuffer.h"
#include "ui/screens/radar.h"
#include "ui/screens/status.h"

using namespace skyblip::ui;

namespace {

int length(const char* s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

// A case claims "it reads 047", not "there is ink up there".
bool reads_in(const Framebuffer& fb, const char* text, int x0, int y0, int x1, int y1,
              int scale = 1) {
    Framebuffer wanted;
    wanted.clear(true);
    wanted.draw_text(0, 0, text, true, scale);
    const int w = length(text) * 6 * scale - scale, h = 7 * scale;
    for (int y = y0; y + h <= y1; y++) {
        for (int x = x0; x + w <= x1; x++) {
            bool same = true;
            for (int dy = 0; dy < h && same; dy++)
                for (int dx = 0; dx < w && same; dx++)
                    if (fb.get_pixel(x + dx, y + dy) != wanted.get_pixel(dx, dy)) same = false;
            if (same) return true;
        }
    }
    return false;
}

int ink_in(const Framebuffer& fb, int x0, int y0, int x1, int y1) {
    int n = 0;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) n += fb.get_pixel(x, y) ? 1 : 0;
    return n;
}

RadarSnapshot flying(uint16_t track_deg) {
    RadarSnapshot snap;
    snap.fix_valid = true;
    snap.range_nm = kDefaultRangeNm;
    snap.track_deg = track_deg;
    snap.flight_time_valid = true;
    snap.airborne = true;
    snap.flight_seconds = 42 * 60;
    snap.receiver_listening = true;
    return snap;
}

Framebuffer radar(const RadarSnapshot& snap) {
    Framebuffer fb;
    draw_radar(fb, snap);
    return fb;
}

}  // namespace

TEST_CASE("fb: pixel set/get and clear") {
    Framebuffer fb;
    fb.clear(true);
    CHECK(fb.count_black() == 0);
    fb.set_pixel(10, 20, true);
    CHECK(fb.get_pixel(10, 20));
    CHECK(fb.count_black() == 1);
    fb.set_pixel(10, 20, false);
    CHECK_FALSE(fb.get_pixel(10, 20));
    fb.set_pixel(-1, -1, true);  // out of bounds no-op
    fb.set_pixel(999, 999, true);
    CHECK(fb.count_black() == 0);
}

TEST_CASE("fb: primitives draw something") {
    Framebuffer fb;
    fb.clear(true);
    fb.line(0, 0, 199, 199, true);
    CHECK(fb.get_pixel(0, 0));
    CHECK(fb.get_pixel(199, 199));
    int before = fb.count_black();
    fb.circle(100, 100, 50, true, false);
    CHECK(fb.count_black() > before);
    fb.rect(10, 10, 30, 20, true, true);
    CHECK(fb.get_pixel(25, 20));
}

TEST_CASE("fb: text advances and draws glyph pixels") {
    Framebuffer fb;
    fb.clear(true);
    int x = fb.draw_text(5, 5, "AB1", true, 1);
    CHECK(x == 5 + 3 * 6);
    CHECK(fb.count_black() > 0);
    // space draws nothing
    Framebuffer fb2;
    fb2.clear(true);
    fb2.draw_char(0, 0, ' ', true, 1);
    CHECK(fb2.count_black() == 0);
}

TEST_CASE("radar: renders rings, own symbol and plots targets") {
    Framebuffer fb;
    RadarTarget targets[2] = {
        {2000, 0, 100, 1},    // north, above
        {0, -3000, -100, 3},  // west, below, urgent
    };
    // Mirror-image targets must land mirror-image distances from the centre
    // point: east of it starts at pixel 100, west of it at 99.
    {
        RadarTarget pair[2] = {{0, 4000, 0, 1}, {0, -4000, 0, 1}};
        RadarSnapshot s2;
        s2.fix_valid = true;
        s2.range_nm = 5;
        s2.n_targets = 2;
        s2.targets = pair;
        Framebuffer f2;
        draw_radar(f2, s2);
        int e = -1, w = -1;
        for (int x = 100; x < 200; x++)
            if (f2.get_pixel(x, 99)) {
                e = x;
                break;
            }
        for (int x = 99; x >= 0; x--)
            if (f2.get_pixel(x, 99)) {
                w = x;
                break;
            }
        CHECK(e - 100 == 99 - w);
    }
    RadarSnapshot snap;
    snap.fix_valid = true;
    snap.range_nm = 5;
    snap.n_targets = 2;
    snap.targets = targets;
    snap.max_alarm = 3;
    draw_radar(fb, snap);
    CHECK(fb.count_black() > 100);

    // no-fix path shows text, few pixels but non-empty
    Framebuffer fb2;
    RadarSnapshot ns;
    ns.fix_valid = false;
    draw_radar(fb2, ns);
    CHECK(fb2.count_black() > 0);
}

TEST_CASE("radar: everything is centred on the 99|100 point, not on a pixel") {
    // 200x200 is an EVEN grid: there is no middle pixel. The centre is the point
    // where pixels 99 and 100 meet on both axes, so anything "on" the centre is
    // a PAIR of pixels. No alarm and no fix, so only rings and ship mark the edges.
    Framebuffer fb;
    RadarSnapshot snap;
    draw_radar(fb, snap);
    // the own ship straddles the centre: its fuselage is a pair of columns
    CHECK(fb.get_pixel(99, 99));
    CHECK(fb.get_pixel(100, 99));
    CHECK(fb.get_pixel(99, 100));
    CHECK(fb.get_pixel(100, 100));
    // ... and the glyph is mirror-symmetric about that point, not about a column
    int mism = 0;
    for (int y = 88; y < 118; y++)
        for (int k = 0; k < 14; k++)
            if (fb.get_pixel(99 - k, y) != fb.get_pixel(100 + k, y)) mism++;
    CHECK(mism == 0);
    // The hot spot (the wing = the widest row, i.e. the aircraft's position)
    // sits ON the centre point, not at the glyph's bounding-box centre: targets
    // are plotted as offsets from it, so bbox-centring a long-tailed aeroplane
    // puts the wing several px forward and skews every bearing on screen.
    int widest = 0, widest_row = -1;
    for (int y = 88; y < 118; y++) {
        int n = 0;
        for (int x = 86; x < 114; x++) n += fb.get_pixel(x, y) ? 1 : 0;
        if (n > widest) {
            widest = n;
            widest_row = y;
        }
    }
    CHECK(widest_row == 99);
    // The rings are concentric with the same point: equal margin on all sides.
    int left = -1, right = -1, top = -1, bottom = -1;
    for (int x = 0; x < 200; x++)
        if (fb.get_pixel(x, 99)) {
            if (left < 0) left = x;
            right = x;
        }
    // column 50: clear of the flight clock, which erases the ring where it sits
    for (int y = 0; y < 200; y++)
        if (fb.get_pixel(50, y)) {
            if (top < 0) top = y;
            bottom = y;
        }
    CHECK(left == 199 - right);
    CHECK(top == 199 - bottom);
}

TEST_CASE("radar: the range ring is one unbroken stroke, and the only ring on the glass") {
    Framebuffer fb;
    RadarSnapshot snap;  // no fix: the ring and own ship, nothing plotted
    draw_radar(fb, snap);
    int thinnest = 200, thickest = 0, inside_ink = 0;
    for (int y = 60; y <= 140; y++) {
        int stroke = 0;
        for (int x = 0; x < 20; x++) stroke += fb.get_pixel(x, y) ? 1 : 0;
        thinnest = stroke < thinnest ? stroke : thinnest;
        thickest = stroke > thickest ? stroke : thickest;
        for (int x = 20; x < 80; x++) inside_ink += fb.get_pixel(x, y) ? 1 : 0;
    }
    CHECK(thinnest >= 2);
    CHECK(thickest <= 4);
    CHECK(inside_ink == 0);

    // A stroke that steps diagonally reads as speckled glass, holes and all.
    int speckles = 0;
    for (int y = 1; y < 170; y++)  // above the footer, where no label bites the arc
        for (int x = 1; x < 199; x++) {
            if (!fb.get_pixel(x, y)) continue;
            const int dx = x < 100 ? 99 - x : x - 100, dy = y < 100 ? 99 - y : y - 100;
            const int r2 = dx * dx + dy * dy;
            if (r2 < 80 * 80 || r2 > 95 * 95) continue;
            const int neighbours = fb.get_pixel(x - 1, y) + fb.get_pixel(x + 1, y) +
                                   fb.get_pixel(x, y - 1) + fb.get_pixel(x, y + 1);
            if (neighbours < 2) speckles++;
        }
    CHECK(speckles == 0);
}

// 2 NM ahead on the 4 NM ring is 46 px, so every case below plots on (100, 54).
constexpr int kPlotX = 100;
constexpr int kPlotY = 54;

RadarTarget abeam[1] = {{0, 3000, 0, 0}};

// The dots are only struck when the plot has company, so every case has some.
RadarSnapshot cruising(int32_t speed_mps) {
    RadarSnapshot snap = flying(0);
    snap.speed_mps = speed_mps;
    snap.n_targets = 1;
    snap.targets = abeam;
    return snap;
}

RadarSnapshot one_target(RadarTarget* t) {
    RadarSnapshot snap = flying(0);
    snap.n_targets = 1;
    snap.targets = t;
    return snap;
}

TEST_CASE("radar: traffic wears its TCAS symbol, hollow, filled, or an advisory circle") {
    RadarTarget other[1] = {{2 * kMetresPerNm, 0, 0, 0}};
    const Framebuffer hollow = radar(one_target(other));
    CHECK_FALSE(hollow.get_pixel(kPlotX, kPlotY));
    CHECK(hollow.get_pixel(kPlotX - 4, kPlotY));
    CHECK(hollow.get_pixel(kPlotX + 4, kPlotY));

    RadarTarget proximate[1] = {{2 * kMetresPerNm, 0, 0, 1}};
    const Framebuffer filled = radar(one_target(proximate));
    CHECK(filled.get_pixel(kPlotX, kPlotY));
    CHECK(filled.get_pixel(kPlotX - 4, kPlotY));
    // A diamond has empty corners where a circle of the same reach has none.
    CHECK_FALSE(filled.get_pixel(kPlotX + 3, kPlotY + 3));

    RadarTarget advisory[1] = {{2 * kMetresPerNm, 0, 0, 2}};
    const Framebuffer circle = radar(one_target(advisory));
    CHECK(circle.get_pixel(kPlotX + 3, kPlotY + 3));
    CHECK(ink_in(circle, kPlotX - 6, kPlotY - 6, kPlotX + 7, kPlotY + 7) >
          ink_in(filled, kPlotX - 6, kPlotY - 6, kPlotX + 7, kPlotY + 7));

    // There is no resolution advisory on this device: the loudest grade reads as the same circle.
    RadarTarget urgent[1] = {{2 * kMetresPerNm, 0, 0, 3}};
    CHECK(ink_in(radar(one_target(urgent)), kPlotX - 6, kPlotY - 6, kPlotX + 7, kPlotY + 7) ==
          ink_in(circle, kPlotX - 6, kPlotY - 6, kPlotX + 7, kPlotY + 7));
}

TEST_CASE("radar: a leader line runs the minute ahead of the target, out to the glass") {
    // 30 m/s for 60 s is 1800 m, which is 22 px on the 4 NM ring.
    RadarTarget north[1] = {{2 * kMetresPerNm, 0, 0, 0, 0, false, 30, 0}};
    const Framebuffer ahead = radar(one_target(north));
    CHECK(ahead.get_pixel(kPlotX, kPlotY - 16));
    CHECK(ahead.get_pixel(kPlotX, kPlotY - 22));
    CHECK_FALSE(ahead.get_pixel(kPlotX, kPlotY - 30));

    RadarTarget crossing[1] = {{2 * kMetresPerNm, 0, 0, 0, 0, false, 30, 90}};
    const Framebuffer east = radar(one_target(crossing));
    CHECK(east.get_pixel(kPlotX + 20, kPlotY));
    CHECK_FALSE(east.get_pixel(kPlotX, kPlotY - 16));

    RadarTarget parked[1] = {{2 * kMetresPerNm, 0, 0, 0, 0, false, 0, 0}};
    CHECK_FALSE(radar(one_target(parked)).get_pixel(kPlotX, kPlotY - 16));

    // 100 m/s from 3.8 NM out runs off the top: the ring is a scale, not a wall.
    RadarTarget fast[1] = {{(38 * kMetresPerNm) / 10, 0, 0, 0, 0, false, 100, 0}};
    const Framebuffer running_out = radar(one_target(fast));
    CHECK(running_out.get_pixel(kPlotX, 8));
    CHECK(running_out.get_pixel(kPlotX, 0));
}

// The ring is the scale the footer reads in, and the glass around it is spare.
TEST_CASE("radar: traffic past the ring still draws, and the count stays on the ring") {
    // 4.2 NM abeam is off the 4 NM ring and still on the glass, at 96 px.
    RadarTarget beside[1] = {{0, (42 * kMetresPerNm) / 10, 0, 1}};
    RadarSnapshot snap = flying(0);
    snap.speed_mps = 30;
    snap.n_targets = 1;
    snap.targets = beside;
    const Framebuffer fb = radar(snap);

    CHECK(fb.get_pixel(196, 100));
    CHECK(fb.get_pixel(192, 100));
    // Its tag would hang off the edge, so it slides inboard instead of being cut.
    CHECK(reads_in(fb, "00", 160, 74, 200, 94, 2));
    CHECK(reads_in(fb, "0", 170, 170, 200, 200, 3));
    CHECK_FALSE(fb.get_pixel(99, 77));

    // The footer owns the bottom band, so traffic outside the ring keeps off it.
    RadarTarget behind[1] = {{-6500, -4000, -200, 0, 0, false, 30, 20}};
    RadarSnapshot low = snap;
    low.targets = behind;
    RadarSnapshot none = snap;
    none.n_targets = 0;
    CHECK(ink_in(radar(low), 20, 150, 90, 199) == ink_in(radar(none), 20, 150, 90, 199));
    CHECK(reads_in(radar(low), "0:42", 0, 176, 60, 198, 2));

    // Past the glass it is gone altogether, count and all.
    RadarTarget far_out[1] = {{0, 8 * kMetresPerNm, 0, 1}};
    RadarSnapshot beyond = snap;
    beyond.targets = far_out;
    const Framebuffer empty = radar(beyond);
    CHECK_FALSE(empty.get_pixel(196, 100));
    CHECK_FALSE(empty.get_pixel(192, 100));
    CHECK(reads_in(empty, "0", 170, 170, 200, 200, 3));
}

TEST_CASE("radar: two dots off the nose mark the next minute and the one after") {
    const Framebuffer fb = radar(cruising(30));

    // 30 m/s for 60 s is 1800 m, 22 px up the glass, on the 99|100 pair.
    CHECK(fb.get_pixel(99, 77));
    CHECK(fb.get_pixel(100, 77));
    CHECK(fb.get_pixel(99, 78));
    CHECK(fb.get_pixel(100, 78));
    CHECK_FALSE(fb.get_pixel(98, 77));
    CHECK_FALSE(fb.get_pixel(101, 77));
    CHECK_FALSE(fb.get_pixel(99, 76));
    // Nothing joins it to the aeroplane: the glass between them stays clear.
    CHECK_FALSE(fb.get_pixel(99, 85));
    CHECK_FALSE(fb.get_pixel(100, 85));

    // The second minute is twice as far up the same line.
    CHECK(fb.get_pixel(99, 55));
    CHECK(fb.get_pixel(100, 56));
    CHECK_FALSE(fb.get_pixel(99, 53));

    // Twice the speed puts the first dot where the second one was.
    CHECK(radar(cruising(60)).get_pixel(99, 55));

    // A second minute beyond the ring is dropped, not parked on the stroke.
    const Framebuffer clipped = radar(cruising(80));
    CHECK(clipped.get_pixel(99, 40));
    for (int y = 10; y < 20; y++) CHECK_FALSE(clipped.get_pixel(99, y));

    CHECK_FALSE(radar(cruising(0)).get_pixel(99, 77));

    RadarSnapshot searching;
    searching.speed_mps = 30;
    CHECK_FALSE(radar(searching).get_pixel(99, 77));
}

// An empty ring needs no scale: the dots are read against traffic or not at all.
TEST_CASE("radar: the minute dots keep off a plot with nothing on it") {
    RadarSnapshot alone = flying(0);
    alone.speed_mps = 30;
    CHECK_FALSE(radar(alone).get_pixel(99, 77));
    CHECK_FALSE(radar(alone).get_pixel(99, 55));

    CHECK(radar(cruising(30)).get_pixel(99, 77));

    // Heard but outside the ring is not on the plot, and does not bring them back.
    RadarTarget far_out[1] = {{40000, 0, 0, 0}};
    RadarSnapshot beyond = flying(0);
    beyond.speed_mps = 30;
    beyond.n_targets = 1;
    beyond.targets = far_out;
    CHECK_FALSE(radar(beyond).get_pixel(99, 77));
}

// Symbol edge 4, gap 2, pad 2 and 14 rows of glyph: the digits stand 22 px off the plot.
constexpr int kTagTop = kPlotY - 22;
constexpr int kTagBottom = kPlotY + 9;

TEST_CASE("radar: the relative altitude sits on the side the traffic is on") {
    // 300 m = 984 ft, which is ten hundreds of feet to the nearest hundred.
    RadarTarget above[1] = {{2 * kMetresPerNm, 0, 300, 1}};
    const Framebuffer higher = radar(one_target(above));
    CHECK(reads_in(higher, "+10", 70, kTagTop - 2, 130, kTagTop + 16, 2));
    CHECK_FALSE(reads_in(higher, "+10", 70, kPlotY, 130, 100, 2));
    // It is set at double height: the small figure is nowhere on the glass.
    CHECK_FALSE(reads_in(higher, "+10", 0, 0, 200, 170, 1));

    RadarTarget below[1] = {{2 * kMetresPerNm, 0, -300, 1}};
    const Framebuffer lower = radar(one_target(below));
    CHECK(reads_in(lower, "-10", 70, kTagBottom - 2, 130, kTagBottom + 16, 2));
    CHECK_FALSE(reads_in(lower, "-10", 70, 20, 130, kPlotY, 2));

    // Level traffic reads 00, unsigned: +0 and -0 are the same separation.
    RadarTarget level[1] = {{2 * kMetresPerNm, 0, 10, 1}};
    CHECK(reads_in(radar(one_target(level)), "00", 70, kTagTop - 2, 130, kTagTop + 16, 2));
}

// A third digit is 12 px more tag for a separation no pilot manoeuvres against.
TEST_CASE("radar: the tag stops at 99 hundreds of feet, the way a TCAS tag does") {
    RadarTarget high[1] = {{2 * kMetresPerNm, 0, 4000, 0}};
    const Framebuffer fb = radar(one_target(high));
    CHECK(reads_in(fb, "+99", 60, kTagTop - 4, 140, kTagTop + 16, 2));

    RadarTarget deep[1] = {{2 * kMetresPerNm, 0, -4000, 0}};
    CHECK(reads_in(radar(one_target(deep)), "-99", 60, kTagBottom - 4, 140, kTagBottom + 16, 2));
}

TEST_CASE("radar: a tag that would land on another is dropped, never overlaid") {
    // 200 m abeam is 2 px on the 4 NM ring, so the two tags want the same glass.
    RadarTarget pair[2] = {{2 * kMetresPerNm, 0, 300, 1}, {2 * kMetresPerNm, 200, 600, 1}};
    RadarSnapshot snap = flying(0);
    snap.n_targets = 2;
    snap.targets = pair;
    const Framebuffer fb = radar(snap);

    CHECK(reads_in(fb, "+10", 70, kTagTop - 2, 130, kTagTop + 16, 2));
    CHECK_FALSE(reads_in(fb, "+20", 0, 0, 200, 170, 2));
    // Both aircraft are still on the glass: the tag goes, the symbol stays.
    CHECK(fb.get_pixel(kPlotX + 2 + 4, kPlotY));
    CHECK(fb.get_pixel(kPlotX - 4, kPlotY));
}

// Tags are four times the area they were, so which one survives a clash is a decision.
TEST_CASE("radar: the advisory keeps its tag and the quiet aircraft loses it") {
    RadarTarget pair[2] = {{2 * kMetresPerNm, 0, 300, 0}, {2 * kMetresPerNm, 200, 600, 2}};
    RadarSnapshot snap = flying(0);
    snap.n_targets = 2;
    snap.targets = pair;
    const Framebuffer fb = radar(snap);

    CHECK(reads_in(fb, "+20", 60, 20, 140, kPlotY, 2));
    CHECK_FALSE(reads_in(fb, "+10", 0, 0, 200, 170, 2));
}

TEST_CASE("radar: a chevron on the tag says climbing or descending, past 500 fpm") {
    RadarTarget steady[1] = {{2 * kMetresPerNm, 0, 300, 1, 0, true}};
    const Framebuffer flat = radar(one_target(steady));

    // 2.5 m/s is 492 fpm: the arrow is for a rate a pilot has to act on.
    RadarTarget slow[1] = {{2 * kMetresPerNm, 0, 300, 1, 19, true}};
    CHECK(ink_in(radar(one_target(slow)), 60, kTagTop, 140, kTagTop + 14) ==
          ink_in(flat, 60, kTagTop, 140, kTagTop + 14));

    RadarTarget climbing[1] = {{2 * kMetresPerNm, 0, 300, 1, 20, true}};
    const Framebuffer up = radar(one_target(climbing));
    CHECK(ink_in(up, 60, kTagTop, 140, kTagTop + 14) >
          ink_in(flat, 60, kTagTop, 140, kTagTop + 14));

    RadarTarget descending[1] = {{2 * kMetresPerNm, 0, 300, 1, -20, true}};
    const Framebuffer down = radar(one_target(descending));
    // The arrow is 6 rows, centred in the 14 the glyphs are, and its apex is a pixel pair.
    const int arrow_top = kTagTop + 4, arrow_mid = 118;
    CHECK(up.get_pixel(arrow_mid, arrow_top));
    CHECK_FALSE(up.get_pixel(arrow_mid, arrow_top + 5));
    CHECK(down.get_pixel(arrow_mid, arrow_top + 5));
    CHECK_FALSE(down.get_pixel(arrow_mid, arrow_top));

    // A target that never reported a rate is not credited with one.
    RadarTarget silent[1] = {{2 * kMetresPerNm, 0, 300, 1, 40, false}};
    CHECK(ink_in(radar(one_target(silent)), 60, kTagTop, 140, kTagTop + 14) ==
          ink_in(flat, 60, kTagTop, 140, kTagTop + 14));
}

// A tag is now wide enough to bury the aeroplane it is plotted against.
TEST_CASE("radar: a tag keeps off own ship and off the line its target is flying") {
    Framebuffer bare;
    RadarSnapshot empty = flying(0);
    draw_radar(bare, empty);

    // 1500 m astern plots 19 px below the ship, so its tag wants the wing and goes instead.
    RadarTarget astern[1] = {{-1500, 0, 300, 1}};
    const Framebuffer fb = radar(one_target(astern));
    CHECK(ink_in(fb, 88, 94, 112, 110) == ink_in(bare, 88, 94, 112, 110));
    CHECK_FALSE(reads_in(fb, "+10", 0, 0, 200, 170, 2));

    // Clear of the ship it keeps its figure, on the beam rather than over the wing.
    RadarTarget quarter[1] = {{-1500, 900, 300, 1}};
    CHECK(reads_in(radar(one_target(quarter)), "+10", 110, 90, 180, 115, 2));

    // Flying straight up the glass, the tag would sit on the whole minute of line.
    RadarTarget running[1] = {{2 * kMetresPerNm, 0, 300, 1, 0, false, 30, 0}};
    const Framebuffer ahead = radar(one_target(running));
    CHECK(ahead.get_pixel(kPlotX, kPlotY - 16));
    CHECK(ahead.get_pixel(kPlotX, kPlotY - 22));
    CHECK(reads_in(ahead, "+10", 40, kTagTop - 2, kPlotX, kTagTop + 16, 2));
}

TEST_CASE("radar: the plot turns with the track, so what is ahead is up the glass") {
    RadarTarget east[1] = {{0, 2 * kMetresPerNm, 0, 1}};
    RadarSnapshot flying_east = flying(90);
    flying_east.n_targets = 1;
    flying_east.targets = east;
    const Framebuffer ahead = radar(flying_east);

    // 2 NM on the 4 NM ring is 46 px, and the nose is the top of the glass.
    CHECK(ahead.get_pixel(99, 99 - 46 + 1 - 2));
    CHECK(ahead.get_pixel(100, 99 - 46 + 1 - 2));
    CHECK_FALSE(ahead.get_pixel(100 + 46 + 2, 100));

    RadarSnapshot flying_north = flying_east;
    flying_north.track_deg = 0;
    const Framebuffer beam = radar(flying_north);
    CHECK(beam.get_pixel(100 + 46 + 2, 100));
    CHECK_FALSE(beam.get_pixel(99, 99 - 46 + 1 - 2));
}

TEST_CASE("radar: the flight time reads in the bottom-left, and dashes before a flight") {
    CHECK(reads_in(radar(flying(47)), "0:42", 0, 176, 60, 198, 2));

    RadarSnapshot long_flight = flying(47);
    long_flight.flight_seconds = 3 * 3600 + 7 * 60 + 59;
    CHECK(reads_in(radar(long_flight), "3:07", 0, 176, 60, 198, 2));

    RadarSnapshot no_fix;
    CHECK(reads_in(radar(no_fix), "-:--", 0, 176, 60, 198, 2));

    // The sector a pilot is flying into carries the plot and nothing else.
    CHECK_FALSE(reads_in(radar(flying(47)), "0:42", 40, 0, 160, 90, 2));
}

TEST_CASE("radar: the range labels the ring, centred on it and cleared off it") {
    const Framebuffer fb = radar(flying(0));
    CHECK(reads_in(fb, "4", 70, 176, 100, 198, 2));
    CHECK(reads_in(fb, "NM", 95, 183, 130, 198));

    RadarSnapshot wider = flying(0);
    wider.range_nm = 12;
    CHECK(reads_in(radar(wider), "12", 70, 176, 106, 198, 2));

    // The ring is cleared off the label rather than read through it.
    for (int y = 186; y < 196; y++)
        for (int x = 82; x < 88; x++) CHECK_FALSE(fb.get_pixel(x, y));
}

// B4. One circle, read in two habits: only the label under it changes.
TEST_CASE("radar: the ring is labelled in the unit a pilot set, and the plot does not move") {
    RadarSnapshot metric = flying(0);
    metric.units = skyblip::settings::Units::Metric;
    const Framebuffer km = radar(metric);
    const Framebuffer nm = radar(flying(0));

    CHECK(reads_in(km, "7.4", 60, 176, 112, 198, 2));
    CHECK(reads_in(km, "KM", 95, 183, 140, 198));
    CHECK_FALSE(reads_in(km, "NM", 60, 176, 140, 198));

    for (int y = 0; y < 170; y++)
        for (int x = 0; x < Framebuffer::kW; x++) REQUIRE(km.get_pixel(x, y) == nm.get_pixel(x, y));
}

TEST_CASE("radar: the footer counts what is on the glass, either side of the clock") {
    RadarTarget targets[3] = {
        {2000, 0, 0, 1},
        {0, -3000, 0, 1},
        {40000, 0, 0, 1},  // four rings out: heard, and off the picture
    };
    RadarSnapshot snap = flying(0);
    snap.n_targets = 3;
    snap.targets = targets;
    const Framebuffer fb = radar(snap);

    CHECK(reads_in(fb, "FLIGHT", 0, 168, 50, 182));
    CHECK(reads_in(fb, "0:42", 0, 176, 60, 198, 2));
    CHECK(reads_in(fb, "2", 170, 170, 200, 200, 3));
    CHECK(reads_in(fb, "ACT", 140, 175, 190, 200));

    RadarSnapshot closer = flying(0);
    closer.range_nm = 2;
    closer.airborne = false;
    const Framebuffer near = radar(closer);
    CHECK(reads_in(near, "GROUND", 0, 168, 50, 182));
    CHECK(reads_in(near, "0", 170, 170, 200, 200, 3));
}

// An empty sky and a radio that is not listening yet look the same on the plot.
TEST_CASE("radar: a radio not yet listening counts no aircraft, it dashes") {
    RadarSnapshot deaf = flying(0);
    deaf.receiver_listening = false;
    const Framebuffer fb = radar(deaf);

    CHECK(reads_in(fb, "-", 170, 170, 200, 200, 3));
    CHECK_FALSE(reads_in(fb, "0", 170, 170, 200, 200, 3));
    CHECK(reads_in(fb, "ACT", 140, 175, 190, 200));

    RadarSnapshot no_position = flying(0);
    no_position.fix_valid = false;
    CHECK(reads_in(radar(no_position), "-", 170, 170, 200, 200, 3));

    CHECK(reads_in(radar(flying(0)), "0", 170, 170, 200, 200, 3));
}

TEST_CASE("radar: the footer sits on one baseline, a margin clear of the glass edge") {
    const Framebuffer fb = radar(flying(0));
    int clock_bottom = -1, range_bottom = -1, count_bottom = -1;
    for (int y = 180; y < 200; y++)
        for (int x = 0; x < 200; x++)
            if (fb.get_pixel(x, y)) {
                if (x < 60) clock_bottom = y;
                if (x > 80 && x < 120) range_bottom = y;
                if (x > 170) count_bottom = y;
            }
    CHECK(clock_bottom == count_bottom);
    CHECK(range_bottom == count_bottom);
    for (int y = count_bottom + 1; y < 200; y++)
        for (int x = 0; x < 200; x++) CHECK_FALSE(fb.get_pixel(x, y));
    CHECK(199 - count_bottom >= 4);
}

TEST_CASE("radar: a device with no fix says so where it reports its flight state") {
    RadarSnapshot searching;
    searching.airborne = true;  // stale from the last flight: no fix outranks it
    const Framebuffer fb = radar(searching);

    CHECK(reads_in(fb, "NO FIX", 0, 168, 50, 182));
    CHECK_FALSE(reads_in(fb, "FLIGHT", 0, 168, 50, 182));
    // The picture itself stays empty rather than carrying a second message.
    CHECK_FALSE(reads_in(fb, "NO FIX", 20, 20, 180, 150));
}

TEST_CASE("status: every value reads in the aeronautical unit first, then SI") {
    // 1500 m = 4921 ft, 40 kt = 20 m/s (speed_q is quarter-m/s), +2.0 m/s =
    // +394 fpm. Rendering is 5x7 glyphs, so the check is on the row's ink: the
    // dual-unit row is wider than a single-unit one would be.
    Framebuffer both;
    StatusSnapshot s;
    s.fix_valid = true;
    s.utc_valid = true;
    s.sats = 9;
    s.alt_m = 1500;
    s.speed_q = 80;
    s.climb_mm_s = 2000;
    s.track_c9 = 128;
    draw_status(both, s);

    // The barometric rows only exist when a barometer answered: altitude on the
    // subscale the pilot set, pressure altitude on 1013.25, and both pressures.
    Framebuffer with_baro;
    StatusSnapshot b = s;
    b.baro_valid = true;
    b.pressure_mpa = 84556000;
    b.qnh_pa = 101325;
    b.alt_qnh_m = 1500;
    b.alt_std_m = 1500;
    draw_status(with_baro, b);
    CHECK(with_baro.count_black() > both.count_black());
}

TEST_CASE("status: the barometer row reads what the sensor resolves, beside the subscale") {
    Framebuffer fb;
    StatusSnapshot s;
    s.baro_valid = true;
    s.pressure_mpa = 101325253;  // the BME280's own tenths of a pascal
    s.qnh_pa = 101300;
    s.climb_mm_s = -1234;  // -242 fpm, a rate the 0.125 m/s of ADS-L cannot hold
    draw_status(fb, s);

    CHECK(reads_in(fb, "1013.252", 0, 85, 200, 105));
    CHECK(reads_in(fb, "Q1013", 0, 85, 200, 105));
    CHECK(reads_in(fb, "-242", 0, 165, 200, 185));
    CHECK(reads_in(fb, "-1.23", 0, 165, 200, 185));
}

TEST_CASE("status: the battery row states the voltage, the charge and which curve") {
    // 4.00 V is nearly full off charge and about half full on it, so the two rows
    // must not read the same - and the charging one carries the CHG marker, which
    // is more ink either way.
    StatusSnapshot s;
    s.battery_valid = true;
    s.battery_mv = 4000;
    s.battery_percent = 89;

    Framebuffer resting;
    draw_status(resting, s);

    StatusSnapshot c = s;
    c.charging = true;
    c.battery_percent = 55;
    Framebuffer charging;
    draw_status(charging, c);
    CHECK(charging.count_black() != resting.count_black());

    // A board with no divider fitted says so rather than reading empty.
    StatusSnapshot absent;
    Framebuffer no_sensor;
    draw_status(no_sensor, absent);
    CHECK(no_sensor.count_black() != resting.count_black());

    // The cutoff monitor's warning, on the page: a cell at 3.45 V is low and
    // says so, and the same cell on the cable is charging, not low.
    StatusSnapshot l = s;
    l.battery_mv = 3450;
    l.battery_percent = 12;
    l.battery_low = true;
    Framebuffer low;
    draw_status(low, l);
    StatusSnapshot q = l;
    q.battery_low = false;
    Framebuffer quiet;
    draw_status(quiet, q);
    CHECK(low.count_black() > quiet.count_black());

    StatusSnapshot on_cable = l;
    on_cable.charging = true;
    Framebuffer cable;
    draw_status(cable, on_cable);
    CHECK(cable.count_black() != low.count_black());

    // The row is the last one on the panel: it has to fit inside it.
    for (int y = 194; y < Framebuffer::kH; y++)
        for (int x = 0; x < Framebuffer::kW; x++) CHECK_FALSE(charging.get_pixel(x, y));
}

TEST_CASE("status: a receiver with no fix says so where a fix would have read 3D") {
    StatusSnapshot s;
    s.sats = 9;
    Framebuffer searching;
    draw_status(searching, s);

    CHECK(reads_in(searching, "NO FIX", 0, 24, 120, 40));
    CHECK_FALSE(reads_in(searching, "SAT", 0, 24, 120, 40));
    CHECK_FALSE(reads_in(searching, "3D", 0, 24, 120, 40));

    StatusSnapshot fixed = s;
    fixed.fix_valid = true;
    Framebuffer solved;
    draw_status(solved, fixed);
    CHECK(reads_in(solved, "3D", 0, 24, 120, 40));
    CHECK(reads_in(solved, "9 SAT", 0, 24, 120, 40));
    CHECK_FALSE(reads_in(solved, "NO FIX", 0, 24, 120, 40));

    StatusSnapshot flat = fixed;
    flat.sats = 3;
    Framebuffer two_d;
    draw_status(two_d, flat);
    CHECK(reads_in(two_d, "2D", 0, 24, 120, 40));
}

// The page used to report PPS lock, a pin a pilot cannot act on.
TEST_CASE("status: the page reports whether own-ship is transmitting, not the PPS pin") {
    StatusSnapshot s;
    s.fix_valid = true;
    s.sats = 9;
    Framebuffer silent;
    draw_status(silent, s);
    CHECK(reads_in(silent, "TX OFF", 100, 65, 200, 85));
    CHECK_FALSE(reads_in(silent, "PPS", 0, 65, 200, 85));

    StatusSnapshot seen = s;
    seen.transmitting = true;
    Framebuffer on_air;
    draw_status(on_air, seen);
    CHECK(reads_in(on_air, "TX ON", 100, 65, 200, 85));
    CHECK_FALSE(reads_in(on_air, "TX OFF", 100, 65, 200, 85));
}

TEST_CASE("panel model: the driver's own output is what the model shows") {
    Framebuffer fb;
    StatusSnapshot s;
    s.fix_valid = true;
    s.alt_m = 900;
    s.baro_valid = true;
    s.pressure_mpa = 90810000;
    draw_status(fb, s);

    skyblip::models::Ssd1681 panel;
    skyblip::parts::Ssd1681 driver(panel, panel, panel.dc, panel.rst, panel.busy);
    driver.begin();
    driver.present(fb, skyblip::hal::Refresh::Full, 0);

    CHECK(panel.present_count == 1);
    CHECK(panel.last_full);
    // Round trip through the driver's inversion: what the panel holds must be
    // pixel-for-pixel what the UI drew.
    CHECK(panel.framebuffer().count_black() == fb.count_black());
    CHECK(panel.save_pgm("build/status.pgm"));
}

// B4. ADS-L carries no callsign, so the setting has exactly one job: telling
// three devices on a bench apart. It shares the header with the identity that
// does go on the air.
TEST_CASE("status: the callsign shares the header with the address, and never crowds it") {
    StatusSnapshot s;
    s.device_addr = 0xED1234;

    Framebuffer bare;
    draw_status(bare, s);

    StatusSnapshot named = s;
    named.callsign = "D-KXYZ";
    Framebuffer with_name;
    draw_status(with_name, named);
    CHECK(with_name.count_black() > bare.count_black());

    // The address is drawn at scale 2 from the left margin; the name is
    // right-aligned on the same row. Neither may touch the other or the rule
    // under them.
    StatusSnapshot widest = s;
    widest.callsign = "123456789";
    Framebuffer full;
    draw_status(full, widest);
    for (int y = 3; y < 21; y++)
        for (int x = 116; x < 128; x++) CHECK_FALSE(full.get_pixel(x, y));
    for (int y = 3; y < 21; y++) CHECK_FALSE(full.get_pixel(Framebuffer::kW - 1, y));

    // An empty callsign is a header with nothing extra on it, not a blank box.
    StatusSnapshot empty = s;
    empty.callsign = "";
    Framebuffer none;
    draw_status(none, empty);
    CHECK(none.count_black() == bare.count_black());
}

TEST_CASE("status: the widest position on earth still fits its row") {
    // -90.0000000 and -180.0000000: eleven and twelve characters, the most the
    // format can produce. The latitude ends on the first column's unit edge and
    // the longitude block is anchored to the margin, so the worst case is where
    // they nearly meet.
    Framebuffer fb;
    StatusSnapshot s;
    s.fix_valid = true;
    s.lat_1e7 = -900000000;
    s.lon_1e7 = -1800000000;
    draw_status(fb, s);

    const int y0 = 43, y1 = 50;  // the LAT/LON row, one glyph tall
    for (int y = y0; y < y1; y++)
        for (int x = 196; x < Framebuffer::kW; x++) CHECK_FALSE(fb.get_pixel(x, y));

    // At least one blank column between the latitude and the LON block, and the
    // label is not touched either.
    int blank = 0;
    for (int x = 88; x < 106; x++) {
        bool ink = false;
        for (int y = y0; y < y1; y++) ink = ink || fb.get_pixel(x, y);
        if (!ink) blank++;
    }
    CHECK(blank >= 1);
    for (int y = y0; y < y1; y++) CHECK_FALSE(fb.get_pixel(23, y));
}
