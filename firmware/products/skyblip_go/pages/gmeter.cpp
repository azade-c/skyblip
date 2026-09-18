#include "products/skyblip_go/pages/gmeter.h"

#include "core/util/format.h"

namespace skyblip::go {

namespace {
constexpr int kLeft = 4;
constexpr int kCellW = 6;
constexpr int kGlyphH = 7;
constexpr int kTitleY = 3;
constexpr int kRuleY = 21;

constexpr int kFieldCx = 100;
constexpr int kFieldCy = 82;
constexpr int kFieldHalfW = 52;
constexpr int kFieldHalfH = 52;
constexpr int kLateralPerG = 52;
constexpr int kNormalPerG = 16;
constexpr int kMarkerR = 3;
constexpr int kTickLen = 3;

constexpr int kStripY = 158;
constexpr int kStripH = 15;
constexpr int kStripHalfW = 78;
constexpr int kLongitudinalPerG = 78;

constexpr int kNormalRowY = 136;
constexpr int kLateralRowY = 146;
constexpr int kStripRowY = 178;

constexpr int kNowEnd = kLeft + 10 * kCellW;
constexpr int kMostEnd = kLeft + 17 * kCellW;
constexpr int kLeastEnd = kLeft + 23 * kCellW;

constexpr int32_t kMilliPerG = 1000;
constexpr int32_t kMilliPerTenth = 100;
constexpr int16_t kLevelFlightMg = flight::kLevelFlightMg;

int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

int fmt_g(char* out, int32_t mg) {
    const int32_t half = mg < 0 ? -kMilliPerTenth / 2 : kMilliPerTenth / 2;
    const int n = fmt_int(out, (mg + half) / kMilliPerTenth, 1, 1, false);
    out[n] = 0;
    return n;
}

void right_aligned(ui::Canvas& fb, int x_end, int y, const char* text, int len, int scale = 1) {
    fb.draw_text(x_end - len * kCellW * scale, y, text, true, scale);
}

void value_at(ui::Canvas& fb, int x_end, int y, int32_t mg) {
    char buf[8];
    const int n = fmt_g(buf, mg);
    right_aligned(fb, x_end, y, buf, n);
}

int normal_y(int32_t mg) {
    const int32_t from_level = mg - kLevelFlightMg;
    const int offset = static_cast<int>((from_level * kNormalPerG) / kMilliPerG);
    return kFieldCy - clampi(offset, -kFieldHalfH, kFieldHalfH);
}

int lateral_x(int32_t mg) {
    const int offset = static_cast<int>((mg * kLateralPerG) / kMilliPerG);
    return kFieldCx + clampi(offset, -kFieldHalfW, kFieldHalfW);
}

int longitudinal_x(int32_t mg) {
    const int offset = static_cast<int>((mg * kLongitudinalPerG) / kMilliPerG);
    return kFieldCx + clampi(offset, -kStripHalfW, kStripHalfW);
}

void field_scale(ui::Canvas& fb) {
    fb.rect(kFieldCx - kFieldHalfW, kFieldCy - kFieldHalfH, 2 * kFieldHalfW + 1,
            2 * kFieldHalfH + 1, true);
    fb.hline(kFieldCx - kFieldHalfW, kFieldCy, 2 * kFieldHalfW + 1, true);
    fb.vline(kFieldCx, kFieldCy - kFieldHalfH, 2 * kFieldHalfH + 1, true);

    for (int32_t g = -2; g <= 4; g++) {
        if (g == 1) continue;
        const int y = normal_y(g * kMilliPerG);
        fb.hline(kFieldCx - kTickLen, y, 2 * kTickLen + 1, true);
    }
    for (int32_t half_g = -2; half_g <= 2; half_g++) {
        if (half_g == 0) continue;
        const int x = lateral_x(half_g * kMilliPerG / 2);
        fb.vline(x, kFieldCy - kTickLen, 2 * kTickLen + 1, true);
    }
}

void field_reading(ui::Canvas& fb, const GMeterSnapshot& s) {
    const int left = lateral_x(s.least.lateral_mg);
    const int right = lateral_x(s.most.lateral_mg);
    const int top = normal_y(s.most.normal_mg);
    const int bottom = normal_y(s.least.normal_mg);
    fb.rect(left, top, right - left + 1, bottom - top + 1, true);
    fb.circle(lateral_x(s.now.lateral_mg), normal_y(s.now.normal_mg), kMarkerR, true, true);
}

void strip_scale(ui::Canvas& fb) {
    fb.rect(kFieldCx - kStripHalfW, kStripY, 2 * kStripHalfW + 1, kStripH, true);
    fb.vline(kFieldCx, kStripY, kStripH, true);
    for (int32_t half_g = -2; half_g <= 2; half_g++) {
        if (half_g == 0) continue;
        const int x = longitudinal_x(half_g * kMilliPerG / 2);
        fb.vline(x, kStripY + kStripH - 4, 4, true);
    }
}

void strip_reading(ui::Canvas& fb, const GMeterSnapshot& s) {
    fb.vline(longitudinal_x(s.most.longitudinal_mg), kStripY + 1, kStripH - 2, true);
    fb.vline(longitudinal_x(s.least.longitudinal_mg), kStripY + 1, kStripH - 2, true);
    fb.rect(longitudinal_x(s.now.longitudinal_mg) - 1, kStripY + 3, 3, kStripH - 6, true, true);
}

void reading_row(ui::Canvas& fb, int y, const char* label, int32_t now_mg, int32_t most_mg,
                 int32_t least_mg, bool valid) {
    fb.draw_text(kLeft, y, label, true, 1);
    if (!valid) {
        fb.draw_text(kNowEnd - 4 * kCellW, y, "----", true, 1);
        return;
    }
    value_at(fb, kNowEnd, y, now_mg);
    value_at(fb, kMostEnd, y, most_mg);
    value_at(fb, kLeastEnd, y, least_mg);
}

}  // namespace

void draw_gmeter(ui::Canvas& fb, const GMeterSnapshot& s) {
    fb.clear(true);
    fb.draw_text(kLeft, kTitleY, "G", true, 2);

    if (s.valid) {
        char buf[8];
        const int n = fmt_g(buf, s.now.normal_mg);
        right_aligned(fb, kGlassW - kLeft, kTitleY, buf, n, 2);
    }
    fb.hline(kLeft, kRuleY, kGlassW - 2 * kLeft, true);

    if (!s.fitted) {
        fb.draw_text(kLeft, kFieldCy - kGlyphH / 2, "NO SENSOR", true, 1);
        return;
    }

    field_scale(fb);
    strip_scale(fb);
    if (s.valid) {
        field_reading(fb, s);
        strip_reading(fb, s);
    }

    reading_row(fb, kNormalRowY, "NRM", s.now.normal_mg, s.most.normal_mg, s.least.normal_mg,
                s.valid);
    reading_row(fb, kLateralRowY, "LAT", s.now.lateral_mg, s.most.lateral_mg, s.least.lateral_mg,
                s.valid);
    reading_row(fb, kStripRowY, "F/A", s.now.longitudinal_mg, s.most.longitudinal_mg,
                s.least.longitudinal_mg, s.valid);
}

}  // namespace skyblip::go
