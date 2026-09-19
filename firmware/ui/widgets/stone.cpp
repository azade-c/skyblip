#include "ui/widgets/stone.h"

#include <algorithm>

namespace skyblip::ui {

namespace {

struct Sprite {
    const uint32_t* rows;
    int w;
    int h;
    int dx;
    int dy;
};

constexpr uint32_t kNearCrownRows[] = {
    0x00000100,  // ........#........
    0x00000380,  // .......###.......
    0x000007c0,  // ......#####......
    0x00000ee0,  // .....###.###.....
    0x00001c70,  // ....###...###....
    0x00003838,  // ...###.....###...
    0x0000701c,  // ..###.......###..
    0x0000fffe,  // .###############.
    0x0001ffff,  // #################
};

constexpr uint32_t kNearDiamondRows[] = {
    0x00000080,  // .......#.......
    0x000001c0,  // ......###......
    0x000003e0,  // .....#####.....
    0x00000770,  // ....###.###....
    0x00000e38,  // ...###...###...
    0x00001c1c,  // ..###.....###..
    0x0000380e,  // .###.......###.
    0x00007007,  // ###.........###
    0x0000380e,  // .###.......###.
    0x00001c1c,  // ..###.....###..
    0x00000e38,  // ...###...###...
    0x00000770,  // ....###.###....
    0x000003e0,  // .....#####.....
    0x000001c0,  // ......###......
    0x00000080,  // .......#.......
};

constexpr uint32_t kNearCaretRows[] = {
    0x00000080,  // .......#.......
    0x000001c0,  // ......###......
    0x00000360,  // .....##.##.....
    0x00000630,  // ....##...##....
    0x00000c18,  // ...##.....##...
    0x0000180c,  // ..##.......##..
    0x00003006,  // .##.........##.
    0x00006003,  // ##...........##
};

constexpr uint32_t kFarCrownRows[] = {
    0x00000040,  // ......#......
    0x000000e0,  // .....###.....
    0x000001b0,  // ....##.##....
    0x00000318,  // ...##...##...
    0x0000060c,  // ..##.....##..
    0x00000c06,  // .##.......##.
    0x00001fff,  // #############
};

constexpr uint32_t kFarCaretRows[] = {
    0x00000020,  // .....#.....
    0x00000070,  // ....###....
    0x000000d8,  // ...##.##...
    0x0000018c,  // ..##...##..
    0x00000306,  // .##.....##.
    0x00000603,  // ##.......##
};

constexpr Sprite kNearCrown{kNearCrownRows, 17, 9, -8, -7};
constexpr Sprite kNearDiamond{kNearDiamondRows, 15, 15, -7, -7};
constexpr Sprite kNearCaret{kNearCaretRows, 15, 8, -7, -11};
constexpr Sprite kFarCrown{kFarCrownRows, 13, 7, -6, -5};
constexpr Sprite kFarCaret{kFarCaretRows, 11, 6, -5, -9};

const Sprite& stone_of(Cut cut, Band band) {
    if (cut == Cut::Diamond) return kNearDiamond;
    return band == Band::Near ? kNearCrown : kFarCrown;
}

const Sprite& caret_of(Band band) { return band == Band::Near ? kNearCaret : kFarCaret; }

bool upside_down(Cut cut) { return cut == Cut::Pavilion; }

int top_row(const Sprite& s, bool flip) { return flip ? -(s.dy + s.h - 1) : s.dy; }

int bottom_row(const Sprite& s, bool flip) { return top_row(s, flip) + s.h - 1; }

void blit(Canvas& fb, int x, int y, const Sprite& s, bool flip, bool fill) {
    for (int row = 0; row < s.h; row++) {
        const uint32_t bits = s.rows[flip ? s.h - 1 - row : row];
        if (bits == 0) continue;
        const int py = y + top_row(s, flip) + row;
        int first = 0, last = s.w - 1;
        while (first < s.w && !(bits & (1u << first))) first++;
        while (last > first && !(bits & (1u << last))) last--;
        for (int col = first; col <= last; col++)
            if (fill || (bits & (1u << col))) fb.set_pixel(x + s.dx + col, py, true);
    }
}

bool caret_above(Trend trend) { return trend == Trend::Climbing; }

}  // namespace

void draw_stone(Canvas& fb, int x, int y, Cut cut, Band band, Trend trend, bool alarm) {
    const bool flip = upside_down(cut);
    blit(fb, x, y, stone_of(cut, band), flip, alarm);
    if (trend == Trend::Level) return;
    blit(fb, x, y, caret_of(band), !caret_above(trend), false);
}

int stone_above(Cut cut, Band band, Trend trend) {
    const bool flip = upside_down(cut);
    int above = -top_row(stone_of(cut, band), flip);
    if (caret_above(trend)) above = std::max(above, -top_row(caret_of(band), false));
    return above;
}

int stone_below(Cut cut, Band band, Trend trend) {
    const bool flip = upside_down(cut);
    int below = bottom_row(stone_of(cut, band), flip);
    if (trend == Trend::Sinking) below = std::max(below, bottom_row(caret_of(band), true));
    return below;
}

int stone_beside(Band band) { return stone_of(Cut::Crown, band).w / 2 + 1; }

}  // namespace skyblip::ui
