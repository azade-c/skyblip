// Read against a pilot's habit, not pixels: two dials on one value looks right and lies.
#include "doctest/doctest.h"
#include "ui/framebuffer.h"
#include "ui/screens/sixpack.h"

using namespace skyblip::ui;

namespace {
// The six dial centres, in the order the panel draws them.
struct Tile {
    int cx, cy;
};
const Tile kTiles[6] = {{34, 67}, {100, 67}, {166, 67}, {34, 133}, {100, 133}, {166, 133}};

// The number sits outside the glass: above the top row, below the bottom one.
const int kValueScale = 2;
int value_y(const Tile& t) { return t.cy < 100 ? t.cy - 31 - 7 - 7 * kValueScale : t.cy + 31 + 7; }

int black_in(const Framebuffer& fb, Tile t, int r) {
    int n = 0;
    for (int y = t.cy - r; y <= t.cy + r; y++)
        for (int x = t.cx - r; x <= t.cx + r; x++)
            if (fb.get_pixel(x, y)) n++;
    return n;
}

// The number under a dial, matched as the ink the page would draw for it.
bool value_matches(const Framebuffer& fb, Tile t, const char* text) {
    const int y0 = value_y(t);
    Framebuffer expected;
    expected.clear(true);
    int n = 0;
    while (text[n]) n++;
    expected.draw_text(t.cx - (n * 6 * kValueScale) / 2, y0, text, true, kValueScale);
    for (int y = y0; y < y0 + 7 * kValueScale; y++)
        for (int x = t.cx - 32; x <= t.cx + 32; x++)
            if (fb.get_pixel(x, y) != expected.get_pixel(x, y)) return false;
    return true;
}

bool ink_differs(const Framebuffer& a, const Framebuffer& b, Tile t) {
    for (int y = t.cy - 30; y <= t.cy + 30; y++)
        for (int x = t.cx - 30; x <= t.cx + 30; x++)
            if (a.get_pixel(x, y) != b.get_pixel(x, y)) return true;
    return false;
}

SixPackSnapshot flying() {
    SixPackSnapshot s;
    s.have_data = true;
    s.units = skyblip::settings::Units::Imperial;
    s.speed_kt = 90;
    s.alt_ft = 3450;
    s.vs_fpm = 500;
    s.track_deg = 270;
    s.turn_dps = 3;
    s.qnh_pa = 101900;
    s.set_qnh_pa = 101300;
    return s;
}
}  // namespace

TEST_CASE("sixpack: six dials are drawn, each with its own needle") {
    const SixPackSnapshot s = flying();

    Framebuffer fb;
    draw_sixpack(fb, s);
    for (const Tile& t : kTiles) CHECK(black_in(fb, t, 30) > 60);

    // Without a fix the dials stay, the needles go: every tile must lose ink.
    SixPackSnapshot none;
    Framebuffer empty;
    draw_sixpack(empty, none);
    for (const Tile& t : kTiles) CHECK(black_in(empty, t, 30) < black_in(fb, t, 30));
}

TEST_CASE("sixpack: needles move with the data they show") {
    SixPackSnapshot a;
    a.have_data = true;
    a.units = skyblip::settings::Units::Imperial;
    a.speed_kt = 40;
    a.alt_ft = 1200;
    a.track_deg = 0;
    SixPackSnapshot b = a;
    b.speed_kt = 140;
    b.alt_ft = 1900;
    b.track_deg = 180;

    Framebuffer fa, fbuf;
    draw_sixpack(fa, a);
    draw_sixpack(fbuf, b);

    bool differs[6] = {};
    for (int i = 0; i < 6; i++) {
        for (int y = kTiles[i].cy - 28; y <= kTiles[i].cy + 28 && !differs[i]; y++)
            for (int x = kTiles[i].cx - 28; x <= kTiles[i].cx + 28; x++)
                if (fa.get_pixel(x, y) != fbuf.get_pixel(x, y)) {
                    differs[i] = true;
                    break;
                }
    }
    CHECK(differs[0]);  // airspeed
    CHECK(differs[2]);  // altimeter
    CHECK(differs[4]);  // heading
}

