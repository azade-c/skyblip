// The diagnostic page, where the discipline is what it refuses to state. An
// emitter that cannot be modelled shows its range and no verdict, no fix means no
// range at all, and the header counts every emitter heard rather than the handful
// of rows that fit. A page that quietly lists five of nine is a page that says the
// sky is emptier than it is.
#include "core/model/aircraft.h"
#include "doctest/doctest.h"
#include "products/skyblip_go/glass.h"
#include "products/skyblip_go/pages/signal.h"

using namespace skyblip;
using namespace skyblip::go;

namespace {

constexpr int kFirstRowY = 42;
constexpr int kLineH = 16;

// Ink inside one row band, so a row that was drawn can be told from one that
// was not without asserting on glyph shapes.
int ink_in_row(const Glass& fb, int index) {
    const int y0 = kFirstRowY + index * kLineH;
    int n = 0;
    for (int y = y0; y < y0 + 8; y++)
        for (int x = 0; x < Glass::kW; x++)
            if (fb.get_pixel(x, y)) n++;
    return n;
}

// A case claims a column reads "2.3", not that there is ink in it.
bool reads_right_of(const Glass& fb, int x_end, int y, const char* text) {
    int n = 0;
    while (text[n]) n++;
    Glass wanted;
    wanted.clear(true);
    wanted.draw_text(0, 0, text, true, 1);
    const int x0 = x_end - n * 6;
    for (int dy = 0; dy < 7; dy++)
        for (int dx = 0; dx < n * 6 - 1; dx++)
            if (fb.get_pixel(x0 + dx, y + dy) != wanted.get_pixel(dx, dy)) return false;
    return true;
}

int row_y(int index) { return kFirstRowY + index * kLineH; }

int ink_in_column(const Glass& fb, int index, int x0, int x1) {
    const int y0 = kFirstRowY + index * kLineH;
    int n = 0;
    for (int y = y0; y < y0 + 8; y++)
        for (int x = x0; x < x1; x++)
            if (fb.get_pixel(x, y)) n++;
    return n;
}

constexpr int kUnitsY = 28;
constexpr int kSlantEnd = 4 + 13 * 6;
constexpr int kAltEnd = 4 + 19 * 6;

SignalSnapshot listing(const traffic::LinkRow* rows, int n, go::Units units) {
    SignalSnapshot snap;
    snap.fix_valid = true;
    snap.units = units;
    snap.n_heard = n;
    snap.n_rows = n;
    snap.rows = rows;
    return snap;
}

traffic::LinkRow row_at(int32_t slant_m, int8_t rssi, int16_t erp, bool modelled) {
    traffic::LinkRow r;
    r.addr = 0xABCD;
    r.source = model::Source::AdslDirect;
    r.slant_m = slant_m;
    r.up_m = 120;
    r.rssi_dbm = rssi;
    r.implied_erp_dbm = erp;
    r.modelled = modelled;
    return r;
}

}  // namespace

TEST_CASE("signal: one row per emitter heard, and none for the rest") {
    traffic::LinkRow rows[3] = {row_at(400, -70, 14, true), row_at(4300, -92, 12, true),
                                row_at(9100, -101, 9, true)};
    SignalSnapshot snap;
    snap.fix_valid = true;
    snap.n_heard = 3;
    snap.n_rows = 3;
    snap.rows = rows;

    Glass fb;
    draw_signal(fb, snap);

    for (int i = 0; i < 3; i++) CHECK(ink_in_row(fb, i) > 20);
    CHECK(ink_in_row(fb, 3) == 0);
}

