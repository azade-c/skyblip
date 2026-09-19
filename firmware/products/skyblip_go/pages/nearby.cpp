#include "products/skyblip_go/pages/nearby.h"

#include "core/model/aircraft.h"
#include "core/units/units.h"
#include "core/util/format.h"
#include "core/util/intmath.h"

namespace skyblip::go {

namespace {
constexpr int kSmallCellW = 6;
constexpr int kGlyphH = 7;
constexpr int kTitleScale = 2;
constexpr int kCountScale = 2;
constexpr int kWordGap = 3;
constexpr int kRuleY = 32;
constexpr int kRelHundredsCap = 99;
constexpr int32_t kSlantE1Cap = 999;

void right_aligned(ui::Canvas& fb, int x_end, int y, const char* text, int len, int scale) {
    fb.draw_text(x_end - len * kSmallCellW * scale, y, text, true, scale);
}

void right_aligned_text(ui::Canvas& fb, int x_end, int y, const char* text, int scale) {
    int n = 0;
    while (text[n]) n++;
    right_aligned(fb, x_end, y, text, n, scale);
}

void draw_count(ui::Canvas& fb, int n_heard) {
    char buf[8];
    const int n = fmt_uint(buf, static_cast<uint32_t>(n_heard), 1);
    buf[n] = 0;
    right_aligned(fb, kNearbyRelEnd, kNearbyTitleY, buf, n, kCountScale);

    const int word_end = kNearbyRelEnd - n * kSmallCellW * kCountScale - kWordGap;
    right_aligned_text(fb, word_end, kNearbyTitleY + kGlyphH * (kCountScale - 1), "TRAFFIC", 1);
}

void banner(ui::Canvas& fb, const char* text) {
    int n = 0;
    while (text[n]) n++;
    fb.draw_text((kGlassW - n * kSmallCellW * kNearbyScale) / 2, kNearbyBannerY, text, true,
                 kNearbyScale);
}

void draw_header(ui::Canvas& fb, const NearbySnapshot& snap) {
    fb.draw_text(kNearbyIdX, kNearbyTitleY, "NEARBY", true, kTitleScale);
    draw_count(fb, snap.n_heard);

    fb.draw_text(kNearbyIdX, kNearbyHeadY, "SRC ID", true, 1);
    right_aligned_text(fb, kNearbySlantEnd, kNearbyHeadY, "SLANT", 1);
    right_aligned_text(fb, kNearbyRelEnd, kNearbyHeadY, "ALT", 1);
    fb.hline(kNearbyIdX, kRuleY, kNearbyRelEnd - kNearbyIdX, true);
}

int32_t rel_hundreds_of_feet(int32_t up_m) {
    const int32_t feet = to_feet(Metres(up_m)).v;
    const int32_t rounded = (feet >= 0 ? feet + 50 : feet - 50) / 100;
    if (rounded > kRelHundredsCap) return kRelHundredsCap;
    if (rounded < -kRelHundredsCap) return -kRelHundredsCap;
    return rounded;
}

void draw_row(ui::Canvas& fb, int y, const traffic::RangeRow& row, bool metric) {
    char buf[16];

    buf[0] = model::source_letter(row.source);
    buf[1] = ' ';
    const int n_id = 2 + fmt_hex(buf + 2, row.addr, 6);
    buf[n_id] = 0;
    fb.draw_text(kNearbyIdX, y, buf, true, kNearbyScale);

    const int32_t range_e1 = metric ? div_round(row.slant_m, 100) : to_nm_e1(Metres(row.slant_m)).v;
    int n = range_e1 > kSlantE1Cap ? fmt_string(buf, "FAR")
                                   : fmt_uint(buf, static_cast<uint32_t>(range_e1), 2, 1);
    buf[n] = 0;
    right_aligned(fb, kNearbySlantEnd, y, buf, n, kNearbyScale);

    const int32_t rel = rel_hundreds_of_feet(row.up_m);
    n = fmt_int(buf, rel, 2, 0, rel == 0);
    buf[n] = 0;
    right_aligned(fb, kNearbyRelEnd, y, buf, n, kNearbyScale);
}

}  // namespace

void draw_nearby(ui::Canvas& fb, const NearbySnapshot& snap) {
    draw_header(fb, snap);

    if (!snap.fix_valid) {
        banner(fb, "NO FIX");
        return;
    }
    if (snap.n_rows == 0 || snap.rows == nullptr) {
        banner(fb, "NO TRAFFIC");
        return;
    }

    const int rows = snap.n_rows < kNearbyRows ? snap.n_rows : kNearbyRows;
    const bool metric = snap.units == go::Units::Metric;
    for (int i = 0; i < rows; i++)
        draw_row(fb, kNearbyFirstRowY + i * kNearbyLineH, snap.rows[i], metric);
}

}  // namespace skyblip::go
