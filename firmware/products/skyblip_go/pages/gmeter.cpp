#include "products/skyblip_go/pages/gmeter.h"

#include "core/util/format.h"
#include "core/util/intmath.h"

namespace skyblip::go {

namespace {
constexpr int kLeft = 4;
constexpr int kCellW = 6;
constexpr int kGlyphH = 7;
constexpr int kTitleY = 3;
constexpr int kRuleY = 21;

constexpr int kFieldCx = 82;
constexpr int kFieldCy = 82;
constexpr int kFieldHalfW = 52;
constexpr int kFieldHalfH = 52;
constexpr int kLateralPerG = 52;
constexpr int kNormalPerG = 16;
constexpr int kMarkerR = 3;
constexpr int kTickLen = 3;

constexpr int kStripCx = 164;
constexpr int kStripHalfW = 10;
constexpr int kStripTop = kFieldCy - kFieldHalfH;
constexpr int kStripBottom = kFieldCy + kFieldHalfH;
constexpr int kThrownPerG = 36;
constexpr int kStripLabelInset = 2;

constexpr int kNormalRowY = 140;
constexpr int kLateralRowY = 160;
constexpr int kLongitudinalRowY = 180;
constexpr int kPeakDrop = 4;
constexpr int kNowScale = 2;

constexpr int kNowEnd = 116;
constexpr int kLeastEnd = 156;
constexpr int kMostEnd = 194;

constexpr int32_t kMilliPerG = 1000;
constexpr int32_t kMilliPerTenth = 100;
constexpr int16_t kLevelFlightMg = flight::kLevelFlightMg;

int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

int32_t tenths(int32_t mg) { return div_round(mg, kMilliPerTenth); }

int fmt_g(char* out, int32_t mg) {
    const int32_t value = tenths(mg);
    const int n = fmt_int(out, value, 1, 1, value == 0);
    out[n] = 0;
    return n;
}

int fmt_named_g(char* out, int32_t mg, char positive, char negative) {
    const int32_t value = tenths(mg);
    int n = 0;
    if (value != 0) out[n++] = value < 0 ? negative : positive;
    n += fmt_uint(out + n, static_cast<uint32_t>(value < 0 ? -value : value), 1, 1);
    out[n] = 0;
    return n;
}

void right_aligned(ui::Canvas& fb, int x_end, int y, const char* text, int len, int scale = 1) {
    fb.draw_text(x_end - len * kCellW * scale, y, text, true, scale);
}

void centred(ui::Canvas& fb, int cx, int y, const char* text, int len) {
    fb.draw_text(cx - (len * kCellW - 1) / 2, y, text, true, 1);
}

void value_at(ui::Canvas& fb, int x_end, int y, int32_t mg, int scale = 1) {
    char buf[8];
    const int n = fmt_g(buf, mg);
    right_aligned(fb, x_end, y, buf, n, scale);
}

void named_at(ui::Canvas& fb, int x_end, int y, int32_t mg, char positive, char negative,
              int scale = 1) {
    char buf[12];
    const int n = fmt_named_g(buf, mg, positive, negative);
    right_aligned(fb, x_end, y, buf, n, scale);
}

int thrown_down_y(int32_t mg) {
    const int32_t from_level = mg - kLevelFlightMg;
    const int offset = static_cast<int>((from_level * kNormalPerG) / kMilliPerG);
    return kFieldCy + clampi(offset, -kFieldHalfH, kFieldHalfH);
}

int lateral_x(int32_t mg) {
    const int offset = static_cast<int>((mg * kLateralPerG) / kMilliPerG);
    return kFieldCx + clampi(offset, -kFieldHalfW, kFieldHalfW);
}

int thrown_forward_y(int32_t accelerating_mg) {
    const int offset = static_cast<int>((accelerating_mg * kThrownPerG) / kMilliPerG);
    return kFieldCy + clampi(offset, -kThrownPerG, kThrownPerG);
}

void field_scale(ui::Canvas& fb) {
    fb.rect(kFieldCx - kFieldHalfW, kFieldCy - kFieldHalfH, 2 * kFieldHalfW + 1,
            2 * kFieldHalfH + 1, true);
    fb.hline(kFieldCx - kFieldHalfW, kFieldCy, 2 * kFieldHalfW + 1, true);
    fb.vline(kFieldCx, kFieldCy - kFieldHalfH, 2 * kFieldHalfH + 1, true);

    for (int32_t g = -2; g <= 4; g++) {
        if (g == 1) continue;
        fb.hline(kFieldCx - kTickLen, thrown_down_y(g * kMilliPerG), 2 * kTickLen + 1, true);
    }
    for (int32_t half_g = -2; half_g <= 2; half_g++) {
        if (half_g == 0) continue;
        fb.vline(lateral_x(half_g * kMilliPerG / 2), kFieldCy - kTickLen, 2 * kTickLen + 1, true);
    }
}

void field_reading(ui::Canvas& fb, const GMeterSnapshot& s) {
    const int left = lateral_x(s.least.lateral_mg);
    const int right = lateral_x(s.most.lateral_mg);
    const int top = thrown_down_y(s.least.normal_mg);
    const int bottom = thrown_down_y(s.most.normal_mg);
    fb.rect(left, top, right - left + 1, bottom - top + 1, true);
    fb.circle(lateral_x(s.now.lateral_mg), thrown_down_y(s.now.normal_mg), kMarkerR, true, true);
}

void strip_scale(ui::Canvas& fb) {
    fb.rect(kStripCx - kStripHalfW, kStripTop, 2 * kStripHalfW + 1, kStripBottom - kStripTop + 1,
            true);
    fb.hline(kStripCx - kStripHalfW, kFieldCy, 2 * kStripHalfW + 1, true);
    centred(fb, kStripCx, kStripTop + kStripLabelInset, "DEC", 3);
    centred(fb, kStripCx, kStripBottom - kStripLabelInset - kGlyphH, "ACC", 3);
}

void strip_reading(ui::Canvas& fb, const GMeterSnapshot& s) {
    const int inset = kStripHalfW - 3;
    fb.hline(kStripCx - inset, thrown_forward_y(s.most.longitudinal_mg), 2 * inset + 1, true);
    fb.hline(kStripCx - inset, thrown_forward_y(s.least.longitudinal_mg), 2 * inset + 1, true);
    fb.rect(kStripCx - inset, thrown_forward_y(s.now.longitudinal_mg) - 1, 2 * inset + 1, 3, true,
            true);
}

void row_label(ui::Canvas& fb, int y, const char* label, bool valid) {
    fb.draw_text(kLeft, y + kPeakDrop, label, true, 1);
    if (!valid) fb.draw_text(kNowEnd - 4 * kCellW, y + kPeakDrop, "----", true, 1);
}

}  // namespace

void draw_gmeter(ui::Canvas& fb, const GMeterSnapshot& s) {
    fb.clear(true);
    fb.draw_text(kLeft, kTitleY, "G METER", true, 2);
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

    row_label(fb, kNormalRowY, "G", s.valid);
    row_label(fb, kLateralRowY, "SIDE", s.valid);
    row_label(fb, kLongitudinalRowY, "ACCEL", s.valid);
    if (!s.valid) return;

    value_at(fb, kNowEnd, kNormalRowY, s.now.normal_mg, kNowScale);
    value_at(fb, kLeastEnd, kNormalRowY + kPeakDrop, s.least.normal_mg);
    value_at(fb, kMostEnd, kNormalRowY + kPeakDrop, s.most.normal_mg);

    named_at(fb, kNowEnd, kLateralRowY, s.now.lateral_mg, 'R', 'L', kNowScale);
    named_at(fb, kLeastEnd, kLateralRowY + kPeakDrop, s.least.lateral_mg, 'R', 'L');
    named_at(fb, kMostEnd, kLateralRowY + kPeakDrop, s.most.lateral_mg, 'R', 'L');

    named_at(fb, kNowEnd, kLongitudinalRowY, s.now.longitudinal_mg, 'A', 'D', kNowScale);
    named_at(fb, kLeastEnd, kLongitudinalRowY + kPeakDrop, s.least.longitudinal_mg, 'A', 'D');
    named_at(fb, kMostEnd, kLongitudinalRowY + kPeakDrop, s.most.longitudinal_mg, 'A', 'D');
}

}  // namespace skyblip::go
