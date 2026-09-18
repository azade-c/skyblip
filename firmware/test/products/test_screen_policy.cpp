// The refresh policy over the real SSD1681 driver: partials only, every screen through black.
#include "core/events/input.h"
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

// The rails carry the ~140 ms power-down: skipping it is the whole saving of the shorter wipe.
TEST_CASE("screen policy: a wipe leaves the rails up, and the page behind it puts them down") {
    Rig rig;
    uint32_t t = 0;
    rig.run_seconds(t, 3);

    rig.screen.next_page();
    rig.tick(t += 1000);
    REQUIRE(rig.glass_all_black());
    CHECK(rig.chip.rails_on);

    // 500 ms of partial waveform less the 140 ms tail the wipe does not spend
    rig.tick(t += 400);
    REQUIRE_FALSE(rig.glass_all_black());
    CHECK_FALSE(rig.chip.rails_on);
}

TEST_CASE("screen policy: a second page change on a black glass costs no second wipe") {
    Rig rig;
    uint32_t t = 0;
    rig.run_seconds(t, 3);
    const int before = rig.chip.present_count;

    rig.screen.next_page();
    rig.tick(t += 1000);
    REQUIRE(rig.glass_all_black());

    rig.screen.next_page();
    rig.tick(t += 600);
    CHECK(rig.chip.present_count == before + 2);
    CHECK_FALSE(rig.glass_all_black());
    CHECK(rig.screen.page() == go::Page::Status);
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

// A radar ghosted over the page under it is worse than 360 ms of black the pilot asked for.
TEST_CASE("screen policy: the loudest alarm standing still gets the page its black") {
    Rig rig;
    uint32_t t = 0;
    rig.run_seconds(t, 3);

    rig.alarm(traffic::Level::Urgent);
    rig.screen.next_page();
    rig.tick(t += 1000);
    CHECK(rig.glass_all_black());
    CHECK_FALSE(rig.chip.last_full);

    rig.tick(t += 400);
    CHECK_FALSE(rig.glass_all_black());
    CHECK_FALSE(rig.chip.last_full);
}

// The settings mode keeps the button to itself, so it has to give it back unasked.
TEST_CASE("screen policy: converging traffic takes the settings mode back off the glass") {
    Rig rig;
    uint32_t t = 0;
    rig.run_seconds(t, 3);
    rig.bus.input.push(events::ContactEvent{events::Contact::Button, true, t});
    rig.bus.input.push(events::ContactEvent{events::Contact::Button, false, t + 100});
    rig.run_seconds(t, 2);
    REQUIRE(rig.screen.mode() == go::Mode::Settings);
    REQUIRE(rig.screen.editor().active());

    // An advisory is not worth taking a pilot's page away.
    rig.alarm(traffic::Level::Info);
    rig.run_seconds(t, 2);
    CHECK(rig.screen.mode() == go::Mode::Settings);

    // A bearing worth turning the head for is: the menu goes and the traffic picture comes back.
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

// The wedge flips once per frame presented, so the present floor is the blink rate.
TEST_CASE("screen policy: an alarm flashes the wedge at a flip a second") {
    Rig rig;
    uint32_t t = 0;
    rig.run_seconds(t, 3);
    const int settled = rig.chip.present_count;

    rig.threat(traffic::Level::Info, 0, 1500, t);
    for (int i = 0; i < 100; i++) rig.tick(t += 100);  // ten seconds, sampled ten times a second
    const int flips = rig.chip.present_count - settled;
    CHECK(flips >= 9);
    CHECK(flips <= 11);

    // Dismissed, the picture stands still: one last frame, then nothing.
    rig.state.alarm_dismissed = true;
    for (int i = 0; i < 20; i++) rig.tick(t += 100);
    const int held = rig.chip.present_count;
    for (int i = 0; i < 100; i++) rig.tick(t += 100);
    CHECK(rig.chip.present_count == held);
}
