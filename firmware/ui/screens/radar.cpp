#include "ui/screens/radar.h"

#include "core/units/units.h"
#include "core/util/format.h"
#include "core/util/intmath.h"
#include "ui/widgets/skyship.h"

namespace skyblip::ui {

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
constexpr int kNear = Framebuffer::kW / 2 - 1;
constexpr int kFar = Framebuffer::kW / 2;
constexpr int kCx = kFar;  // only for centring text, which has no such nicety
constexpr int kMargin = 4;
constexpr int kOuterR = 92;
constexpr int kRingW = 2;
constexpr int kGlyphH = 7;
constexpr int kCellW = 6;
constexpr int kAlarmBarH = 3;
constexpr int kClockScale = 2;
constexpr int kRangeScale = 2;
constexpr int kStateScale = 1;
constexpr int kTrafficScale = 3;
constexpr int kRangePad = 6;
constexpr int kLabelPad = 2;
constexpr int kStackGap = 2;
constexpr int kFooterBottom = Framebuffer::kH - kMargin;
constexpr int kFooterY = kFooterBottom - kGlyphH;
constexpr int kClockY = kFooterBottom - kGlyphH * kClockScale;
constexpr int kStateY = kClockY - kStackGap - kGlyphH * kStateScale;
constexpr int kRangeY = kFooterBottom - kGlyphH * kRangeScale;
constexpr int kUnitGap = 3;
constexpr int32_t kQ14One = 16384;
constexpr int32_t kTurn16 = 65536;
constexpr int kSymbolR = 4;
constexpr int kAdvisoryR = 5;
constexpr uint8_t kProximateLevel = 1;
constexpr uint8_t kAdvisoryLevel = 2;
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
constexpr int kMinLeaderPx = 3;
constexpr int kOwnNoseAhead = kSkyshipRowsToNose + 1;
constexpr int kFooterTop = kStateY - kLabelPad;
constexpr int kMinuteDotW = 2;
constexpr int kMinutesMarked = 2;

int half_chord_in_half_pixels(int r, int b) {
    const int32_t v = 4 * r * r - (2 * b + 1) * (2 * b + 1);
    if (v < 0) return -1;
    int q = isqrt<int32_t>(v);
    while (q * q > v) q--;
    return (q - 1) / 2;
}

void ring(Framebuffer& fb, int r) {
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

void clear_behind(Framebuffer& fb, int x, int y, int w, int h, int pad) {
    fb.rect(x - pad, y - pad, w + 2 * pad, h + 2 * pad, false, true);
}

void flight_clock(Framebuffer& fb, const RadarSnapshot& snap) {
    char buf[8];
    fmt_flight_clock(buf, snap.flight_seconds, snap.flight_time_valid);
    clear_behind(fb, kMargin, kClockY, text_width(buf, kClockScale), kGlyphH * kClockScale,
                 kLabelPad);
    fb.draw_text(kMargin, kClockY, buf, true, kClockScale);
}

void range_label(Framebuffer& fb, const RadarSnapshot& snap) {
    const bool metric = snap.units == settings::Units::Metric;
    const int32_t km_e1 = (snap.range_nm * kMetresPerNm) / 100;
    char buf[8];
    const int n = metric ? fmt_uint(buf, static_cast<uint32_t>(km_e1), 2, 1)
                         : fmt_uint(buf, static_cast<uint32_t>(snap.range_nm));
    buf[n] = 0;
    const char* unit = metric ? "KM" : "NM";
    const int number_w = text_width(buf, kRangeScale);
    const int w = number_w + kUnitGap + text_width(unit, 1);
    const int x = kCx - w / 2;
    clear_behind(fb, x, kRangeY, w, kGlyphH * kRangeScale, kRangePad);
    fb.draw_text(x, kRangeY, buf, true, kRangeScale);
    fb.draw_text(x + number_w + kUnitGap, kRangeY + kGlyphH * (kRangeScale - 1), unit, true, 1);
}

void flight_state(Framebuffer& fb, const RadarSnapshot& snap) {
    const char* state = !snap.fix_valid ? "NO FIX" : (snap.airborne ? "FLIGHT" : "GROUND");
    clear_behind(fb, kMargin, kStateY, text_width(state, kStateScale), kGlyphH * kStateScale,
                 kLabelPad);
    fb.draw_text(kMargin, kStateY, state, true, kStateScale);
}

void aircraft(Framebuffer& fb, int in_view, bool counting) {
    char buf[4];
    if (counting)
        buf[fmt_uint(buf, static_cast<uint32_t>(in_view))] = 0;
    else
        buf[fmt_string(buf, "-")] = 0;
    const int x = Framebuffer::kW - kMargin - text_width(buf, kTrafficScale);
    fb.draw_text(x, kFooterBottom - kGlyphH * kTrafficScale, buf, true, kTrafficScale);
    fb.draw_text(x - kUnitGap - text_width("ACT", 1), kFooterY, "ACT", true, 1);
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

bool on_glass(int x, int y) {
    return x >= 0 && x < Framebuffer::kW && y >= 0 && y < Framebuffer::kH;
}

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

void own_minute_marks(Framebuffer& fb, const RadarSnapshot& snap) {
    if (snap.speed_mps <= 0) return;
    for (int minute = 1; minute <= kMinutesMarked; minute++) {
        const int32_t ahead = to_px(snap.speed_mps * kLeaderSeconds * minute, range_metres(snap));
        if (ahead > kOuterR || ahead - kOwnNoseAhead < kMinLeaderPx) continue;
        fb.rect(kNear, kNear - ahead, kMinuteDotW, kMinuteDotW, true, true);
    }
}

struct Leader {
    int32_t right;
    int32_t ahead;
    bool valid;
};

Leader leader_of(const RadarSnapshot& snap, const RadarTarget& t, int16_t track) {
    if (t.speed_mps <= 0) return {0, 0, false};
    const int16_t course = c16(t.track_deg);
    const int64_t run = static_cast<int64_t>(t.speed_mps) * kLeaderSeconds;
    const HeadingUp v = heading_up(static_cast<int32_t>((run * icos(course)) / kQ14One),
                                   static_cast<int32_t>((run * isin(course)) / kQ14One), track);
    const int64_t range = range_metres(snap);
    const int32_t right = to_px(v.right, range), ahead = to_px(v.ahead, range);
    if (right * right + ahead * ahead < kMinLeaderPx * kMinLeaderPx) return {0, 0, false};
    return {right, ahead, true};
}

void draw_leader(Framebuffer& fb, const Plotted& p, const Leader& v) {
    if (!v.valid) return;
    fb.line(p.x, p.y, px_of(p.right + v.right), py_of(p.ahead + v.ahead), true);
}

void diamond(Framebuffer& fb, int cx, int cy, int r, bool fill) {
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

void traffic_symbol(Framebuffer& fb, const Plotted& p, uint8_t alarm_level) {
    if (alarm_level >= kAdvisoryLevel) {
        fb.circle(p.x, p.y, kAdvisoryR, true, true);
        return;
    }
    diamond(fb, p.x, p.y, kSymbolR, alarm_level >= kProximateLevel);
}

void chevron(Framebuffer& fb, int x, int y, bool up) {
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

struct Box {
    int x;
    int y;
    int w;
    int h;
};

bool overlap(const Box& a, const Box& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

Box footer_band() { return {0, kFooterTop, Framebuffer::kW, Framebuffer::kH - kFooterTop}; }

Box own_ship_box() {
    return {kFar - kSkyshipSpan / 2, kNear - kSkyshipRowsToNose, kSkyshipSpan, kSkyshipRows};
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
    return t.alarm_level >= kAdvisoryLevel ? kAdvisoryR : kSymbolR;
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
    if (x + w > Framebuffer::kW - kMargin) return Framebuffer::kW - kMargin - w;
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

void draw_tag(Framebuffer& fb, const Tag& tag) {
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

int plot(Framebuffer& fb, const RadarSnapshot& snap, int16_t track) {
    Plotted shown[kMaxRadarTargets];
    const RadarTarget* in_view[kMaxRadarTargets];
    int n = 0;
    for (int i = 0; i < snap.n_targets && n < kMaxRadarTargets; i++) {
        Plotted p;
        if (!plot_point(snap, snap.targets[i], track, p)) continue;
        shown[n] = p;
        in_view[n] = &snap.targets[i];
        n++;
    }

    int in_ring = 0;
    for (int i = 0; i < n; i++)
        if (shown[i].in_ring) in_ring++;

    Leader run[kMaxRadarTargets];
    for (int i = 0; i < n; i++) {
        run[i] = leader_of(snap, *in_view[i], track);
        draw_leader(fb, shown[i], run[i]);
    }
    if (in_ring > 0) own_minute_marks(fb, snap);

    Box taken[2 * kMaxRadarTargets + 2];
    int n_taken = 0;
    taken[n_taken++] = footer_band();
    taken[n_taken++] = own_ship_box();
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

}  // namespace

void draw_radar(Framebuffer& fb, const RadarSnapshot& snap) {
    fb.clear(true);

    ring(fb, kOuterR);

    const int16_t track = c16(snap.track_deg);

    draw_skyship(fb, kFar, kNear);

    const int in_ring = snap.fix_valid ? plot(fb, snap, track) : 0;

    flight_clock(fb, snap);
    flight_state(fb, snap);
    range_label(fb, snap);
    aircraft(fb, in_ring, snap.fix_valid && snap.receiver_listening);

    if (snap.max_alarm >= 3) fb.rect(0, 0, Framebuffer::kW, kAlarmBarH, true, true);
}

}  // namespace skyblip::ui