TEST_CASE("sixpack: the altimeter reads like a three-pointer, the card like a compass") {
    SixPackSnapshot s;
    s.have_data = true;
    s.units = skyblip::settings::Units::Imperial;
    s.alt_ft = 2500;  // long hand at 500 ft (down), short hand at 2.5/10 (right)
    Framebuffer fb;
    draw_sixpack(fb, s);

    const Tile alt = kTiles[2];
    CHECK(fb.get_pixel(alt.cx, alt.cy + 20));        // hundreds hand, straight down
    CHECK(fb.get_pixel(alt.cx + 12, alt.cy));        // thousands hand, quarter turn
    CHECK_FALSE(fb.get_pixel(alt.cx + 20, alt.cy));  // and it is the SHORT one
    CHECK_FALSE(fb.get_pixel(alt.cx, alt.cy - 20));

    // The compass card turns with the track: north swings to the right when the
    // aircraft flies west.
    SixPackSnapshot west = s;
    west.track_deg = 270;
    Framebuffer fw;
    draw_sixpack(fw, west);
    const Tile hdg = kTiles[4];
    int right = 0, left = 0;
    for (int y = hdg.cy - 8; y <= hdg.cy + 8; y++)
        for (int d = 12; d <= 24; d++) {
            if (fw.get_pixel(hdg.cx + d, y)) right++;
            if (fw.get_pixel(hdg.cx - d, y)) left++;
        }
    CHECK(right > left);
}

// B4. A dial has one needle and one number, so the km/h pilot has to ask for it.
TEST_CASE("sixpack: the unit setting decides the speed dial, and only the speed dial") {
    SixPackSnapshot imperial;
    imperial.have_data = true;
    imperial.units = skyblip::settings::Units::Imperial;
    imperial.speed_kt = 90;  // 166 km/h
    imperial.alt_ft = 3450;
    imperial.vs_fpm = 500;
    imperial.track_deg = 7;
    SixPackSnapshot metric = imperial;
    metric.units = skyblip::settings::Units::Metric;

    Framebuffer fi, fm;
    draw_sixpack(fi, imperial);
    draw_sixpack(fm, metric);

    // The number under a dial is the converted one, drawn where the page draws
    // it: 90 kt reads 166, and it is not the same ink as 90.
    CHECK(value_matches(fi, kTiles[0], "90"));
    CHECK(value_matches(fm, kTiles[0], "166"));

    const Framebuffer* faces[2] = {&fi, &fm};
    for (const Framebuffer* fb : faces) {
        CHECK(value_matches(*fb, kTiles[2], "3450"));
        CHECK(value_matches(*fb, kTiles[5], "+500"));
        CHECK(value_matches(*fb, kTiles[4], "007"));
    }
    for (int i = 1; i < 6; i++) CHECK(black_in(fm, kTiles[i], 30) == black_in(fi, kTiles[i], 30));
}

TEST_CASE("sixpack: the subscale reads the pressure the two sensors agree on") {
    SixPackSnapshot s = flying();
    s.qnh_pa = 101900;
    Framebuffer fb;
    draw_sixpack(fb, s);
    CHECK(value_matches(fb, kTiles[1], "1019"));

    // 1010 hPa is the top of the scale, so its needle is the one pointing up.
    SixPackSnapshot top = s;
    top.qnh_pa = 101000;
    Framebuffer up;
    draw_sixpack(up, top);
    CHECK(value_matches(up, kTiles[1], "1010"));
    CHECK(up.get_pixel(kTiles[1].cx, kTiles[1].cy - 20));
    CHECK_FALSE(fb.get_pixel(kTiles[1].cx, kTiles[1].cy - 20));

    // A barometer and a fix that have not met yet: the scale stays, the answer does not.
    SixPackSnapshot unknown = s;
    unknown.qnh_pa = 0;
    Framebuffer none;
    draw_sixpack(none, unknown);
    CHECK(value_matches(none, kTiles[1], "---"));
    CHECK(black_in(none, kTiles[1], 30) < black_in(fb, kTiles[1], 30));
}

// The dial exists to show the disagreement, so the pilot's own setting is on it.
TEST_CASE("sixpack: the subscale carries the setting the pilot dialled in") {
    SixPackSnapshot low = flying();
    low.set_qnh_pa = 99000;
    SixPackSnapshot high = low;
    high.set_qnh_pa = 103000;

    Framebuffer fl, fh;
    draw_sixpack(fl, low);
    draw_sixpack(fh, high);
    CHECK(ink_differs(fl, fh, kTiles[1]));

    // It is the pilot's setting, not the derived one: the number does not move.
    CHECK(value_matches(fl, kTiles[1], "1019"));
    CHECK(value_matches(fh, kTiles[1], "1019"));

    SixPackSnapshot unset = low;
    unset.set_qnh_pa = 0;
    Framebuffer fu;
    draw_sixpack(fu, unset);
    CHECK(black_in(fu, kTiles[1], 30) < black_in(fl, kTiles[1], 30));
}
