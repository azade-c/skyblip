#include "products/skyblip_go/pages/menu.h"

#include "core/util/format.h"

namespace skyblip::go {

namespace {

constexpr int kHeaderY = 3;
constexpr int kHeaderRuleY = 21;

constexpr MenuRow kRadarMenuRows[] = {MenuRow::AircraftType, MenuRow::Units, MenuRow::Range,
                                      MenuRow::Alarm, MenuRow::Volume};
constexpr MenuRow kNearbyMenuRows[] = {MenuRow::Status, MenuRow::Sats, MenuRow::RadioLog,
                                       MenuRow::SelfTest};

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

}  // namespace

Menu menu_for(Page page) {
    switch (page) {
        case Page::Radar: return menu_of(kRadarMenuRows);
        case Page::Nearby: return menu_of(kNearbyMenuRows);
        default: return Menu{};
    }
}

int menu_row_index(const Menu& menu, MenuRow row) {
    for (int i = 0; i < menu.n; i++)
        if (menu.rows[i] == row) return i;
    return -1;
}

const char* menu_row_label(MenuRow row) {
    switch (row) {
        case MenuRow::AircraftType: return "AIRCRAFT";
        case MenuRow::Units: return "UNITS";
        case MenuRow::Range: return "RANGE";
        case MenuRow::Alarm: return "ALARM";
        case MenuRow::Volume: return "VOLUME";
        case MenuRow::Status: return "STATUS";
        case MenuRow::Sats: return "SATELLITES";
        case MenuRow::RadioLog: return "RADIO LOG";
        case MenuRow::SelfTest: return "SELF TEST";
        default: return "";
    }
}

Page page_behind(MenuRow row) {
    switch (row) {
        case MenuRow::RadioLog: return Page::RadioLog;
        case MenuRow::Sats: return Page::Sats;
        case MenuRow::Status: return Page::Status;
        case MenuRow::SelfTest: return Page::SelfTest;
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

int menu_row_value(char* out, MenuRow row, const MenuValues& v) {
    int n = 0;
    switch (row) {
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
        case MenuRow::Units:
            n = fmt_string(out, v.settings.units == go::Units::Metric ? "METRIC" : "NAUTICAL");
            break;
        case MenuRow::Range:
            n = fmt_uint(out, static_cast<uint32_t>(range_value(v.range_step, v.settings.units)));
            n += fmt_string(out + n, " ");
            n += fmt_string(out + n, range_unit(v.settings.units));
            break;
        case MenuRow::Alarm: n = fmt_string(out, v.settings.alarm_enabled ? "ON" : "OFF"); break;
        case MenuRow::Volume:
            n = fmt_uint(out, v.settings.alarm_volume);
            n += fmt_string(out + n, " OF 5");
            break;
        default: break;
    }
    out[n] = 0;
    return n;
}

void draw_menu(ui::Canvas& fb, const MenuSnapshot& s) {
    fb.clear(true);
    fb.draw_text(kMenuLeftX - 2, kHeaderY, menu_title(s.page), true, 2);
    fb.hline(kMenuLeftX - 2, kHeaderRuleY, kGlassW - 2 * (kMenuLeftX - 2), true);

    const Menu menu = menu_for(s.page);
    char value[kMenuValueCap];
    for (int i = 0; i < menu.n; i++) {
        const MenuRow row = menu.rows[i];
        menu_row_value(value, row, s.values);
        row_text(fb, i, menu_row_label(row), value, row == s.focus);
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
        case MenuRow::AircraftType:
            next.settings.aircraft_type = next_aircraft_type(current.settings.aircraft_type);
            break;
        case MenuRow::Units:
            next.settings.units = current.settings.units == go::Units::Metric ? go::Units::Nautical
                                                                              : go::Units::Metric;
            break;
        case MenuRow::Range: next.range_step = next_range_step(current.range_step); break;
        case MenuRow::Alarm: next.settings.alarm_enabled = !current.settings.alarm_enabled; break;
        case MenuRow::Volume:
            next.settings.alarm_volume =
                static_cast<uint8_t>((current.settings.alarm_volume + 1) % (kMaxAlarmVolume + 1));
            break;
        default: leave(); return MenuAction::Leave;
    }

    if (go::validate(next.settings) != Status::Ok) {
        next = current;
        return MenuAction::None;
    }
    return MenuAction::Changed;
}

}  // namespace skyblip::go