TEST_CASE("signal: an unmodelled emitter shows its range and withholds a verdict") {
    // The ERP column of a relayed or too-close emitter carries "--", which is
    // less ink than a signed number and is drawn in the same place.
    traffic::LinkRow modelled[1] = {row_at(4300, -92, 12, true)};
    traffic::LinkRow unmodelled[1] = {row_at(4300, -92, 0, false)};

    SignalSnapshot a;
    a.fix_valid = true;
    a.n_heard = 1;
    a.n_rows = 1;
    a.rows = modelled;
    SignalSnapshot b = a;
    b.rows = unmodelled;

    Glass fa, fb2;
    draw_signal(fa, a);
    draw_signal(fb2, b);

    const int x0 = 4 + 25 * 6;
    const int x1 = 4 + 31 * 6;
    CHECK(ink_in_column(fb2, 0, x0, x1) > 0);
    CHECK(ink_in_column(fb2, 0, x0, x1) < ink_in_column(fa, 0, x0, x1));
    // Everything left of the ERP column is identical: same range, same RSSI.
    CHECK(ink_in_column(fb2, 0, 0, x0) == ink_in_column(fa, 0, 0, x0));
}

TEST_CASE("signal: no fix means no range, and the page says so instead of listing") {
    traffic::LinkRow rows[1] = {row_at(4300, -92, 12, true)};
    SignalSnapshot snap;
    snap.fix_valid = false;
    snap.n_heard = 4;
    snap.n_rows = 1;
    snap.rows = rows;

    Glass fb;
    draw_signal(fb, snap);
    CHECK(ink_in_row(fb, 0) == 0);
    CHECK(ink_in_row(fb, 1) > 20);  // the reason, on the first free line
}

TEST_CASE("signal: the header counts what was heard, not what fits") {
    SignalSnapshot snap;
    snap.fix_valid = true;
    snap.n_heard = 26;
    snap.n_rows = 0;
    snap.rows = nullptr;

    Glass fb;
    draw_signal(fb, snap);
    int header_ink = 0;
    for (int y = 0; y < 38; y++)
        for (int x = 0; x < Glass::kW; x++)
            if (fb.get_pixel(x, y)) header_ink++;
    CHECK(header_ink > 40);
    CHECK(ink_in_row(fb, 0) == 0);
}

// B4. A pilot who asked for miles on the instruments is not handed kilometres here.
TEST_CASE("signal: the range column reads in the unit a pilot set") {
    traffic::LinkRow rows[1] = {row_at(4300, -92, 12, true)};  // 4.3 km, 2.3 NM

    Glass nautical, metric;
    draw_signal(nautical, listing(rows, 1, go::Units::Nautical));
    draw_signal(metric, listing(rows, 1, go::Units::Metric));

    CHECK(reads_right_of(nautical, kSlantEnd, row_y(0), "2.3"));
    CHECK(reads_right_of(nautical, kSlantEnd, kUnitsY, "NM"));
    CHECK(reads_right_of(metric, kSlantEnd, row_y(0), "4.3"));
    CHECK(reads_right_of(metric, kSlantEnd, kUnitsY, "km"));
}

// A separation is cleared and flown in feet wherever the aeroplane is.
TEST_CASE("signal: relative altitude reads in feet under either unit setting") {
    traffic::LinkRow rows[1] = {row_at(4300, -92, 12, true)};  // up 120 m, 394 ft

    Glass nautical, metric;
    draw_signal(nautical, listing(rows, 1, go::Units::Nautical));
    draw_signal(metric, listing(rows, 1, go::Units::Metric));

    CHECK(reads_right_of(nautical, kAltEnd, row_y(0), "+394"));
    CHECK(reads_right_of(metric, kAltEnd, row_y(0), "+394"));
    CHECK(reads_right_of(metric, kAltEnd, kUnitsY, "ft"));
}

TEST_CASE("signal: more emitters than rows are cut, never overdrawn") {
    traffic::LinkRow rows[kSignalRows + 4];
    for (int i = 0; i < kSignalRows + 4; i++)
        rows[i] = row_at(500 + 100 * i, static_cast<int8_t>(-70 - i), 14, true);

    SignalSnapshot snap;
    snap.fix_valid = true;
    snap.n_heard = kSignalRows + 4;
    snap.n_rows = kSignalRows + 4;
    snap.rows = rows;

    Glass fb;
    draw_signal(fb, snap);
    for (int i = 0; i < kSignalRows; i++) CHECK(ink_in_row(fb, i) > 20);
    for (int y = kFirstRowY + kSignalRows * kLineH; y < Glass::kH; y++)
        for (int x = 0; x < Glass::kW; x++) CHECK_FALSE(fb.get_pixel(x, y));
}
