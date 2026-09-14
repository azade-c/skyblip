#include "ui/screens/radar.h"

#include "core/util/format.h"
#include "core/util/intmath.h"
#include "core/util/units.h"
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
constexpr int kTagGap = 3;
constexpr int kTagPad = 1;
constexpr int kChevronW = 5;
constexpr int kChevronH = 3;
constexpr int kChevronGap = 2;
constexpr int32_t kFeetPerTagUnit = 100;
constexpr int16_t kChevronClimbE8 = 20;
constexpr int32_t kLeaderSeconds = 60;
constexpr int kMinLeaderPx = 3;
constexpr int kClipSteps = 64;
constexpr int kOwnNoseAhead = kSkyshipRowsToNose + 1;
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
    fmt_flight_clock(buf, snap.flight_seconds, snap.have_flight_time);
    clear_behind(fb, kMargin, kClockY, text_width(buf, kClockScale), kGlyphH * kClockScale,
                 kLabelPad);
    fb.draw_text(kMargin, kClockY, buf, true, kClockScale);
}

void range_label(Framebuffer& fb, int32_t range_nm) {
    char buf[8];
    buf[fmt_uint(buf, static_cast<uint32_t>(range_nm))] = 0;
    const int number_w = text_width(buf, kRangeScale);
    const int w = number_w + kUnitGap + text_width("NM", 1);
    const int x = kCx - w / 2;
    clear_behind(fb, x, kRangeY, w, kGlyphH * kRangeScale, kRangePad);
    fb.draw_text(x, kRangeY, buf, true, kRangeScale);
    fb.draw_text(x + number_w + kUnitGap, kRangeY + kGlyphH * (kRangeScale - 1), "NM", true, 1);
}

void flight_state(Framebuffer& fb, const RadarSnapshot& snap) {
    const char* state = !snap.have_fix ? "NO FIX" : (snap.airborne ? "FLIGHT" : "GROUND");
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
};

int64_t range_metres(const RadarSnapshot& snap) {
    return (snap.range_nm > 0 ? snap.range_nm : 1) * kMetresPerNm;
}

int32_t to_px(int32_t metres, int64_t range) {
    return static_cast<int32_t>((static_cast<int64_t>(metres) * kOuterR) / range);
}

bool inside_ring(int32_t right, int32_t ahead) {
    return right * right + ahead * ahead <= kOuterR * kOuterR;
}

bool plot_point(const RadarSnapshot& snap, const RadarTarget& t, int16_t track, Plotted& out) {
    const int64_t range = range_metres(snap);
    const HeadingUp at = heading_up(t.north_m, t.east_m, track);
    const int32_t dx = to_px(at.right, range), dy = to_px(at.ahead, range);
    if (!inside_ring(dx, dy)) return false;
    out = {dx, dy, px_of(dx), py_of(dy)};
    return true;
}

void shorten_into_ring(const Plotted& from, int32_t& right, int32_t& ahead) {
    if (inside_ring(from.right + right, from.ahead + ahead)) return;
    int lo = 0, hi = kClipSteps;
    while (hi - lo > 1) {
        const int mid = (lo + hi) / 2;
        if (inside_ring(from.right + right * mid / kClipSteps,
                        from.ahead + ahead * mid / kClipSteps))
            lo = mid;
        else
            hi = mid;
    }
    right = right * lo / kClipSteps;
    ahead = ahead * lo / kClipSteps;
}

void own_minute_marks(Framebuffer& fb, const RadarSnapshot& snap) {
    if (snap.speed_mps <= 0) return;
    for (int minute = 1; minute <= kMinutesMarked; minute++) {
        const int32_t ahead = to_px(snap.speed_mps * kLeaderSeconds * minute, range_metres(snap));
        if (ahead > kOuterR || ahead - kOwnNoseAhead < kMinLeaderPx) continue;
        fb.rect(kNear, kNear - ahead, kMinuteDotW, kMinuteDotW, true, true);
    }
}

