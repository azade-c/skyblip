#include "products/skyblip_go/pages/radar.h"

#include "core/flight/arc.h"
#include "core/units/units.h"
#include "core/util/format.h"
#include "core/util/intmath.h"
#include "ui/widgets/skyship.h"

namespace skyblip::go {

namespace {
// The screen is 200x200, an EVEN grid: there is no middle pixel. The centre is
// the POINT where four pixels meet, so each axis has a near-side and a far-side
// middle pixel - 99 and 100. Everything on this screen is built around that
// point rather than around a pixel:
//
//   kNear = 99   the pixel just before the centre (left, and above)
//   kFar  = 100  the pixel just after it (right, and below)
//
// A feature at distance d from the centre therefore occupies kNear-(d-1) on one
// side and kFar+(d-1) on the other. A feature ON the centre is a PAIR of
// pixels, never one. That makes the own ship exactly centred (its fuselage
// straddles 99|100), the rings exactly concentric with it, and every target
// offset measured from the same point in both directions.
constexpr int kNear = kGlassW / 2 - 1;
constexpr int kFar = kGlassW / 2;
constexpr int kCx = kFar;  // only for centring text, which has no such nicety
constexpr int kMargin = 4;
constexpr int kOuterR = 92;
constexpr int kRingW = 2;
constexpr int kGlyphH = 7;
constexpr int kCellW = 6;
constexpr int kClockScale = 2;
constexpr int kSecondsScale = 1;
constexpr int kSecondsGap = 2;
constexpr int kRangeScale = 2;
constexpr int kStateScale = 1;
constexpr int kTrafficScale = 3;
constexpr int kRangePad = 6;
constexpr int kKeepOutPad = 3;
constexpr int kReadingCorner = 3;
constexpr int kLabelPad = 2;
constexpr int kStackGap = 2;
constexpr int kFooterBottom = kGlassH - kMargin;
constexpr int kFooterY = kFooterBottom - kGlyphH;
constexpr int kClockY = kFooterBottom - kGlyphH * kClockScale;
constexpr int kStateY = kClockY - kStackGap - kGlyphH * kStateScale;
constexpr int kRangeY = kFooterBottom - kGlyphH * kRangeScale;
constexpr int kUnitGap = 3;
constexpr int32_t kQ14One = 16384;
constexpr int32_t kTurn16 = 65536;
constexpr int kSymbolR = 4;
constexpr int kAdvisoryR = 5;
constexpr int kTagScale = 2;
constexpr int kTagGlyphH = kGlyphH * kTagScale;
constexpr int kTagGap = 2;
constexpr int kTagSideClear = kCellW * kTagScale;
constexpr int kTagPad = 2;
constexpr int kChevronW = 10;
constexpr int kChevronH = 6;
constexpr int kChevronStroke = 2;
constexpr int kChevronGap = 4;
constexpr int32_t kFeetPerTagUnit = 100;
constexpr int32_t kMaxTagHundreds = 99;
constexpr int16_t kChevronClimbE8 = 20;
constexpr int32_t kLeaderSeconds = 60;
constexpr uint32_t kLeaderStepMs = 5000;
constexpr int kLeaderSteps = static_cast<int>((kLeaderSeconds * 1000) / kLeaderStepMs);
constexpr uint32_t kMinuteStepMs = 10000;
constexpr int kStepsPerMinute = static_cast<int>((kLeaderSeconds * 1000) / kMinuteStepMs);
constexpr int32_t kSpeedQPerMps = 4;
constexpr int32_t kTrackC9Turn = 512;
constexpr int kMinLeaderPx = 3;
constexpr int kOwnNoseAhead = ui::kSkyshipRowsToNose + 1;
constexpr int kMinuteClearPx = kOwnNoseAhead + kMinLeaderPx;
constexpr int kFooterTop = kStateY - kLabelPad;
constexpr int kMinuteDotW = 2;
constexpr int kWedgeInnerR = 15;
constexpr int kWedgeOuterR = kOuterR - kRingW;
constexpr int64_t kTanScale = 10000;
constexpr int64_t kWedgeEdgeTanE4 = 10000;
constexpr int kFormationD = 13;
constexpr int kFormationCorner = 4;
constexpr int kFormationInset = 3;
constexpr int kDigitW = 5;
constexpr int kBannerScale = 2;
constexpr int kBannerPad = 4;
constexpr int kOwnShipTail = kNear - ui::kSkyshipRowsToNose + ui::kSkyshipRows;
constexpr int kBannerY = (kOwnShipTail + kFooterTop - kGlyphH * kBannerScale) / 2;
constexpr int kNoteScale = 1;
constexpr int kNotePad = 2;
constexpr int kNoteGap = 5;
constexpr int kNoteY = kBannerY + kGlyphH * kBannerScale + kBannerPad + kNoteGap;
constexpr int kMinutesMarked = 2;

int half_chord_in_half_pixels(int r, int b) {
    const int32_t v = 4 * r * r - (2 * b + 1) * (2 * b + 1);
    if (v < 0) return -1;
    int q = isqrt<int32_t>(v);
    while (q * q > v) q--;
    return (q - 1) / 2;
}

void ring(ui::Canvas& fb, int r) {
    for (int b = 0; b < r; b++) {
        const int outer = half_chord_in_half_pixels(r, b);
        if (outer < 0) continue;
        const int inner = half_chord_in_half_pixels(r - kRingW, b) + 1;
        const int w = outer - inner + 1;
        fb.hline(kFar + inner, kFar + b, w, true);
        fb.hline(kFar + inner, kNear - b, w, true);
        fb.hline(kNear - outer, kFar + b, w, true);
        fb.hline(kNear - outer, kNear - b, w, true);
    }
}

int16_t c16(int32_t deg) {
    int32_t d = ((deg % 360) + 360) % 360;
    if (d >= 180) d -= 360;  // keep the cordic value inside int16_t
    return static_cast<int16_t>((d * kTurn16) / 360);
}

struct HeadingUp {
    int32_t ahead;
    int32_t right;
};

HeadingUp heading_up(int32_t north, int32_t east, int16_t track) {
    const int64_t c = icos(track), s = isin(track);
    return {static_cast<int32_t>((north * c + east * s) / kQ14One),
            static_cast<int32_t>((east * c - north * s) / kQ14One)};
}

int px_of(int32_t right) { return right >= 0 ? kFar + right : kNear + right + 1; }
int py_of(int32_t ahead) { return ahead <= 0 ? kFar - ahead : kNear - ahead + 1; }

int text_width(const char* s, int scale) {
    int n = 0;
    while (s[n]) n++;
    return n * kCellW * scale - scale;
}

struct Box {
    int x;
    int y;
    int w;
    int h;
};

Box padded(int x, int y, int w, int h, int pad) {
    return {x - pad, y - pad, w + 2 * pad, h + 2 * pad};
}

void clear_behind(ui::Canvas& fb, int x, int y, int w, int h, int pad) {
    const Box b = padded(x, y, w, h, pad);
    fb.rect(b.x, b.y, b.w, b.h, false, true);
}

bool flight_over(const RadarSnapshot& snap) { return snap.flight_time_valid && !snap.airborne; }

Box clock_box(const RadarSnapshot& snap, int pad) {
    char buf[8];
    fmt_flight_clock(buf, snap.flight_seconds, snap.flight_time_valid);
    int w = text_width(buf, kClockScale);
    if (flight_over(snap)) w += kSecondsGap + text_width("00", kSecondsScale);
    return padded(kMargin, kClockY, w, kGlyphH * kClockScale, pad);
}

void flight_clock(ui::Canvas& fb, const RadarSnapshot& snap) {
    char buf[8];
    fmt_flight_clock(buf, snap.flight_seconds, snap.flight_time_valid);
    const int minutes_w = text_width(buf, kClockScale);
    clear_behind(fb, kMargin, kClockY, minutes_w, kGlyphH * kClockScale, kLabelPad);
    fb.draw_text(kMargin, kClockY, buf, true, kClockScale);
    if (!flight_over(snap)) return;

    char seconds[4];
    seconds[fmt_uint(seconds, snap.flight_seconds % 60, 2)] = 0;
    const int x = kMargin + minutes_w + kSecondsGap;
    const int y = kClockY + kGlyphH * (kClockScale - kSecondsScale);
    clear_behind(fb, x, y, text_width(seconds, kSecondsScale), kGlyphH * kSecondsScale, kLabelPad);
    fb.draw_text(x, y, seconds, true, kSecondsScale);
}

struct RangeText {
    char number[8];
    const char* unit;
    int number_w;
    int w;
};

RangeText range_text(const RadarSnapshot& snap) {
    RangeText t;
    const bool metric = snap.units == go::Units::Metric;
    const int32_t km_e1 = (snap.range_nm * kMetresPerNm) / 100;
    const int n = metric ? fmt_uint(t.number, static_cast<uint32_t>(km_e1), 2, 1)
                         : fmt_uint(t.number, static_cast<uint32_t>(snap.range_nm));
    t.number[n] = 0;
    t.unit = metric ? "KM" : "NM";
    t.number_w = text_width(t.number, kRangeScale);
    t.w = t.number_w + kUnitGap + text_width(t.unit, 1);
    return t;
}

Box range_box(const RadarSnapshot& snap, int pad) {
    const int w = range_text(snap).w;
    return padded(kCx - w / 2, kRangeY, w, kGlyphH * kRangeScale, pad);
}

void range_label(ui::Canvas& fb, const RadarSnapshot& snap) {
    const RangeText t = range_text(snap);
    const Box b = range_box(snap, kRangePad);
    const int x = b.x + kRangePad;
    fb.rect(b.x, b.y, b.w, b.h, false, true);
    fb.draw_text(x, kRangeY, t.number, true, kRangeScale);
    fb.draw_text(x + t.number_w + kUnitGap, kRangeY + kGlyphH * (kRangeScale - 1), t.unit, true, 1);
}

void flight_word(ui::Canvas& fb, const RadarSnapshot& snap) {
    if (!snap.fix_valid || !snap.airborne) return;
    const char* state = "FLIGHT";
    clear_behind(fb, kMargin, kStateY, text_width(state, kStateScale), kGlyphH * kStateScale,
                 kLabelPad);
    fb.draw_text(kMargin, kStateY, state, true, kStateScale);
}

const char* ring_word(const RadarSnapshot& snap) {
    if (!snap.fix_valid) return "NO FIX";
    if (snap.airborne) return nullptr;
    return snap.taxiing ? "TAXI" : "GROUND";
}

// INFO: fc 18sep26 how far the receiver has got, under the word that says it has not got there
const char* ring_note(const RadarSnapshot& snap) {
    return snap.fix_valid ? nullptr : gnss::stage_name(snap.stage);
}

Box note_box(const char* note) {
    const int w = text_width(note, kNoteScale);
    return {kCx - w / 2 - kNotePad, kNoteY - kNotePad, w + 2 * kNotePad,
            kGlyphH * kNoteScale + 2 * kNotePad};
}

void state_note(ui::Canvas& fb, const char* note) {
    const Box b = note_box(note);
    fb.rect(b.x, b.y, b.w, b.h, false, true);
    fb.draw_text(b.x + kNotePad, b.y + kNotePad, note, true, kNoteScale);
}

void aircraft(ui::Canvas& fb, int in_view, bool counting) {
    char buf[4];
    if (counting)
        buf[fmt_uint(buf, static_cast<uint32_t>(in_view))] = 0;
    else
        buf[fmt_string(buf, "-")] = 0;
    const int x = kGlassW - kMargin - text_width(buf, kTrafficScale);
    fb.draw_text(x, kFooterBottom - kGlyphH * kTrafficScale, buf, true, kTrafficScale);
    fb.draw_text(x - kUnitGap - text_width("ACT", 1), kFooterY, "ACT", true, 1);
}

Box banner_box(const char* word) {
    const int w = text_width(word, kBannerScale);
    return {kCx - w / 2 - kBannerPad, kBannerY - kBannerPad, w + 2 * kBannerPad,
            kGlyphH * kBannerScale + 2 * kBannerPad};
}

void state_banner(ui::Canvas& fb, const char* word) {
    const Box b = banner_box(word);
    fb.rect(b.x, b.y, b.w, b.h, false, true);
    fb.draw_text(b.x + kBannerPad, b.y + kBannerPad, word, true, kBannerScale);
}

struct Plotted {
    int32_t right;
    int32_t ahead;
    int x;
    int y;
    bool in_ring;
};

int64_t range_metres(const RadarSnapshot& snap) {
    return static_cast<int64_t>(snap.range_nm > 0 ? snap.range_nm : 1) * kMetresPerNm;
}

int32_t to_px(int32_t metres, int64_t range) {
    return static_cast<int32_t>((static_cast<int64_t>(metres) * kOuterR) / range);
}

bool inside_ring(int32_t right, int32_t ahead) {
    return right * right + ahead * ahead <= kOuterR * kOuterR;
}

bool on_glass(int x, int y) { return x >= 0 && x < kGlassW && y >= 0 && y < kGlassH; }

bool plot_point(const RadarSnapshot& snap, const RadarTarget& t, int16_t track, Plotted& out) {
    const int64_t range = range_metres(snap);
    const HeadingUp at = heading_up(t.north_m, t.east_m, track);
    const int32_t dx = to_px(at.right, range), dy = to_px(at.ahead, range);
    const int x = px_of(dx), y = py_of(dy);
    if (!on_glass(x, y)) return false;
    if (!inside_ring(dx, dy) && y + kAdvisoryR >= kFooterTop) return false;
    out = {dx, dy, x, y, inside_ring(dx, dy)};
    return true;
}

flight::Motion motion_of(int32_t speed_mps, uint16_t track_deg, int16_t turn_dps, bool turning) {
    flight::Motion m{};
    m.speed_q = static_cast<uint16_t>(speed_mps * kSpeedQPerMps);
    m.track_c9 =
        static_cast<uint16_t>((static_cast<int32_t>(track_deg % 360) * kTrackC9Turn) / 360);
    m.turn_dps = turn_dps;
    m.turning = turning;
    return m;
}

HeadingUp on_glass_at(const flight::Position& p, const RadarSnapshot& snap, int16_t track) {
    const HeadingUp at = heading_up(p.north_m, p.east_m, track);
    const int64_t range = range_metres(snap);
    return {to_px(at.ahead, range), to_px(at.right, range)};
}

Box formation_box() {
    return {kNear - kFormationD, kNear - kFormationD, 2 * kFormationD + 2, 2 * kFormationD + 2};
}

void formation_square(ui::Canvas& fb) {
    const int d = kFormationD, r = kFormationCorner;
    const int x0 = kNear - d, x1 = kFar + d, y0 = kNear - d, y1 = kFar + d;
    fb.hline(x0 + r + 1, y0, x1 - x0 - 2 * r - 1, true);
    fb.hline(x0 + r + 1, y1, x1 - x0 - 2 * r - 1, true);
    fb.vline(x0, y0 + r + 1, y1 - y0 - 2 * r - 1, true);
    fb.vline(x1, y0 + r + 1, y1 - y0 - 2 * r - 1, true);
    for (int step = 0; step <= 90; step++) {
        const int16_t a = c16(step);
        const int dx = r - (r * icos(a)) / kQ14One, dy = r - (r * isin(a)) / kQ14One;
        fb.set_pixel(x0 + dx, y0 + dy, true);
        fb.set_pixel(x1 - dx, y0 + dy, true);
        fb.set_pixel(x0 + dx, y1 - dy, true);
        fb.set_pixel(x1 - dx, y1 - dy, true);
    }
}

// One digit per quadrant of own-ship's nose: how many of the formation are over there.
void formation_counts(ui::Canvas& fb, const RadarSnapshot& snap, int16_t track) {
    int count[4] = {0, 0, 0, 0};
    for (int i = 0; i < snap.n_targets; i++) {
        const RadarTarget& t = snap.targets[i];
        if (!t.in_formation) continue;
        const HeadingUp at = heading_up(t.north_m, t.east_m, track);
        const int quadrant = (at.ahead < 0 ? 2 : 0) + (at.right >= 0 ? 1 : 0);
        count[quadrant]++;
    }

    const int left = kNear - kFormationD + kFormationInset;
    const int right = kFar + kFormationD - kFormationInset - kDigitW + 1;
    const int fore = kNear - kFormationD + kFormationInset;
    const int aft = kFar + kFormationD - kFormationInset - kGlyphH + 1;
    const int x[4] = {left, right, left, right};
    const int y[4] = {fore, fore, aft, aft};
    for (int q = 0; q < 4; q++) {
        if (count[q] <= 0) continue;
        char buf[4];
        buf[fmt_uint(buf, static_cast<uint32_t>(count[q] > 9 ? 9 : count[q]))] = 0;
        fb.draw_text(x[q], y[q], buf, true, 1);
    }
}

int own_minute_marks(ui::Canvas& fb, const RadarSnapshot& snap, int16_t track, Box* marked) {
    if (snap.speed_mps <= 0) return 0;
    flight::Arc arc(motion_of(snap.speed_mps, snap.track_deg, snap.turn_dps, true), kMinuteStepMs);
    int n = 0;
    for (int step = 1; step <= kMinutesMarked * kStepsPerMinute; step++) {
        const flight::Position ahead = arc.advance();
        if (step % kStepsPerMinute != 0) continue;
        const HeadingUp at = on_glass_at(ahead, snap, track);
        if (!inside_ring(at.right, at.ahead)) continue;
        if (at.right * at.right + at.ahead * at.ahead < kMinuteClearPx * kMinuteClearPx) continue;
        const int x = px_of(at.right) - kMinuteDotW / 2, y = py_of(at.ahead) - kMinuteDotW / 2;
        fb.rect(x, y, kMinuteDotW, kMinuteDotW, true, true);
        marked[n++] = {x, y, kMinuteDotW, kMinuteDotW};
    }
    return n;
}

struct Leader {
    int32_t right;
    int32_t ahead;
    bool valid;
};

Leader leader_of(const RadarSnapshot& snap, const RadarTarget& t, int16_t track) {
    if (t.speed_mps <= 0) return {0, 0, false};
    flight::Arc arc(motion_of(t.speed_mps, t.track_deg, t.turn_dps, t.turn_valid), kLeaderStepMs);
    HeadingUp end{0, 0};
    for (int step = 0; step < kLeaderSteps; step++) end = on_glass_at(arc.advance(), snap, track);
    if (end.right * end.right + end.ahead * end.ahead < kMinLeaderPx * kMinLeaderPx)
        return {0, 0, false};
    return {end.right, end.ahead, true};
}

void draw_leader(ui::Canvas& fb, const RadarSnapshot& snap, const RadarTarget& t, int16_t track,
                 const Plotted& p, const Leader& v) {
    if (!v.valid) return;
    flight::Arc arc(motion_of(t.speed_mps, t.track_deg, t.turn_dps, t.turn_valid), kLeaderStepMs);
    int from_x = p.x, from_y = p.y;
    for (int step = 0; step < kLeaderSteps; step++) {
        const HeadingUp at = on_glass_at(arc.advance(), snap, track);
        const int to_x = px_of(p.right + at.right), to_y = py_of(p.ahead + at.ahead);
        fb.line(from_x, from_y, to_x, to_y, true);
        from_x = to_x;
        from_y = to_y;
    }
}

void diamond(ui::Canvas& fb, int cx, int cy, int r, bool fill) {
    for (int dy = -r; dy <= r; dy++) {
        const int half = r - (dy < 0 ? -dy : dy);
        if (fill) {
            fb.hline(cx - half, cy + dy, 2 * half + 1, true);
        } else {
            fb.set_pixel(cx - half, cy + dy, true);
            fb.set_pixel(cx + half, cy + dy, true);
        }
    }
}

void traffic_symbol(ui::Canvas& fb, const Plotted& p, traffic::Level alarm_level) {
    if (alarm_level >= traffic::Level::Important) {
        fb.circle(p.x, p.y, kAdvisoryR, true, true);
        return;
    }
    diamond(fb, p.x, p.y, kSymbolR, alarm_level >= traffic::Level::Info);
}

void chevron(ui::Canvas& fb, int x, int y, bool up) {
    const int half = kChevronW / 2;
    const int travel = kChevronH - kChevronStroke;
    for (int s = 0; s < kChevronStroke; s++) {
        const int apex = up ? y + s : y + kChevronH - 1 - s;
        const int base = up ? apex + travel : apex - travel;
        fb.line(x, base, x + half - 1, apex, true);
        fb.line(x + half, apex, x + kChevronW - 1, base, true);
    }
}

int32_t hundreds_of_feet(int32_t up_m) {
    const int32_t ft = to_feet(Metres(up_m)).v;
    const int32_t half = kFeetPerTagUnit / 2;
    const int32_t hundreds = (ft >= 0 ? ft + half : ft - half) / kFeetPerTagUnit;
    if (hundreds > kMaxTagHundreds) return kMaxTagHundreds;
    if (hundreds < -kMaxTagHundreds) return -kMaxTagHundreds;
    return hundreds;
}

int chevron_direction(const RadarTarget& t) {
    if (!t.climb_valid) return 0;
    if (t.climb_e8 >= kChevronClimbE8) return 1;
    if (t.climb_e8 <= -kChevronClimbE8) return -1;
    return 0;
}

bool overlap(const Box& a, const Box& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

Box footer_band() { return {0, kFooterTop, kGlassW, kGlassH - kFooterTop}; }

Box own_ship_box() {
    return {kFar - ui::kSkyshipSpan / 2, kNear - ui::kSkyshipRowsToNose, ui::kSkyshipSpan,
            ui::kSkyshipRows};
}

bool fits_on_glass(const Box& b) {
    return on_glass(b.x, b.y) && on_glass(b.x + b.w - 1, b.y + b.h - 1);
}

struct Tag {
    char text[8];
    int climbing;
    Box box;
};

int symbol_radius(const RadarTarget& t) {
    return t.alarm_level >= traffic::Level::Important ? kAdvisoryR : kSymbolR;
}

Box symbol_box(const Plotted& p, const RadarTarget& t) {
    const int r = symbol_radius(t);
    return {p.x - r, p.y - r, 2 * r + 1, 2 * r + 1};
}

Tag tag_for(const Plotted& p, const RadarTarget& t) {
    Tag tag{};
    const int32_t hundreds = hundreds_of_feet(t.up_m);
    tag.text[fmt_int(tag.text, hundreds, 2, 0, hundreds == 0)] = 0;
    tag.climbing = chevron_direction(t);
    const int w =
        text_width(tag.text, kTagScale) + (tag.climbing != 0 ? kChevronGap + kChevronW : 0);
    const int h = kTagGlyphH + 2 * kTagPad;
    const int r = symbol_radius(t);
    const int y = t.up_m >= 0 ? p.y - r - kTagGap - h : p.y + r + 1 + kTagGap;
    tag.box = {p.x - w / 2 - kTagPad, y, w + 2 * kTagPad, h};
    return tag;
}

int slid_inside_margin(int x, int w) {
    if (x < kMargin) return kMargin;
    if (x + w > kGlassW - kMargin) return kGlassW - kMargin - w;
    return x;
}

Box side_clearance(const Box& b) {
    return {b.x - kTagSideClear, b.y, b.w + 2 * kTagSideClear, b.h};
}

bool place_tag(Tag& tag, const Plotted& p, const Leader& v, const Box* taken, int n_taken) {
    const int w = tag.box.w;
    const int centred = tag.box.x, beside = p.x + 1, before = p.x - w;
    int candidate[] = {centred, beside, before};
    if (v.valid) {
        candidate[0] = v.right >= 0 ? before : beside;
        candidate[1] = centred;
        candidate[2] = v.right >= 0 ? beside : before;
    }
    for (const int x : candidate) {
        tag.box.x = slid_inside_margin(x, w);
        if (!fits_on_glass(tag.box)) continue;
        bool clash = false;
        for (int i = 0; i < n_taken && !clash; i++) clash = overlap(tag.box, taken[i]);
        if (!clash) return true;
    }
    return false;
}

void draw_tag(ui::Canvas& fb, const Tag& tag) {
    fb.rect(tag.box.x, tag.box.y, tag.box.w, tag.box.h, false, true);
    const int x = tag.box.x + kTagPad, y = tag.box.y + kTagPad;
    fb.draw_text(x, y, tag.text, true, kTagScale);
    if (tag.climbing != 0)
        chevron(fb, tag.box.x + tag.box.w - kTagPad - kChevronW, y + (kTagGlyphH - kChevronH) / 2,
                tag.climbing > 0);
}

void loudest_first(const RadarTarget* const* in_view, int n, int* order) {
    for (int i = 0; i < n; i++) {
        int at = i;
        while (at > 0 && in_view[order[at - 1]]->alarm_level < in_view[i]->alarm_level) {
            order[at] = order[at - 1];
            at--;
        }
        order[at] = i;
    }
}

int plot(ui::Canvas& fb, const RadarSnapshot& snap, int16_t track) {
    Plotted shown[kMaxRadarTargets];
    const RadarTarget* in_view[kMaxRadarTargets];
    int n = 0;
    int in_ring = 0;
    for (int i = 0; i < snap.n_targets && n < kMaxRadarTargets; i++) {
        Plotted p;
        if (!plot_point(snap, snap.targets[i], track, p)) continue;
        // A member of the formation is drawn once, as the square around own ship
        // and the count in its quadrant. Twice is two aircraft.
        if (snap.targets[i].in_formation) {
            if (p.in_ring) in_ring++;
            continue;
        }
        shown[n] = p;
        in_view[n] = &snap.targets[i];
        n++;
    }

    for (int i = 0; i < n; i++)
        if (shown[i].in_ring) in_ring++;

    Leader run[kMaxRadarTargets];
    for (int i = 0; i < n; i++) {
        run[i] = leader_of(snap, *in_view[i], track);
        draw_leader(fb, snap, *in_view[i], track, shown[i], run[i]);
    }
    Box taken[2 * kMaxRadarTargets + 4 + kMinutesMarked];
    int n_taken = 0;
    if (in_ring > 0) n_taken += own_minute_marks(fb, snap, track, taken);
    taken[n_taken++] = footer_band();
    taken[n_taken++] = own_ship_box();
    if (const char* word = ring_word(snap)) taken[n_taken++] = banner_box(word);
    if (const char* note = ring_note(snap)) taken[n_taken++] = note_box(note);
    if (snap.formation_members > 0) taken[n_taken++] = formation_box();
    for (int i = 0; i < n; i++) taken[n_taken++] = symbol_box(shown[i], *in_view[i]);

    int order[kMaxRadarTargets];
    loudest_first(in_view, n, order);
    for (int i = 0; i < n; i++) {
        const int at = order[i];
        Tag tag = tag_for(shown[at], *in_view[at]);
        if (!place_tag(tag, shown[at], run[at], taken, n_taken)) continue;
        draw_tag(fb, tag);
        taken[n_taken++] = side_clearance(tag.box);
    }

    for (int i = 0; i < n; i++) traffic_symbol(fb, shown[i], in_view[i]->alarm_level);
    return in_ring;
}

struct Wedge {
    int32_t x{0};
    int32_t y{0};
};

int alarm_wedges(const RadarSnapshot& snap, int16_t track, Wedge* out) {
    if (!snap.alarm_flash) return 0;
    int n = 0;
    for (int i = 0; i < snap.n_targets && n < kMaxRadarTargets; i++) {
        const RadarTarget& t = snap.targets[i];
        if (t.alarm_level < traffic::Level::Info || t.alarm_dismissed) continue;
        const HeadingUp at = heading_up(t.north_m, t.east_m, track);
        if (at.ahead == 0 && at.right == 0) continue;
        out[n++] = {at.right, -at.ahead};
    }
    return n;
}

bool in_wedge(const Wedge& w, int64_t px, int64_t py) {
    const int64_t along = px * w.x + py * w.y;
    if (along <= 0) return false;
    const int64_t across = px * w.y - py * w.x;
    return (across < 0 ? -across : across) * kTanScale <= kWedgeEdgeTanE4 * along;
}

bool in_any_wedge(const Wedge* wedges, int n, int64_t px, int64_t py) {
    for (int i = 0; i < n; i++)
        if (in_wedge(wedges[i], px, py)) return true;
    return false;
}

bool inside_rounded(const Box& b, int x, int y) {
    const int past_left = b.x + kReadingCorner - x;
    const int past_right = x - (b.x + b.w - 1 - kReadingCorner);
    const int past_top = b.y + kReadingCorner - y;
    const int past_bottom = y - (b.y + b.h - 1 - kReadingCorner);
    if (x < b.x || y < b.y || past_right > kReadingCorner || past_bottom > kReadingCorner)
        return false;
    const int dx = past_left > 0 ? past_left : (past_right > 0 ? past_right : 0);
    const int dy = past_top > 0 ? past_top : (past_bottom > 0 ? past_bottom : 0);
    return dx * dx + dy * dy <= kReadingCorner * kReadingCorner;
}

bool spared(const Box* keep_out, int n, int x, int y) {
    for (int i = 0; i < n; i++)
        if (inside_rounded(keep_out[i], x, y)) return true;
    return false;
}

void invert_wedges(ui::Canvas& fb, const Wedge* wedges, int n_wedges, const Box* keep_out,
                   int n_keep_out) {
    const int64_t outer2 = 4 * static_cast<int64_t>(kWedgeOuterR) * kWedgeOuterR;
    const int64_t inner2 = 4 * static_cast<int64_t>(kWedgeInnerR) * kWedgeInnerR;
    const int bottom = kFar + kOuterR < kGlassH ? kFar + kOuterR : kGlassH;
    for (int y = kFar - kOuterR; y < bottom; y++) {
        const int64_t py = 2 * (y - kFar) + 1;
        for (int x = kFar - kOuterR; x < kFar + kOuterR; x++) {
            const int64_t px = 2 * (x - kFar) + 1;
            const int64_t r2 = px * px + py * py;
            if (r2 > outer2 || r2 < inner2) continue;
            if (!in_any_wedge(wedges, n_wedges, px, py)) continue;
            if (spared(keep_out, n_keep_out, x, y)) continue;
            fb.set_pixel(x, y, !fb.get_pixel(x, y));
        }
    }
}

}  // namespace

void draw_radar(ui::Canvas& fb, const RadarSnapshot& snap) {
    fb.clear(true);

    ring(fb, kOuterR);

    const int16_t track = c16(snap.track_deg);

    if (snap.formation_members > 0) formation_square(fb);
    ui::draw_skyship(fb, kFar, kNear);
    if (snap.formation_members > 0) formation_counts(fb, snap, track);

    const int in_ring = snap.fix_valid ? plot(fb, snap, track) : 0;

    flight_clock(fb, snap);
    flight_word(fb, snap);
    range_label(fb, snap);
    aircraft(fb, in_ring, snap.fix_valid && snap.receiver_listening);

    Wedge wedges[kMaxRadarTargets];
    const int n_wedges = alarm_wedges(snap, track, wedges);
    if (n_wedges > 0) {
        const Box readings[] = {range_box(snap, kKeepOutPad), clock_box(snap, kKeepOutPad)};
        invert_wedges(fb, wedges, n_wedges, readings, 2);
    }

    if (const char* word = ring_word(snap)) state_banner(fb, word);
    if (const char* note = ring_note(snap)) state_note(fb, note);
}

}  // namespace skyblip::go
