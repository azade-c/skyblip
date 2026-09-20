#include "products/skyblip_go/pages/raw.h"

#include "core/util/format.h"

namespace skyblip::go {

namespace {
constexpr int kLeft = 4;
constexpr int kCellW = 6;
constexpr int kRight = kGlassW - kLeft;

constexpr int kTitleY = 2;
constexpr int kTitleRuleY = 12;
constexpr int kGnssTopY = 16;
constexpr int kLineH = 11;
constexpr int kGnssRows = 6;
constexpr int kBlockRuleY = kGnssTopY + kGnssRows * kLineH + 1;
constexpr int kRadioTopY = kBlockRuleY + 5;

void right_aligned(ui::Canvas& fb, int x_end, int y, const char* text, int len) {
    fb.draw_text(x_end - len * kCellW, y, text, true, 1);
}

void row(ui::Canvas& fb, int y, char* buf, int n) {
    buf[n] = 0;
    fb.draw_text(kLeft, y, buf, true, 1);
}

int fmt_word(char* out, const char* word) {
    int n = fmt_string(out, word);
    out[n++] = ' ';
    return n;
}

int fmt_keyed(char* out, const char* key, uint32_t value) {
    int n = fmt_word(out, key);
    n += fmt_uint(out + n, value);
    out[n++] = ' ';
    return n;
}

int fmt_dop(char* out, const char* key, uint16_t dop_e2) {
    int n = fmt_word(out, key);
    n += dop_e2 == 0 ? fmt_string(out + n, "---") : fmt_uint(out + n, dop_e2, 3, 2);
    out[n++] = ' ';
    return n;
}

int fmt_micros(char* out, const char* key, int32_t us) {
    int n = fmt_word(out, key);
    n += fmt_int(out + n, us, 1, 0, /*no_plus=*/true);
    n += fmt_string(out + n, "US ");
    return n;
}

const char* yes_no(bool on) { return on ? "Y" : "N"; }

const char* pps_word(PpsState pps) {
    switch (pps) {
        case PpsState::Lock: return "LOCK";
        case PpsState::Holdover: return "HOLD";
        case PpsState::None:
        default: return "NONE";
    }
}

const char* slot_word(timing::SlotState slot) {
    switch (slot) {
        case timing::SlotState::UplinkRxO: return "O-UP";
        case timing::SlotState::SwitchOtoM: return "O>M";
        case timing::SlotState::Slot0: return "M0";
        case timing::SlotState::Hop: return "HOP";
        case timing::SlotState::Slot1: return "M1";
        case timing::SlotState::SwitchMtoO: return "M>O";
    }
    return "?";
}

void draw_title(ui::Canvas& fb, const RawSnapshot& s) {
    fb.draw_text(kLeft, kTitleY, "RAW", true, 1);
    char buf[16];
    int n = fmt_string(buf, "T+");
    n += fmt_uint(buf + n, s.uptime_s % kUptimeClockWrapS);
    buf[n] = 0;
    right_aligned(fb, kRight, kTitleY, buf, n);
    fb.hline(kLeft, kTitleRuleY, kRight - kLeft, true);
}

void draw_port(ui::Canvas& fb, int y, const RawGnss& g) {
    char buf[40];
    int n = fmt_keyed(buf, "PORT", g.health.baud);
    n += fmt_word(buf + n, ports::to_string(g.health.config));
    n += fmt_keyed(buf + n, "OVR", g.health.overruns);
    n += fmt_word(buf + n, "ID");
    n += fmt_string(buf + n, yes_no(g.health.identified));
    row(fb, y, buf, n);
}

void draw_nav(ui::Canvas& fb, int y, const RawGnss& g) {
    char buf[40];
    int n = fmt_word(buf, "NAV");
    if (g.nav_valid) {
        n += fmt_uint(buf + n, g.nav_ms, 3);
        n += fmt_string(buf + n, "MS ");
    } else {
        n += fmt_string(buf + n, "--- ");
    }
    n += fmt_word(buf + n, "WIRE");
    n += fmt_uint(buf + n, g.health.pps_latency_ms);
    n += fmt_string(buf + n, "MS ");
    n += fmt_keyed(buf + n, "SENT", g.health.sentences);
    row(fb, y, buf, n);
}

void draw_fix(ui::Canvas& fb, int y, const RawGnss& g) {
    char buf[40];
    int n = fmt_word(buf, "FIX");
    n += fmt_word(buf + n, g.fix_valid ? "OK" : gnss::stage_name(g.stage));
    n += fmt_keyed(buf + n, "SV", g.sats);
    n += fmt_keyed(buf + n, "USE", g.in_use);
    n += fmt_keyed(buf + n, "Q", g.fix_mode);
    n += fmt_word(buf + n, "SET");
    n += fmt_string(buf + n, yes_no(g.settled));
    row(fb, y, buf, n);
}

void draw_quality(ui::Canvas& fb, int y, const RawGnss& g) {
    char buf[40];
    int n = fmt_dop(buf, "HDOP", g.hdop_e2);
    n += fmt_dop(buf + n, "VDOP", g.vdop_e2);
    n += fmt_word(buf + n, "RES");
    if (g.resid_valid) {
        n += fmt_uint(buf + n, g.resid_m);
        n += fmt_string(buf + n, "M");
    } else {
        n += fmt_string(buf + n, "---");
    }
    row(fb, y, buf, n);
}

void draw_refusals(ui::Canvas& fb, int y, const RawGnss& g) {
    char buf[40];
    int n = fmt_word(buf, "REJ");
    n += fmt_word(buf + n, gnss::reject_name(g.health.reject));
    n += fmt_keyed(buf + n, "N", g.health.rejected);
    n += fmt_keyed(buf + n, "SOL", g.solutions);
    n += fmt_word(buf + n, "GSV");
    n += fmt_string(buf + n, yes_no(g.levels_live));
    row(fb, y, buf, n);
}

void draw_clock(ui::Canvas& fb, int y, const RawGnss& g) {
    char buf[40];
    int n = fmt_word(buf, "PPS");
    n += fmt_word(buf + n, pps_word(g.pps));
    n += fmt_uint(buf + n, g.pps_age_ms);
    n += fmt_string(buf + n, "MS ");
    n += fmt_word(buf + n, "UTC");
    n += g.utc_valid ? fmt_seconds_of_day(buf + n, g.utc) : fmt_string(buf + n, "--:--:--");
    row(fb, y, buf, n);
}

void draw_dwell(ui::Canvas& fb, int y, const RawRadio& r) {
    char buf[40];
    int n = fmt_word(buf, "DWELL");
    n += fmt_word(buf + n, slot_word(r.slot));
    n += fmt_uint(buf + n, r.freq_hz / 100000 % 10000, 4, 1);
    n += fmt_string(buf + n, " TX ");
    n += fmt_string(buf + n, yes_no(r.tx_allowed));
    row(fb, y, buf, n);

    n = fmt_int(buf, r.noise_dbm, 1, 0, /*no_plus=*/true);
    n += fmt_string(buf + n, "DBM");
    buf[n] = 0;
    right_aligned(fb, kRight, y, buf, n);
}

void draw_rx(ui::Canvas& fb, int y, const RawRadio& r) {
    char buf[40];
    int n = fmt_keyed(buf, "RX", r.rx_ok);
    n += fmt_keyed(buf + n, "CRC", r.rx_bad);
    n += fmt_keyed(buf + n, "SYNC", r.rx_unframed);
    n += fmt_keyed(buf + n, "KEY", r.rx_miskeyed);
    row(fb, y, buf, n);
}

void draw_rx_verdicts(ui::Canvas& fb, int y, const RawRadio& r) {
    char buf[40];
    int n = fmt_keyed(buf, "DEC", r.rx_noise);
    n += fmt_keyed(buf + n, "TYPE", r.rx_type);
    n += fmt_keyed(buf + n, "WAIT", r.rx_wait);
    row(fb, y, buf, n);
}

void draw_uplink(ui::Canvas& fb, int y, const RawRadio& r) {
    char buf[40];
    int n = fmt_word(buf, "UPLINK");
    n += fmt_uint(buf + n, r.uplink_frames);
    buf[n++] = '/';
    n += fmt_uint(buf + n, r.uplink_bad);
    buf[n++] = '/';
    n += fmt_uint(buf + n, r.uplink_targets);
    buf[n++] = ' ';
    n += fmt_keyed(buf + n, "TRK", r.tracked);
    n += fmt_keyed(buf + n, "ALM", r.alarm);
    row(fb, y, buf, n);
}

void draw_tx(ui::Canvas& fb, int y, const RawRadio& r) {
    char buf[40];
    int n = fmt_keyed(buf, "TX", r.tx_ok);
    n += fmt_keyed(buf + n, "LOST", r.tx_lost);
    n += fmt_keyed(buf + n, "MISS", r.missed);
    n += fmt_keyed(buf + n, "HELD", r.refused);
    row(fb, y, buf, n);
}

void draw_burst(ui::Canvas& fb, int y, const RawRadio& r) {
    char buf[40];
    int n = fmt_word(buf, "DUTY");
    n += fmt_uint(buf + n, r.duty_permille);
    n += fmt_string(buf + n, "/1000 ");
    n += fmt_word(buf + n, "KEYED");
    n += fmt_uint(buf + n, r.tx_keyed_us);
    n += fmt_string(buf + n, "US");
    row(fb, y, buf, n);
}

void draw_error(ui::Canvas& fb, int y, const RawRadio& r) {
    char buf[40];
    int n = fmt_micros(buf, "SPAN", r.tx_span_us);
    n += fmt_micros(buf + n, "SLOT", r.dwell_worst_us);
    row(fb, y, buf, n);

    n = fmt_micros(buf, "PPS", r.pps_worst_us);
    n += fmt_keyed(buf + n, "HOLDOVER", r.holdover);
    row(fb, y + kLineH, buf, n - 1);
}

}  // namespace

void draw_raw(ui::Canvas& fb, const RawSnapshot& s) {
    fb.clear(true);
    draw_title(fb, s);

    draw_port(fb, kGnssTopY, s.gnss);
    draw_nav(fb, kGnssTopY + kLineH, s.gnss);
    draw_fix(fb, kGnssTopY + 2 * kLineH, s.gnss);
    draw_quality(fb, kGnssTopY + 3 * kLineH, s.gnss);
    draw_refusals(fb, kGnssTopY + 4 * kLineH, s.gnss);
    draw_clock(fb, kGnssTopY + 5 * kLineH, s.gnss);

    fb.hline(kLeft, kBlockRuleY, kRight - kLeft, true);

    draw_dwell(fb, kRadioTopY, s.radio);
    draw_rx(fb, kRadioTopY + kLineH, s.radio);
    draw_rx_verdicts(fb, kRadioTopY + 2 * kLineH, s.radio);
    draw_uplink(fb, kRadioTopY + 3 * kLineH, s.radio);
    draw_tx(fb, kRadioTopY + 4 * kLineH, s.radio);
    draw_burst(fb, kRadioTopY + 5 * kLineH, s.radio);
    draw_error(fb, kRadioTopY + 6 * kLineH, s.radio);
}

}  // namespace skyblip::go
