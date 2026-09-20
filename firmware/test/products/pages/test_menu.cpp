// The menu behind each page: which row is focused, what the pad moves and what the button changes.
#include <cstring>
#include <initializer_list>
#include <string>

#include "doctest/doctest.h"
#include "products/skyblip_go/glass.h"
#include "products/skyblip_go/pages/menu.h"
#include "products/skyblip_go/settings.h"

using namespace skyblip;
using namespace skyblip::go;

namespace {

constexpr int kGlyphH = 7;

int length(const char* s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

// Draw the same text at the same place in a scratch buffer and compare the box it occupies.
bool reads_at(const Glass& fb, int x, int y, const char* text, bool ink, int scale = 1) {
    Glass expected;
    expected.clear(true);
    if (!ink) expected.clear(false);
    expected.draw_text(x, y, text, ink, scale);
    for (int dy = 0; dy < kGlyphH * scale; dy++)
        for (int dx = 0; dx < length(text) * kSmallCellW * scale; dx++)
            if (fb.get_pixel(x + dx, y + dy) != expected.get_pixel(x + dx, y + dy)) return false;
    return true;
}

int line_of(Page page, MenuRow row) { return menu_row_index(menu_for(page), row); }

bool row_label_reads(const Glass& fb, Page page, MenuRow row, bool focused) {
    return reads_at(fb, kMenuLeftX, menu_line_text_y(line_of(page, row)), menu_row_label(row),
                    !focused, kMenuScale);
}

bool row_value_reads(const Glass& fb, Page page, MenuRow row, const char* value, bool focused) {
    return reads_at(fb, kMenuRightX - length(value) * kMenuCellW,
                    menu_line_text_y(line_of(page, row)), value, !focused, kMenuScale);
}

MenuValues fresh() {
    MenuValues v;
    v.settings = go::defaults();
    return v;
}

Glass page_of(Page page, const MenuValues& values, MenuRow focus) {
    MenuSnapshot snapshot;
    snapshot.page = page;
    snapshot.values = values;
    snapshot.focus = focus;
    Glass fb;
    draw_menu(fb, snapshot);
    return fb;
}

// The editor with a clock the case advances, and the values it hands back applied.
struct Bench {
    MenuEditor editor;
    MenuValues values{fresh()};
    uint32_t t{1000};
    Page page{Page::Radar};

    explicit Bench(Page on = Page::Radar) : page(on) { editor.enter(on, t); }

    MenuAction run(uint32_t ms) {
        MenuAction seen = MenuAction::None;
        for (uint32_t i = 0; i < ms; i += 10) {
            t += 10;
            MenuValues next;
            const MenuAction action = editor.tick(t, values, next);
            if (action == MenuAction::Changed) values = next;
            if (action != MenuAction::None) seen = action;
        }
        return seen;
    }

    // A tap of the pad: the focus moves down a row.
    MenuAction move() {
        editor.pad(t);
        return run(100);
    }

    // A press of the button: the focused row is acted on, and again on every further press.
    MenuAction change() {
        editor.button(t);
        return run(100);
    }

    int rows() const { return menu_for(page).n; }

    void focus_on(MenuRow row) {
        for (int i = 0; i < rows() && editor.focus() != row; i++) move();
        REQUIRE(editor.focus() == row);
    }
};

}  // namespace

TEST_CASE("radar menu: every row names what it holds, and the focused one is reversed out") {
    MenuValues values = fresh();
    values.settings.aircraft_type = 4;
    values.settings.alarm_volume = 3;

    const Glass fb = page_of(Page::Radar, values, MenuRow::Volume);

    CHECK(row_label_reads(fb, Page::Radar, MenuRow::AircraftType, false));
    CHECK(row_value_reads(fb, Page::Radar, MenuRow::AircraftType, "GLIDER", false));
    CHECK(row_label_reads(fb, Page::Radar, MenuRow::Alarm, false));
    CHECK(row_value_reads(fb, Page::Radar, MenuRow::Alarm, "ON", false));
    CHECK(row_value_reads(fb, Page::Radar, MenuRow::Range, "4 NM", false));
    CHECK(row_value_reads(fb, Page::Radar, MenuRow::Units, "NAUTICAL", false));

    // The focused row is white ink on a filled bar, told apart by shape before a word is read.
    CHECK(row_label_reads(fb, Page::Radar, MenuRow::Volume, true));
    CHECK(row_value_reads(fb, Page::Radar, MenuRow::Volume, "3 OF 5", true));
    CHECK_FALSE(row_label_reads(fb, Page::Radar, MenuRow::Volume, false));

    // Exactly one bar, and the page says how the two contacts work.
    const Menu menu = menu_for(Page::Radar);
    int bars = 0;
    for (int i = 0; i < menu.n; i++)
        if (row_label_reads(fb, Page::Radar, menu.rows[i], true)) bars++;
    CHECK(bars == 1);
    CHECK(reads_at(fb, kMenuHintX, kMenuHintY, kMenuHintText, true));

    // And nothing falls off a 200 pixel panel, on either axis.
    CHECK(menu_line_top(menu.n - 1) + kMenuRowHeight <= kMenuHintY);
    CHECK(kMenuHintY + kGlyphH < Glass::kH);

    // The hint is centred: the same air either side of it.
    const int hint_w = length(kMenuHintText) * kSmallCellW;
    CHECK(kMenuHintX > 0);
    CHECK(kMenuHintX - (Glass::kW - kMenuHintX - hint_w) <= 1);
}

TEST_CASE("menu: no label and value a row can hold meet at double height") {
    MenuValues values = fresh();
    for (Page page : {Page::Radar, Page::Nearby}) {
        const Menu menu = menu_for(page);
        for (int i = 0; i < menu.n; i++) {
            const int label_end = kMenuLeftX + length(menu_row_label(menu.rows[i])) * kMenuCellW;
            CHECK(label_end <= kMenuRightX);
            for (uint8_t type = 0; type < kNamedAircraftTypes + 2; type++) {
                values.settings.aircraft_type = type;
                for (Units units : {Units::Nautical, Units::Metric}) {
                    values.settings.units = units;
                    for (int step = 0; step < kRangeStepCount; step++) {
                        values.range_step = step;
                        char value[kMenuValueCap];
                        const int n = menu_row_value(value, menu.rows[i], values);
                        CHECK(label_end <= kMenuRightX - n * kMenuCellW);
                    }
                }
            }
        }
    }
}

TEST_CASE("menu: a menu is titled what it holds, not the page a thumb came from") {
    CHECK(std::strcmp(menu_title(Page::Radar), "SETTINGS") == 0);
    CHECK(std::strcmp(menu_title(Page::Nearby), "DIAGNOSTICS") == 0);

    const MenuValues values = fresh();
    for (Page page : {Page::Radar, Page::Nearby}) {
        const Glass fb = page_of(page, values, menu_for(page).rows[0]);
        CHECK(kMenuLeftX - 2 + length(menu_title(page)) * kMenuCellW <= Glass::kW);
        Glass expected;
        expected.clear(true);
        expected.draw_text(kMenuLeftX - 2, 3, menu_title(page), true, 2);
        for (int y = 3; y < 3 + kGlyphH * kMenuScale; y++)
            for (int x = 0; x < Glass::kW; x++)
                CHECK(fb.get_pixel(x, y) == expected.get_pixel(x, y));
    }
}

TEST_CASE("nearby menu: every row opens a page rather than changing a value") {
    const Menu menu = menu_for(Page::Nearby);
    REQUIRE(menu.n == 6);
    for (int i = 0; i < menu.n; i++) CHECK(opens_a_page(menu.rows[i]));
    CHECK(page_behind(MenuRow::RadioLog) == Page::RadioLog);
    CHECK(page_behind(MenuRow::Raw) == Page::Raw);
    CHECK(page_behind(MenuRow::Capture) == Page::Capture);
    CHECK(page_behind(MenuRow::Sats) == Page::Sats);
    CHECK(page_behind(MenuRow::Status) == Page::Status);
    CHECK(page_behind(MenuRow::SelfTest) == Page::SelfTest);

    // A press hands the page back to the caller and closes the menu behind it.
    Bench bench(Page::Nearby);
    bench.focus_on(MenuRow::Status);
    CHECK(bench.change() == MenuAction::Open);
    CHECK(bench.editor.opening() == Page::Status);
    CHECK_FALSE(bench.editor.active());
}

TEST_CASE("menu: a page with nothing behind it opens no menu at all") {
    for (Page page : {Page::SixPack, Page::GMeter, Page::Status, Page::Sats, Page::RadioLog,
                      Page::Raw, Page::SelfTest})
        CHECK(menu_for(page).n == 0);

    MenuEditor editor;
    editor.enter(Page::Status, 1000);
    CHECK_FALSE(editor.active());
}

TEST_CASE("menu: a category the phone stored but the page does not name is still shown") {
    MenuValues values = fresh();
    values.settings.aircraft_type = 13;
    const Glass fb = page_of(Page::Radar, values, MenuRow::Alarm);
    CHECK(row_value_reads(fb, Page::Radar, MenuRow::AircraftType, "CODE 13", false));

    // The first change moves it into the list the page can name.
    CHECK(next_aircraft_type(13) == 0);
}

TEST_CASE("menu: the UAV categories are not a choice a pilot can make on the panel") {
    MenuValues values = fresh();
    values.settings.aircraft_type = 11;
    const Glass fb = page_of(Page::Radar, values, MenuRow::Alarm);
    CHECK(row_value_reads(fb, Page::Radar, MenuRow::AircraftType, "CODE 11", false));
    CHECK(next_aircraft_type(11) == 0);
}

TEST_CASE("menu editor: the pad moves down a row, the button changes the row it is on") {
    Bench bench;
    CHECK(bench.editor.focus() == MenuRow::AircraftType);

    CHECK(bench.move() == MenuAction::Moved);
    CHECK(bench.editor.focus() == MenuRow::Callsign);
    CHECK(bench.move() == MenuAction::Moved);
    CHECK(bench.editor.focus() == MenuRow::Units);
    CHECK(bench.move() == MenuAction::Moved);
    CHECK(bench.editor.focus() == MenuRow::Range);
    CHECK(bench.move() == MenuAction::Moved);
    CHECK(bench.editor.focus() == MenuRow::Alarm);

    // The press acts on the row the focus is on, and leaves the focus there.
    CHECK(bench.values.settings.alarm_enabled);
    CHECK(bench.change() == MenuAction::Changed);
    CHECK(bench.editor.focus() == MenuRow::Alarm);
    CHECK_FALSE(bench.values.settings.alarm_enabled);

    // The next press toggles it back: one press, one step, no rhythm to get right.
    CHECK(bench.change() == MenuAction::Changed);
    CHECK(bench.values.settings.alarm_enabled);
}

// The field a pilot types into, which is the one screen on this device that has a cursor.
TEST_CASE("callsign page: the characters stand at a size a thumb can check, with a bar under one") {
    CallsignSnapshot field;
    field.text = "F-JABC   ";
    field.cursor = 3;

    Glass fb;
    draw_callsign(fb, field);
    CHECK(reads_at(fb, kMenuLeftX - 2, 3, "CALLSIGN", true, kMenuScale));
    CHECK(reads_at(fb, kCallsignTextX, kCallsignTextY, "F-JABC", true, kCallsignScale));

    // The bar is under the character the pad is rolling and under no other.
    const int at = kCallsignTextX + field.cursor * kCallsignCellW;
    CHECK(fb.get_pixel(at, kCallsignCursorY));
    CHECK(fb.get_pixel(at + 5 * kCallsignScale - 1, kCallsignCursorY));
    CHECK_FALSE(fb.get_pixel(at - 2, kCallsignCursorY));
    CHECK_FALSE(fb.get_pixel(at + kCallsignCellW, kCallsignCursorY));
    CHECK(reads_at(fb, kCallsignHintX, kMenuHintY, kCallsignHintText, true));
}

// The ring a pad rolls: blank, dash, letters, digits, and round again.
TEST_CASE("menu: the callsign ring is blank, dash, A to Z, 0 to 9, and back") {
    CHECK(next_callsign_char(' ') == '-');
    CHECK(next_callsign_char('-') == 'A');
    CHECK(next_callsign_char('A') == 'B');
    CHECK(next_callsign_char('Z') == '0');
    CHECK(next_callsign_char('0') == '1');
    CHECK(next_callsign_char('9') == ' ');
}

// Nine characters are edited and what is stored is what a pilot typed.
TEST_CASE("menu: a stored callsign keeps its blanks inside and drops the ones at the end") {
    char out[kCallsignCap] = {0};
    CHECK(callsign_stored(out, "F-JABC   ") == 6);
    CHECK(std::string(out) == "F-JABC");
    CHECK(callsign_stored(out, "         ") == 0);
    CHECK(out[0] == 0);
    CHECK(callsign_stored(out, "F-J ABC  ") == 7);
    CHECK(std::string(out) == "F-J ABC");
    CHECK(callsign_stored(out, "ABCDEFGHI") == 9);
    CHECK(std::string(out) == "ABCDEFGHI");
}

TEST_CASE("menu editor: the callsign is rolled a character at a time and stored at the end") {
    Bench bench;
    bench.focus_on(MenuRow::Callsign);
    REQUIRE(bench.change() == MenuAction::Moved);
    REQUIRE(bench.editor.editing());
    CHECK(std::string(bench.editor.text()) == "         ");
    CHECK(bench.editor.cursor() == 0);

    // The pad rolls this character and moves nothing: the field is not the rows.
    const MenuRow focused = bench.editor.focus();
    for (int i = 0; i < 6; i++) bench.move();
    CHECK(bench.editor.focus() == focused);
    CHECK(bench.editor.text()[0] == 'E');
    CHECK(bench.editor.cursor() == 0);

    // The button steps to the next character, and the last one stores the name.
    CHECK(bench.change() == MenuAction::Moved);
    CHECK(bench.editor.cursor() == 1);
    bench.move();
    CHECK(bench.editor.text()[1] == '-');
    for (int i = 2; i < kCallsignChars; i++) CHECK(bench.change() == MenuAction::Moved);
    CHECK(bench.editor.cursor() == kCallsignChars - 1);

    CHECK(bench.change() == MenuAction::Changed);
    CHECK_FALSE(bench.editor.editing());
    CHECK(std::string(bench.values.settings.callsign) == "E-");
    // And the row it came from is the row the focus is still on.
    CHECK(bench.editor.focus() == MenuRow::Callsign);
}

// A name already stored is what the field opens on, not nine blanks.
TEST_CASE("menu editor: the callsign field opens on what is stored") {
    Bench bench;
    std::memcpy(bench.values.settings.callsign, "F-JABC", 7);
    bench.focus_on(MenuRow::Callsign);
    REQUIRE(bench.change() == MenuAction::Moved);
    CHECK(std::string(bench.editor.text()) == "F-JABC   ");

    // Walked through unchanged, it is stored as it was.
    for (int i = 1; i < kCallsignChars; i++) bench.change();
    CHECK(bench.change() == MenuAction::Changed);
    CHECK(std::string(bench.values.settings.callsign) == "F-JABC");
}

// The pad's long touch and the idle timer both leave, and neither stores.
TEST_CASE("menu editor: a field nobody finished stores nothing") {
    Bench bench;
    std::memcpy(bench.values.settings.callsign, "F-JABC", 7);
    bench.focus_on(MenuRow::Callsign);
    REQUIRE(bench.change() == MenuAction::Moved);
    for (int i = 0; i < 3; i++) bench.move();
    REQUIRE(bench.editor.text()[0] != 'F');

    CHECK(bench.run(MenuEditor::kIdleReturnMs + 100) == MenuAction::Leave);
    CHECK_FALSE(bench.editor.editing());
    CHECK(std::string(bench.values.settings.callsign) == "F-JABC");
}

TEST_CASE("menu editor: the focus only ever advances, and walks out of the menu") {
    Bench bench;
    for (int i = 1; i < bench.rows(); i++) {
        CHECK(bench.move() == MenuAction::Moved);
        CHECK(bench.editor.focus() == menu_for(Page::Radar).rows[i]);
    }
    REQUIRE(bench.editor.focus() == MenuRow::Volume);

    // One more tap leaves: a thumb that only knows the pad cannot be trapped here.
    CHECK(bench.move() == MenuAction::Leave);
    CHECK_FALSE(bench.editor.active());

    // The next entry starts at the top again, so the focus cycle is closed.
    bench.editor.enter(Page::Radar, bench.t);
    CHECK(bench.editor.focus() == MenuRow::AircraftType);
    CHECK(bench.editor.active());
}

TEST_CASE("menu editor: moving the focus over a row is not editing it") {
    // The abandoned edit: nothing is staged, so nothing is half applied.
    Bench bench;
    const go::Settings before = bench.values.settings;
    const int range_before = bench.values.range_step;
    for (int i = 0; i < bench.rows(); i++) bench.move();
    CHECK_FALSE(bench.editor.active());
    CHECK(before.aircraft_type == bench.values.settings.aircraft_type);
    CHECK(before.alarm_enabled == bench.values.settings.alarm_enabled);
    CHECK(before.alarm_volume == bench.values.settings.alarm_volume);
    CHECK(before.units == bench.values.settings.units);
    CHECK(range_before == bench.values.range_step);
}

TEST_CASE("menu editor: aircraft type walks the categories that name an aircraft") {
    Bench bench;
    bench.focus_on(MenuRow::AircraftType);
    REQUIRE(bench.values.settings.aircraft_type == go::kAircraftTypeLight);

    CHECK(bench.change() == MenuAction::Changed);
    CHECK(bench.values.settings.aircraft_type == 2);

    // Pressing on keeps stepping the same row rather than walking away from it.
    CHECK(bench.change() == MenuAction::Changed);
    CHECK(bench.values.settings.aircraft_type == 3);
    CHECK(bench.editor.focus() == MenuRow::AircraftType);

    // Every step is a code the page can name, and the list closes.
    for (int i = 0; i < kNamedAircraftTypes; i++) {
        CHECK(bench.values.settings.aircraft_type < kNamedAircraftTypes);
        CHECK(aircraft_type_name(bench.values.settings.aircraft_type)[0] != 0);
        CHECK(go::validate(bench.values.settings) == Status::Ok);
        bench.change();
    }
    CHECK(bench.values.settings.aircraft_type == 3);
}

TEST_CASE("menu editor: the volume a pilot can hear, and it stays inside what is valid") {
    Bench bench;
    bench.focus_on(MenuRow::Volume);
    REQUIRE(bench.values.settings.alarm_volume == 3);

    bench.change();
    CHECK(bench.values.settings.alarm_volume == 4);
    bench.change();
    CHECK(bench.values.settings.alarm_volume == 5);

    // Off the top it comes back to silent rather than to a value the validator would refuse.
    bench.change();
    CHECK(bench.values.settings.alarm_volume == 0);
    for (int i = 0; i <= kMaxAlarmVolume + 1; i++) {
        CHECK(bench.values.settings.alarm_volume <= kMaxAlarmVolume);
        CHECK(go::validate(bench.values.settings) == Status::Ok);
        bench.change();
    }
}

TEST_CASE("menu editor: the ring is a range a thumb can step, and the cycle closes") {
    Bench bench;
    bench.focus_on(MenuRow::Range);
    REQUIRE(bench.values.range_step == kDefaultRangeStep);
    REQUIRE(range_value(bench.values.range_step, Units::Nautical) == 4);

    CHECK(bench.change() == MenuAction::Changed);
    CHECK(range_value(bench.values.range_step, Units::Nautical) == 8);
    CHECK(bench.change() == MenuAction::Changed);
    CHECK(range_value(bench.values.range_step, Units::Nautical) == 1);

    // Every step is a range the radar can label, and the cycle comes back round.
    for (int i = 0; i < kRangeStepCount; i++) {
        CHECK(bench.values.range_step >= 0);
        CHECK(bench.values.range_step < kRangeStepCount);
        bench.change();
    }
    CHECK(range_value(bench.values.range_step, Units::Nautical) == 1);

    // A step a companion app invented is not on the cycle, and the first press comes back to it.
    CHECK(next_range_step(37) == (kDefaultRangeStep + 1) % kRangeStepCount);
}

// The ring is picked in the unit it is read in, so switching units keeps the step a pilot chose.
TEST_CASE("menu editor: a metric pilot steps whole kilometres, not a converted mile") {
    Bench bench;
    bench.values.settings.units = Units::Metric;
    bench.focus_on(MenuRow::Range);

    const Glass fb = page_of(Page::Radar, bench.values, MenuRow::Range);
    CHECK(row_value_reads(fb, Page::Radar, MenuRow::Range, "8 KM", true));

    CHECK(bench.change() == MenuAction::Changed);
    CHECK(range_value(bench.values.range_step, Units::Metric) == 16);
    CHECK(range_metres(bench.values.range_step, Units::Metric) == 16000);

    // The same step read in the other unit is the ring it was drawn from.
    CHECK(range_value(bench.values.range_step, Units::Nautical) == 8);
    CHECK(range_metres(bench.values.range_step, Units::Nautical) == 8 * kMetresPerNm);
}

TEST_CASE("menu editor: a value that would not validate is never handed back") {
    // The menu reads and does not write: it hands back no blob the firmware would refuse.
    Bench bench;
    bench.values.settings.aircraft_type = 200;
    bench.focus_on(MenuRow::Volume);
    REQUIRE(go::validate(bench.values.settings) != Status::Ok);

    const uint8_t volume = bench.values.settings.alarm_volume;
    CHECK(bench.change() == MenuAction::None);
    CHECK(bench.values.settings.alarm_volume == volume);
    CHECK(bench.values.settings.aircraft_type == 200);
}

TEST_CASE("menu editor: a menu nobody is pressing hands the traffic picture back") {
    Bench bench;
    bench.move();
    REQUIRE(bench.editor.active());

    CHECK(bench.run(MenuEditor::kIdleReturnMs - 1000) == MenuAction::None);
    CHECK(bench.editor.active());
    CHECK(bench.run(2000) == MenuAction::Leave);
    CHECK_FALSE(bench.editor.active());

    // Either contact resets the clock: a pilot working the rows is never dropped.
    bench.editor.enter(Page::Radar, bench.t);
    for (int i = 0; i < 3; i++) {
        bench.run(MenuEditor::kIdleReturnMs - 5000);
        bench.editor.button(bench.t);
        bench.run(100);
        bench.run(MenuEditor::kIdleReturnMs - 5000);
        bench.editor.pad(bench.t);
        bench.run(100);
    }
    CHECK(bench.editor.active());
}
