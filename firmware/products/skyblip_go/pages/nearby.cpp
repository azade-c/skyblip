#include "products/skyblip_go/pages/nearby.h"

#include "core/model/aircraft.h"
#include "core/units/units.h"
#include "core/util/format.h"

namespace skyblip::go {

namespace {
constexpr int kSmallCellW = 6;
constexpr int kTitleY = 2;
constexpr int kRuleY = 31;
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

void draw_header(ui::Canvas& fb, const NearbySnapshot& snap) {
    fb.draw_text(kNearbyIdX, kTitleY, "NEARBY", true, 1);

    char buf[16];
    int n = fmt_uint(buf, static_cast<uint32_t>(snap.n_heard), 1);
    n += fmt_string(buf + n, " HEARD");
    buf[n] = 0;
    right_aligned(fb, kNearbyRelEnd, kTitleY, buf, n, 1);

    fb.draw_text(kNearbyIdX, kNearbyHeaderY, "ID", true, 1);
    right_aligned_text(fb, kNearbySlantEnd, kNearbyHeaderY, "SLANT", 1);
    right_aligned_text(fb, kNearbyRelEnd, kNearbyHeaderY, "REL", 1);

    right_aligned_text(fb, kNearbySlantEnd, kNearbyUnitsY,
                       snap.units == go::Units::Metric ? "km" : "NM", 1);
    right_aligned_text(fb, kNearbyRelEnd, kNearbyUnitsY, "100FT", 1);
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

    const int32_t range_e1 = metric ? row.slant_m / 100 : to_nm_e1(Metres(row.slant_m)).v;
    int n = range_e1 > kSlantE1Cap ? fmt_string(buf, "FAR")
                                   : fmt_uint(buf, static_cast<uint32_t>(range_e1), 2, 1);
    buf[n] = 0;
    right_aligned(fb, kNearbySlantEnd, y, buf, n, kNearbyScale);

    const int32_t rel = rel_hundreds_of_feet(row.up_m);
    n = rel == 0 ? fmt_string(buf, "0") : fmt_int(buf, rel, 1, 0, false);
    buf[n] = 0;
    right_aligned(fb, kNearbyRelEnd, y, buf, n, kNearbyScale);
}

}  // namespace

void draw_nearby(ui::Canvas& fb, const NearbySnapshot& snap) {
    draw_header(fb, snap);

    if (!snap.fix_valid) {
        fb.draw_text(kNearbyIdX, kNearbyFirstRowY, "NO FIX: NO RANGE", true, 1);
        return;
    }
    if (snap.n_rows == 0 || snap.rows == nullptr) {
        fb.draw_text(kNearbyIdX, kNearbyFirstRowY, "NOTHING POSITIONED", true, 1);
        return;
    }

    const int rows = snap.n_rows < kNearbyRows ? snap.n_rows : kNearbyRows;
    const bool metric = snap.units == go::Units::Metric;
    for (int i = 0; i < rows; i++)
        draw_row(fb, kNearbyFirstRowY + i * kNearbyLineH, snap.rows[i], metric);
}

}  // namespace skyblip::go
