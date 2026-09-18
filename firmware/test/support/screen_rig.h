// The screen service alone over the real SSD1681 driver and its model, state written by hand.
#ifndef SKYBLIP_TEST_SUPPORT_SCREEN_RIG_H
#define SKYBLIP_TEST_SUPPORT_SCREEN_RIG_H

#include "doctest/doctest.h"
#include "hardware/parts/ssd1681/model.h"
#include "hardware/parts/ssd1681/ssd1681.h"
#include "hardware/platform/host/clock.h"
#include "ports/null.h"
#include "products/skyblip_go/services/screen.h"
#include "products/skyblip_go/settings_store.h"

using namespace skyblip;

namespace {

// The screen service alone: state is written by hand, so alarm level and
// traffic are exactly what the case says, with no alarm service re-deriving
// them.
struct Rig {
    models::Ssd1681 chip;
    parts::Ssd1681 epd{chip, chip, chip.dc, chip.rst, chip.busy};
    platform::host::Clock clock;
    ports::NullRoles null;
    ports::Roles roles{
        clock,          null.rf,        null.link,        epd,  // epd fills Display
        null.kv,        null.log_flash, null.annunciator, null.dfu, null.die_temperature,
        null.indicator, null.gnss};
    bus::Bus bus{};
    bus::State state{};
    runtime::Context context{roles, bus, state};
    go::Settings settings{};
    go::SettingsStore store{settings};
    comms::ConfigService config{null.link, store};
    go::BootSnapshot self_test{};
    go::AlarmService alarm_service{context, settings};
    go::ScreenService screen{context, settings, config, alarm_service, self_test};

    Rig() {
        chip.attach_clock(clock);
        roles.capabilities = ports::Capability::Display;
        // With a fix the radar page draws rings and the range label, so churn()
        // below produces real pixel changes.
        state.own.fix_valid = true;
        state.own.sats = 9;
        state.flight.time_valid = true;
        epd.begin();
    }

    // The pass a service loop makes: the world's clock, then the service on it.
    void tick(uint32_t now_ms) {
        clock.set_millis(now_ms);
        screen.tick(now_ms);
    }

    // One service tick per second, the render cadence.
    void run_seconds(uint32_t& t, int seconds) {
        for (int i = 0; i < seconds; i++) tick(t += 1000);
    }

    // Forces a visible change every second, orthogonal to alarm level and traffic count.
    void churn(uint32_t& t, int seconds) { churn_at(t, 1000, seconds); }

    // The same flip at the caller's cadence, for the policies measured in minutes.
    void churn_at(uint32_t& t, uint32_t step_ms, int times) {
        for (int i = 0; i < times; i++) {
            state.flight.seconds += 60;
            tick(t += step_ms);
        }
    }

    bool glass_all_black() const {
        return chip.framebuffer().count_black() == go::kGlassW * go::kGlassH;
    }

    void alarm(traffic::Level level) { state.alarm_level = level; }

    void die_temperature(int16_t decicelsius) {
        state.power.die_dc = decicelsius;
        state.power.die_valid = true;
    }
};

}  // namespace

#endif
