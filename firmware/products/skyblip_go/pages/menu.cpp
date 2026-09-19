#include "products/skyblip_go/pages/menu.h"

#include <algorithm>

#include "core/util/format.h"

namespace skyblip::go {

namespace {

constexpr int kHeaderY = 3;
constexpr int kHeaderRuleY = 21;

constexpr MenuRow kRadarMenuRows[] = {MenuRow::Identity, MenuRow::AircraftType, MenuRow::Alarm,
                                      MenuRow::Volume,   MenuRow::Range,        MenuRow::Units,
                                      MenuRow::Stealth};
constexpr MenuRow kNearbyMenuRows[] = {MenuRow::RadioLog, MenuRow::Sats, MenuRow::Status,
                                       MenuRow::SelfTest};
constexpr MenuRow kSixPackMenuRows[] = {MenuRow::GMeter, MenuRow::Gyro, MenuRow::AlignQnh,
                                        MenuRow::QnhDown, MenuRow::QnhUp};

template <int N>
constexpr Menu menu_of(const MenuRow (&rows)[N]) {
    return Menu{rows, N};
}

int length(const char* s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

// INFO: cf 02aug26 ADS-L 4 SRD-860 issue 2 G.1.3 wire values: the code stored is transmitted
const char* const kAircraftNames[kNamedAircraftTypes] = {
    "UNKNOWN",    "LIGHT",      "HEAVY",    "HELICOPTER", "GLIDER",    "BALLOON",
    "MICROLIGHT", "PARAGLIDER", "SKYDIVER", "VTOL",       "GYROCOPTER"};

void row_text(ui::Canvas& fb, int line, const char* label, const char* value, bool focused) {
    const int top = menu_line_top(line);
    const int y = menu_line_text_y(line);
    if (focused) fb.rect(2, top, kGlassW - 4, kMenuRowHeight - 1, true, /*fill=*/true);
    const bool ink = !focused;
    fb.draw_text(kMenuLeftX, y, label, ink, 1);
    fb.draw_text(kMenuRightX - length(value) * kMenuCellW, y, value, ink, 1);
}

void step_box(ui::Canvas& fb, int x, int line, const char* mark, bool focused) {
    const int top = menu_line_top(line);
    const int y = menu_line_text_y(line);
    if (focused) fb.rect(x, top, kQnhStepBoxW, kMenuRowHeight - 1, true, /*fill=*/true);
    fb.draw_text(x + (kQnhStepBoxW - kMenuCellW) / 2, y, mark, !focused, 1);
}

void qnh_line(ui::Canvas& fb, int line, const MenuValues& values, MenuRow focus) {
    char value[kMenuValueCap];
    menu_row_value(value, MenuRow::QnhUp, values);
    fb.draw_text(kMenuLeftX, menu_line_text_y(line), menu_row_label(MenuRow::QnhUp), true, 1);
    step_box(fb, kQnhMinusX, line, "-", focus == MenuRow::QnhDown);
    fb.draw_text(kQnhValueX, menu_line_text_y(line), value, true, 1);
    step_box(fb, kQnhPlusX, line, "+", focus == MenuRow::QnhUp);
}

}  // namespace

Menu menu_for(Page page) {
    switch (page) {
        case Page::Radar: return menu_of(kRadarMenuRows);
        case Page::Nearby: return menu_of(kNearbyMenuRows);
        case Page::SixPack: return menu_of(kSixPackMenuRows);
        default: return Menu{};
    }
}

int menu_row_index(const Menu& menu, MenuRow row) {
    for (int i = 0; i < menu.n; i++)
        if (menu.rows[i] == row) return i;
    return -1;
}

int menu_row_line(const Menu& menu, MenuRow row) {
    int line = 0;
    for (int i = 0; i < menu.n; i++) {
        if (menu.rows[i] == row) return line;
        if (menu.rows[i] != MenuRow::QnhDown) line++;
    }
    return line;
}

const char* menu_row_label(MenuRow row) {
    switch (row) {
        case MenuRow::Identity: return "ID";
        case MenuRow::AircraftType: return "AIRCRAFT";
        case MenuRow::Alarm: return "ALARM";
        case MenuRow::Volume: return "VOLUME";
        case MenuRow::Range: return "RANGE";
        case MenuRow::Units: return "UNITS";
        case MenuRow::Stealth: return "STEALTH";
        case MenuRow::RadioLog: return "RADIO LOG";
        case MenuRow::Sats: return "SATELLITES";
        case MenuRow::Status: return "STATUS";
        case MenuRow::SelfTest: return "SELF TEST";
        case MenuRow::GMeter: return "G METER";
        case MenuRow::Gyro: return "GYRO";
        case MenuRow::AlignQnh: return "QNH FROM GNSS";
        case MenuRow::QnhDown:
        case MenuRow::QnhUp: return "QNH";
        default: return "";
    }
}

Page page_behind(MenuRow row) {
    switch (row) {
        case MenuRow::RadioLog: return Page::RadioLog;
        case MenuRow::Sats: return Page::Sats;
        case MenuRow::Status: return Page::Status;
        case MenuRow::SelfTest: return Page::SelfTest;
        case MenuRow::GMeter: return Page::GMeter;
        default: return Page::Radar;
    }
}

const char* aircraft_type_name(uint8_t code) {
    return code < kNamedAircraftTypes ? kAircraftNames[code] : "";
}

uint8_t next_aircraft_type(uint8_t code) {
    if (code + 1 >= kNamedAircraftTypes) return 0;
    return static_cast<uint8_t>(code + 1);
}

int32_t next_range_nm(int32_t nm) {
    for (int i = 0; i < kRangeStepCount; i++)
        if (kRangeStepsNm[i] == nm) return kRangeStepsNm[(i + 1) % kRangeStepCount];
    return kDefaultRangeNm;
}

uint32_t step_qnh_pa(uint32_t qnh_pa, bool up) {
    uint32_t whole = ((qnh_pa + kQnhStepPa / 2) / kQnhStepPa) * kQnhStepPa;
    whole = std::clamp(whole, kQnhMinPa, kQnhMaxPa);
    if (up) return whole + kQnhStepPa > kQnhMaxPa ? kQnhMaxPa : whole + kQnhStepPa;
    return whole < kQnhMinPa + kQnhStepPa ? kQnhMinPa : whole - kQnhStepPa;
}

// INFO: fc 18sep26 the subscale that makes the barometer read the GNSS altitude, not a QNH given
bool qnh_aligned_with_gnss(const MenuValues& values, uint32_t& out_pa) {
    if (!values.alignable) return false;
    uint32_t setting = 0;
    if (!flight::qnh_from_alt(values.pressure_pa, values.gnss_alt_cm, setting)) return false;
    setting = ((setting + kQnhStepPa / 2) / kQnhStepPa) * kQnhStepPa;
    if (setting < kQnhMinPa || setting > kQnhMaxPa) return false;
    out_pa = setting;
    return true;
}

int menu_row_value(char* out, MenuRow row, const MenuValues& v) {
    int n = 0;
    switch (row) {
        case MenuRow::Identity: n = fmt_hex(out, v.settings.device_addr, 6); break;
        case MenuRow::AircraftType: {
            const char* name = aircraft_type_name(v.settings.aircraft_type);
            if (name[0] != 0) {
                n = fmt_string(out, name);
            } else {
                n = fmt_string(out, "TYPE ");
                n += fmt_uint(out + n, v.settings.aircraft_type);
            }
            break;
        }
        case MenuRow::Alarm: n = fmt_string(out, v.settings.alarm_enabled ? "ON" : "OFF"); break;
        case MenuRow::Volume:
            n = fmt_uint(out, v.settings.alarm_volume);
            n += fmt_string(out + n, " OF 5");
            break;
        case MenuRow::Range:
            n = fmt_uint(out, static_cast<uint32_t>(v.range_nm));
            n += fmt_string(out + n, " NM");
            break;
        case MenuRow::Units:
            n = fmt_string(out, v.settings.units == go::Units::Metric ? "METRIC" : "NAUTICAL");
            break;
        case MenuRow::Stealth: n = fmt_string(out, v.settings.stealth ? "ON" : "OFF"); break;
        case MenuRow::Gyro: n = fmt_string(out, v.settings.gyro_enabled ? "ON" : "OFF"); break;
        case MenuRow::AlignQnh: {
            uint32_t aligned = 0;
            if (!qnh_aligned_with_gnss(v, aligned)) {
                n = fmt_string(out, "---");
                break;
            }
            n = fmt_uint(out, aligned / kQnhStepPa);
            n += fmt_string(out + n, " HPA");
            break;
        }
        case MenuRow::QnhDown:
        case MenuRow::QnhUp:
            n = fmt_uint(out, (v.qnh_pa + kQnhStepPa / 2) / kQnhStepPa);
            n += fmt_string(out + n, " HPA");
            break;
        default: break;
    }
    out[n] = 0;
    return n;
}

void draw_menu(ui::Canvas& fb, const MenuSnapshot& s) {
    fb.clear(true);
    fb.draw_text(kMenuLeftX - 2, kHeaderY, page_title(s.page), true, 2);
    fb.hline(kMenuLeftX - 2, kHeaderRuleY, kGlassW - 2 * (kMenuLeftX - 2), true);

    const Menu menu = menu_for(s.page);
    char value[kMenuValueCap];
    for (int i = 0; i < menu.n; i++) {
        const MenuRow row = menu.rows[i];
        const int line = menu_row_line(menu, row);
        if (row == MenuRow::QnhDown) {
            qnh_line(fb, line, s.values, s.focus);
            continue;
        }
        if (row == MenuRow::QnhUp) continue;
        menu_row_value(value, row, s.values);
        row_text(fb, line, menu_row_label(row), value, row == s.focus);
    }

    fb.draw_text(kMenuLeftX - 2, kMenuHintY, kMenuHintText, true, 1);
}

void MenuEditor::enter(Page page, uint32_t now_ms) {
    page_ = page;
    focus_ = 0;
    idle_since_ms_ = now_ms;
    active_ = menu_for(page).n > 0;
    pending_ = Pending::None;
}

void MenuEditor::leave() {
    focus_ = 0;
    active_ = false;
    pending_ = Pending::None;
}

MenuRow MenuEditor::focus() const {
    const Menu menu = menu_for(page_);
    if (focus_ < 0 || focus_ >= menu.n) return MenuRow::kCount;
    return menu.rows[focus_];
}

void MenuEditor::change(uint32_t now_ms) {
    if (!active_) return;
    idle_since_ms_ = now_ms;
    pending_ = Pending::Act;
}

void MenuEditor::next_row(uint32_t now_ms) {
    if (!active_) return;
    idle_since_ms_ = now_ms;
    pending_ = Pending::Advance;
}

MenuAction MenuEditor::tick(uint32_t now_ms, const MenuValues& current, MenuValues& next) {
    if (!active_) return MenuAction::None;
    const Pending pending = pending_;
    pending_ = Pending::None;
    if (pending == Pending::Act) return act(current, next);
    if (pending == Pending::Advance) return advance();
    if (now_ms - idle_since_ms_ >= kIdleReturnMs) {
        leave();
        return MenuAction::Leave;
    }
    return MenuAction::None;
}

MenuAction MenuEditor::advance() {
    if (focus_ + 1 >= menu_for(page_).n) {
        leave();
        return MenuAction::Leave;
    }
    focus_++;
    return MenuAction::Moved;
}

// INFO: cf 02aug26 one gate for every accepted value, go::validate, the same one a phone goes past
MenuAction MenuEditor::act(const MenuValues& current, MenuValues& next) {
    next = current;
    const MenuRow row = focus();
    if (opens_a_page(row)) {
        opening_ = page_behind(row);
        leave();
        return MenuAction::Open;
    }
    switch (row) {
        case MenuRow::Identity: return MenuAction::None;
        case MenuRow::AircraftType:
            next.settings.aircraft_type = next_aircraft_type(current.settings.aircraft_type);
            break;
        case MenuRow::Alarm: next.settings.alarm_enabled = !current.settings.alarm_enabled; break;
        case MenuRow::Volume:
            next.settings.alarm_volume =
                static_cast<uint8_t>((current.settings.alarm_volume + 1) % (kMaxAlarmVolume + 1));
            break;
        case MenuRow::Range: next.range_nm = next_range_nm(current.range_nm); break;
        case MenuRow::Units:
            next.settings.units = current.settings.units == go::Units::Metric ? go::Units::Nautical
                                                                              : go::Units::Metric;
            break;
        case MenuRow::Stealth: next.settings.stealth = !current.settings.stealth; break;
        case MenuRow::Gyro: next.settings.gyro_enabled = !current.settings.gyro_enabled; break;
        case MenuRow::AlignQnh: {
            uint32_t aligned = 0;
            if (!qnh_aligned_with_gnss(current, aligned) || aligned == current.qnh_pa)
                return MenuAction::None;
            next.qnh_pa = aligned;
            break;
        }
        case MenuRow::QnhUp:
        case MenuRow::QnhDown: {
            const uint32_t stepped = step_qnh_pa(current.qnh_pa, row == MenuRow::QnhUp);
            if (stepped == current.qnh_pa) return MenuAction::None;
            next.qnh_pa = stepped;
            break;
        }
        default: leave(); return MenuAction::Leave;
    }

    if (go::validate(next.settings) != Status::Ok || next.qnh_pa < kQnhMinPa ||
        next.qnh_pa > kQnhMaxPa) {
        next = current;
        return MenuAction::None;
    }
    return MenuAction::Changed;
}

}  // namespace skyblip::go
