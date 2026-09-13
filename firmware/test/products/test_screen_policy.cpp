// The refresh policy over the SSD1681: partials, swaps through black, a wash for an unparked glass.
#include "test/support/screen_rig.h"

TEST_CASE("screen policy: a static frame is never re-presented") {
    Rig rig;
    uint32_t t = 0;
    rig.run_seconds(t, 3);
    CHECK(rig.chip.present_count == 2);  // the boot black, then the page
    CHECK_FALSE(rig.chip.last_full);

    rig.run_seconds(t, 30);  // nothing on screen changes
    CHECK(rig.chip.present_count == 2);
}

TEST_CASE("screen policy: a minute of changing frames costs partials and no full") {
    Rig rig;
    uint32_t t = 0;
    rig.run_seconds(t, 3);  // boot full

    rig.churn(t, 60);
    CHECK(rig.chip.present_count > 1);
    CHECK_FALSE(rig.chip.last_full);
}

// SoftRF runs this glass on partials alone: the full waveform is power on and power off, no more.
TEST_CASE("screen policy: hours of changing frames never cost a full refresh") {
    Rig rig;
    uint32_t t = 0;
    rig.run_seconds(t, 3);  // boot full
    const int boot = rig.chip.present_count;

    rig.churn_at(t, 60000, 180);
    CHECK(rig.chip.present_count > boot);
    CHECK_FALSE(rig.chip.last_full);
}

TEST_CASE("screen policy: a page change goes through black, not through the full waveform") {
    Rig rig;
    uint32_t t = 0;
    rig.run_seconds(t, 3);
    const int before = rig.chip.present_count;

    rig.screen.next_page();
    rig.tick(t += 1000);
    CHECK(rig.chip.present_count == before + 1);
    CHECK_FALSE(rig.chip.last_full);
    CHECK(rig.glass_all_black());

    // The page behind it does not wait out the one-a-second floor.
    rig.tick(t += 600);
    CHECK(rig.chip.present_count == before + 2);
    CHECK_FALSE(rig.glass_all_black());
    CHECK_FALSE(rig.chip.last_full);
}

TEST_CASE("screen policy: a fix arriving is a data change, presented as a partial") {
    Rig rig;
    rig.state.own.fix_valid = false;
    uint32_t t = 0;
    rig.run_seconds(t, 3);
    const int before = rig.chip.present_count;

    rig.state.own.fix_valid = true;
    rig.tick(t += 1000);
    CHECK(rig.chip.present_count == before + 1);
    CHECK_FALSE(rig.glass_all_black());
    CHECK_FALSE(rig.chip.last_full);
}

TEST_CASE("screen policy: a page change under an alarm goes straight to the picture") {
    Rig rig;
    uint32_t t = 0;
    rig.run_seconds(t, 3);

    rig.alarm(2);
    rig.screen.next_page();
    rig.tick(t += 1000);
    CHECK_FALSE(rig.glass_all_black());
    CHECK_FALSE(rig.chip.last_full);
}

// The settings mode keeps the button to itself, so it has to give it back unasked.
TEST_CASE("screen policy: converging traffic takes the settings mode back off the glass") {
    Rig rig;
    uint32_t t = 0;
    rig.run_seconds(t, 3);
    rig.bus.input.push(messages::ButtonEvent{messages::kButtonPressed});
    rig.run_seconds(t, 2);
    REQUIRE(rig.screen.mode() == go::Mode::Settings);
    REQUIRE(rig.screen.editor().active());

    // An advisory is not worth taking a pilot's page away.
    rig.alarm(1);
    rig.run_seconds(t, 2);
    CHECK(rig.screen.mode() == go::Mode::Settings);

    // A bearing worth turning the head for is. The menu goes, the traffic
    // picture comes back, and no wash flashes while the alarm stands.
    rig.alarm(go::ScreenService::kAlarmTakesGlass);
    rig.run_seconds(t, 2);
    CHECK(rig.screen.mode() == go::Mode::Traffic);
    CHECK(rig.screen.page() == go::Page::Radar);
    CHECK_FALSE(rig.screen.editor().active());
    CHECK_FALSE(rig.chip.last_full);
}

TEST_CASE("screen policy: presents wait for the panel, none is issued mid-refresh") {
    Rig rig;
    uint32_t t = 0;
    rig.tick(t += 1000);  // boot full: the glass is busy 2.5 s
    const int count = rig.chip.present_count;

    rig.state.own.sats = 8;  // a visible change, right away
    rig.screen.mark_dirty();
    rig.tick(t += 100);  // 1.1 s: full not settled yet
    CHECK(rig.chip.present_count == count);

    rig.tick(t += 2700);  // settled: the pending change lands
    CHECK(rig.chip.present_count == count + 1);
}

// A pilot switching on wants the page, not 2.6 s of wash: the mark says the glass was left parked.
TEST_CASE("screen policy: a session after a park that landed opens on the black, not a full") {
    Rig rig;
    rig.leave_park_mark();
    rig.screen.open_glass();

    uint32_t t = 0;
    rig.run_seconds(t, 3);
    CHECK(rig.chip.present_count == 2);  // the black, then the page
    CHECK_FALSE(rig.chip.last_full);
    // Taken at boot, so a cell pulled now leaves the session after this one nothing to find.
    CHECK_FALSE(rig.park_marked());
}

// A cell pulled mid-page leaves that page on the glass for months, and a partial cannot lift it.
TEST_CASE("screen policy: a session after a park that never landed opens on a full") {
    Rig rig;
    rig.screen.open_glass();

    uint32_t t = 0;
    rig.tick(t += 1000);
    CHECK(rig.chip.present_count == 1);
    CHECK(rig.chip.last_full);
    CHECK(rig.chip.framebuffer().count_black() == 0);

    rig.run_seconds(t, 4);
    CHECK(rig.chip.present_count == 3);  // the full, the black, then the page
    CHECK_FALSE(rig.chip.last_full);
}

TEST_CASE("screen policy: the park frame that landed leaves the mark the next session takes") {
    Rig rig;
    uint32_t t = 0;
    rig.run_seconds(t, 3);
    REQUIRE_FALSE(rig.park_marked());

    rig.screen.set_power(false);
    rig.run_seconds(t, 6);
    CHECK(rig.chip.last_full);
    CHECK(rig.park_marked());
}

// A panel too hot to drive parks without a frame, and what stays on the glass is the page.
TEST_CASE("screen policy: a park that never put a frame on the glass leaves no mark") {
    Rig rig;
    uint32_t t = 0;
    rig.run_seconds(t, 3);
    rig.die_temperature(go::ScreenService::kHoldAboveDeciCelsius + 10);

    rig.screen.set_power(false);
    rig.run_seconds(t, 6);
    CHECK_FALSE(rig.park_marked());
}

// THE RULE, core/power/cutoff.h: the sector the settings live on is not written on a dying cell.
TEST_CASE("screen policy: a park on a flat cell leaves no mark, and the next session pays a full") {
    Rig rig;
    uint32_t t = 0;
    rig.run_seconds(t, 3);
    rig.state.power_level = power::PowerLevel::Cutoff;

    rig.screen.set_power(false);
    rig.run_seconds(t, 6);
    CHECK(rig.chip.last_full);
    CHECK_FALSE(rig.park_marked());
}
