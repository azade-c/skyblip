// The ownship symbol's placement is a bearing-accuracy property, not decoration:
// bbox-centring it once dragged the wing 4 px forward, which put a target 20 px
// abeam 11 degrees off its true bearing. These pin the hot spot and the symmetry.
#include "doctest/doctest.h"
#include "ui/canvas.h"
#include "ui/widgets/skyship.h"
#include "ui/widgets/stone.h"

using namespace skyblip::ui;

namespace {
using TestPanel = Panel<200, 200>;
constexpr int kCx = 100;
constexpr int kCy = 100;
constexpr int kSpan = 24;
constexpr int kOriginRow = 5;

// Inked rows of the wing band at one column: the chord the planform draws there.
int chord_at(const TestPanel& fb, int x) {
    int chord = 0;
    for (int y = kCy - 1; y <= kCy + 3; y++) chord += fb.get_pixel(x, y) ? 1 : 0;
    return chord;
}
}  // namespace

TEST_CASE("skyship: the hot spot lands on the point it was given") {
    TestPanel fb;
    fb.clear(/*white=*/true);
    draw_skyship(fb, kCx, kCy);

    // The wing's full-span row IS the hot spot, so it must be inked at cy.
    CHECK(fb.get_pixel(kCx, kCy));
    CHECK(fb.get_pixel(kCx - 1, kCy));
    // And nothing may sit above the nose or below the tail.
    for (int x = 0; x < TestPanel::kW; x++) {
        CHECK_FALSE(fb.get_pixel(x, kCy - kOriginRow - 1));
        CHECK_FALSE(fb.get_pixel(x, kCy - kOriginRow + 16));
    }
}

TEST_CASE("skyship: the sprite is symmetric about the centre pixel pair") {
    TestPanel fb;
    fb.clear(true);
    draw_skyship(fb, kCx, kCy);

    for (int y = kCy - kOriginRow; y < kCy - kOriginRow + 16; y++)
        for (int d = 0; d < kSpan / 2; d++)
            CHECK(fb.get_pixel(kCx + d, y) == fb.get_pixel(kCx - 1 - d, y));
}

// The trailing taper was once deleted as ink aft of the hot spot, leaving a slab.
TEST_CASE("skyship: the wing is elliptic, deepest at the root") {
    TestPanel fb;
    fb.clear(true);
    draw_skyship(fb, kCx, kCy);

    const int tip = chord_at(fb, kCx - kSpan / 2);
    const int mid = chord_at(fb, kCx - kSpan / 2 + 3);
    const int root = chord_at(fb, kCx - kSpan / 2 + 6);
    CHECK(tip < mid);
    CHECK(mid < root);
}

TEST_CASE("skyship: it draws the same aircraft wherever it is asked to") {
    TestPanel a, b;
    a.clear(true);
    b.clear(true);
    draw_skyship(a, kCx, kCy);
    draw_skyship(b, kCx + 20, kCy + 20);
    CHECK(a.count_black() == b.count_black());
    CHECK(a.count_black() > 40);
}

// The stone says above or below, inside the advisory's window or not, and which way it is going.
namespace {
int ink(const TestPanel& fb) {
    int n = 0;
    for (int y = 0; y < TestPanel::kH; y++)
        for (int x = 0; x < TestPanel::kW; x++) n += fb.get_pixel(x, y) ? 1 : 0;
    return n;
}

TestPanel stone(Cut cut, Band band, Trend trend, bool alarm = false) {
    TestPanel fb;
    fb.clear(/*white=*/true);
    draw_stone(fb, kCx, kCy, cut, band, trend, alarm);
    return fb;
}
}  // namespace

TEST_CASE("stone: the pavilion is the crown, flipped about the plot point") {
    for (const Band band : {Band::Near, Band::Far})
        for (const Trend trend : {Trend::Climbing, Trend::Level, Trend::Sinking}) {
            const Trend opposite = trend == Trend::Climbing  ? Trend::Sinking
                                   : trend == Trend::Sinking ? Trend::Climbing
                                                             : Trend::Level;
            const TestPanel up = stone(Cut::Crown, band, trend);
            const TestPanel down = stone(Cut::Pavilion, band, opposite);
            for (int y = 0; y < TestPanel::kH; y++)
                for (int x = 0; x < TestPanel::kW; x++)
                    CHECK(up.get_pixel(x, y) == down.get_pixel(x, 2 * kCy - y));
        }
}

TEST_CASE("stone: the caret keeps its place whatever the stone under it is") {
    const int expected_above = stone_above(Cut::Crown, Band::Near, Trend::Climbing);
    const int expected_below = stone_below(Cut::Crown, Band::Near, Trend::Sinking);
    for (const Cut cut : {Cut::Crown, Cut::Diamond, Cut::Pavilion}) {
        CHECK(stone_above(cut, Band::Near, Trend::Climbing) == expected_above);
        CHECK(stone_below(cut, Band::Near, Trend::Sinking) == expected_below);
    }
    // The caret pair is symmetric about the plot point, so a flipped body keeps both places.
    CHECK(stone(Cut::Pavilion, Band::Near, Trend::Climbing).get_pixel(kCx, kCy - expected_above));
    CHECK(stone(Cut::Pavilion, Band::Near, Trend::Sinking).get_pixel(kCx, kCy + expected_below));
}

TEST_CASE("stone: the far band is the smaller stone, and carries no less meaning") {
    CHECK(stone_above(Cut::Crown, Band::Far, Trend::Level) <
          stone_above(Cut::Crown, Band::Near, Trend::Level));
    CHECK(stone_beside(Band::Far) < stone_beside(Band::Near));
    CHECK(ink(stone(Cut::Crown, Band::Far, Trend::Climbing)) <
          ink(stone(Cut::Crown, Band::Near, Trend::Climbing)));
    // Still a crown: apex on the centreline, girdle under the plot point.
    CHECK(stone(Cut::Crown, Band::Far, Trend::Level)
              .get_pixel(kCx, kCy - stone_above(Cut::Crown, Band::Far, Trend::Level)));
    CHECK(stone(Cut::Crown, Band::Far, Trend::Level).get_pixel(kCx, kCy + 1));
}

TEST_CASE("stone: the alarm fills the stone it was already drawing") {
    const TestPanel hollow = stone(Cut::Diamond, Band::Near, Trend::Level);
    const TestPanel filled = stone(Cut::Diamond, Band::Near, Trend::Level, /*alarm=*/true);
    CHECK_FALSE(hollow.get_pixel(kCx, kCy));
    CHECK(filled.get_pixel(kCx, kCy));
    CHECK(ink(filled) > ink(hollow));
    // Filling is ink, never reach: an advisory must not move a tag off its target.
    for (const Cut cut : {Cut::Crown, Cut::Diamond, Cut::Pavilion}) {
        const TestPanel a = stone(cut, Band::Near, Trend::Climbing);
        const TestPanel b = stone(cut, Band::Near, Trend::Climbing, /*alarm=*/true);
        for (int y = 0; y < TestPanel::kH; y++)
            for (int x = 0; x < TestPanel::kW; x++)
                if (a.get_pixel(x, y)) CHECK(b.get_pixel(x, y));
    }
}