void leader(Framebuffer& fb, const RadarSnapshot& snap, const RadarTarget& t, int16_t track,
            const Plotted& p) {
    if (t.speed_mps <= 0) return;
    const int16_t course = c16(t.track_deg);
    const int64_t run = static_cast<int64_t>(t.speed_mps) * kLeaderSeconds;
    const HeadingUp v = heading_up(static_cast<int32_t>((run * icos(course)) / kQ14One),
                                   static_cast<int32_t>((run * isin(course)) / kQ14One), track);
    const int64_t range = range_metres(snap);
    int32_t right = to_px(v.right, range), ahead = to_px(v.ahead, range);
    if (right * right + ahead * ahead < kMinLeaderPx * kMinLeaderPx) return;
    shorten_into_ring(p, right, ahead);
    fb.line(p.x, p.y, px_of(p.right + right), py_of(p.ahead + ahead), true);
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
    const int apex = up ? y : y + kChevronH - 1;
    const int base = up ? y + kChevronH - 1 : y;
    fb.line(x, base, x + kChevronW / 2, apex, true);
    fb.line(x + kChevronW / 2, apex, x + kChevronW - 1, base, true);
}

int32_t hundreds_of_feet(int32_t up_m) {
    const int32_t ft = to_feet(Metres(up_m)).v;
    const int32_t half = kFeetPerTagUnit / 2;
    return (ft >= 0 ? ft + half : ft - half) / kFeetPerTagUnit;
}

int chevron_direction(const RadarTarget& t) {
    if (!t.has_climb) return 0;
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

struct Tag {
    char text[8];
    int climbing;
    Box box;
};

Tag tag_for(const Plotted& p, const RadarTarget& t) {
    Tag tag{};
    const int32_t hundreds = hundreds_of_feet(t.up_m);
    tag.text[fmt_int(tag.text, hundreds, 2, 0, hundreds == 0)] = 0;
    tag.climbing = chevron_direction(t);
    const int w = text_width(tag.text, 1) + (tag.climbing != 0 ? kChevronGap + kChevronW : 0);
    const int r = t.alarm_level >= kAdvisoryLevel ? kAdvisoryR : kSymbolR;
    const int y = t.up_m >= 0 ? p.y - r - kTagGap - kGlyphH : p.y + r + kTagGap;
    tag.box = {p.x - w / 2 - kTagPad, y - kTagPad, w + 2 * kTagPad, kGlyphH + 2 * kTagPad};
    return tag;
}

void draw_tag(Framebuffer& fb, const Tag& tag) {
    fb.rect(tag.box.x, tag.box.y, tag.box.w, tag.box.h, false, true);
    const int x = tag.box.x + kTagPad, y = tag.box.y + kTagPad;
    fb.draw_text(x, y, tag.text, true, 1);
    if (tag.climbing != 0)
        chevron(fb, tag.box.x + tag.box.w - kTagPad - kChevronW, y + (kGlyphH - kChevronH) / 2,
                tag.climbing > 0);
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

    for (int i = 0; i < n; i++) leader(fb, snap, *in_view[i], track, shown[i]);

    Box tagged[kMaxRadarTargets];
    int n_tags = 0;
    for (int i = 0; i < n; i++) {
        const Tag tag = tag_for(shown[i], *in_view[i]);
        bool clash = false;
        for (int j = 0; j < n_tags && !clash; j++) clash = overlap(tag.box, tagged[j]);
        if (clash) continue;
        draw_tag(fb, tag);
        tagged[n_tags++] = tag.box;
    }

    for (int i = 0; i < n; i++) traffic_symbol(fb, shown[i], in_view[i]->alarm_level);
    return n;
}

}  // namespace

void draw_radar(Framebuffer& fb, const RadarSnapshot& snap) {
    fb.clear(true);

    ring(fb, kOuterR);

    const int16_t track = c16(snap.track_deg);

    draw_skyship(fb, kFar, kNear);

    const int in_view = snap.have_fix ? plot(fb, snap, track) : 0;
    if (in_view > 0) own_minute_marks(fb, snap);

    flight_clock(fb, snap);
    flight_state(fb, snap);
    range_label(fb, snap.range_nm);
    aircraft(fb, in_view, snap.have_fix && snap.receiver_listening);

    if (snap.max_alarm >= 3) fb.rect(0, 0, Framebuffer::kW, kAlarmBarH, true, true);
}

}  // namespace skyblip::ui
