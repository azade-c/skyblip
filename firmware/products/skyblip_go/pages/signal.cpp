#include "products/skyblip_go/pages/signal.h"

#include "core/model/aircraft.h"
#include "core/units/units.h"
#include "core/util/format.h"

namespace skyblip::go {

namespace {
constexpr int kLeft = 4;
constexpr int kCellW = 6;  // the 5x7 font's advance at scale 1
constexpr int kColumn(int cell) { return kLeft + cell * kCellW; }

constexpr int kTitleY = 2;
constexpr int kHeaderY = 18;
constexpr int kUnitsY = 28;
constexpr int kRuleY = 38;
constexpr int kFirstRowY = 42;
constexpr int kLineH = 16;

constexpr int kSourceX = kColumn(0);
constexpr int kIdX = kColumn(2);
constexpr int kSlantEnd = kColumn(13);
constexpr int kAltEnd = kColumn(19);
constexpr int kRssiEnd = kColumn(25);
constexpr int kErpEnd = kColumn(31);

void right_aligned(ui::Canvas& fb, int x_end, int y, const char* text, int len) {
    fb.draw_text(x_end - len * kCellW, y, text, true, 1);
}

void right_aligned_text(ui::Canvas& fb, int x_end, int y, const char* text) {
    int n = 0;
    while (text[n]) n++;
    right_aligned(fb, x_end, y, text, n);
}

void draw_header(ui::Canvas& fb, const SignalSnapshot& snap) {
    fb.draw_text(kLeft, kTitleY, "SIGNAL", true, 1);

    char buf[16];
    int n = fmt_uint(buf, static_cast<uint32_t>(snap.n_heard), 1);
    n += fmt_string(buf + n, " HEARD");
    buf[n] = 0;
    right_aligned(fb, kErpEnd, kTitleY, buf, n);

    // Slant range, because a radio wave travels the hypotenuse: the aircraft
    // 2 km overhead is no conflict at all and a 2 km path all the same. The
    // radar and the alarm mean horizontal distance by "distance"; this page
    // does not, so it spells the word out.
    fb.draw_text(kIdX, kHeaderY, "ID", true, 1);
    right_aligned_text(fb, kSlantEnd, kHeaderY, "SLANT");
    right_aligned_text(fb, kAltEnd, kHeaderY, "REL");
    right_aligned_text(fb, kRssiEnd, kHeaderY, "RSSI");
    right_aligned_text(fb, kErpEnd, kHeaderY, "ERP");

    right_aligned_text(fb, kSlantEnd, kUnitsY, snap.units == go::Units::Metric ? "km" : "NM");
    right_aligned_text(fb, kAltEnd, kUnitsY, "ft");
    right_aligned_text(fb, kRssiEnd, kUnitsY, "dBm");
    right_aligned_text(fb, kErpEnd, kUnitsY, "dBm");
    fb.hline(kLeft, kRuleY, kErpEnd - kLeft, true);
}

void draw_row(ui::Canvas& fb, int y, const traffic::LinkRow& row, bool metric) {
    char buf[16];

    buf[0] = model::source_letter(row.source);
    buf[1] = 0;
    fb.draw_text(kSourceX, y, buf, true, 1);

    fmt_hex(buf, row.addr, 4);
    buf[4] = 0;
    fb.draw_text(kIdX, y, buf, true, 1);

    const int32_t range_e1 = metric ? row.slant_m / 100 : to_nm_e1(Metres(row.slant_m)).v;
    int n = fmt_uint(buf, static_cast<uint32_t>(range_e1), 2, 1);
    buf[n] = 0;
    right_aligned(fb, kSlantEnd, y, buf, n);

    n = fmt_int(buf, to_feet(Metres(row.up_m)).v, 1, 0, false);
    buf[n] = 0;
    right_aligned(fb, kAltEnd, y, buf, n);

    n = fmt_int(buf, row.rssi_dbm, 1, 0, false);
    buf[n] = 0;
    right_aligned(fb, kRssiEnd, y, buf, n);

    if (row.modelled) {
        n = fmt_int(buf, row.implied_erp_dbm, 1, 0, false);
    } else {
        n = fmt_string(buf, "--");
    }
    buf[n] = 0;
    right_aligned(fb, kErpEnd, y, buf, n);
}

}  // namespace

void draw_signal(ui::Canvas& fb, const SignalSnapshot& snap) {
    draw_header(fb, snap);

    if (!snap.fix_valid) {
        fb.draw_text(kLeft, kFirstRowY + kLineH, "NO FIX: NO RANGE", true, 1);
        return;
    }
    if (snap.n_rows == 0 || snap.rows == nullptr) {
        fb.draw_text(kLeft, kFirstRowY + kLineH, "NOTHING POSITIONED", true, 1);
        return;
    }

    const int rows = snap.n_rows < kSignalRows ? snap.n_rows : kSignalRows;
    const bool metric = snap.units == go::Units::Metric;
    for (int i = 0; i < rows; i++) draw_row(fb, kFirstRowY + i * kLineH, snap.rows[i], metric);
}

}  // namespace skyblip::go
