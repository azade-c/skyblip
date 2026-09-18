#ifndef SKYBLIP_PRODUCTS_SKYBLIP_GO_SERVICES_SCREEN_H
#define SKYBLIP_PRODUCTS_SKYBLIP_GO_SERVICES_SCREEN_H

#include "core/comms/config.h"
#include "core/flight/state.h"
#include "core/units/units.h"
#include "products/skyblip_go/glass.h"
#include "products/skyblip_go/input/controls.h"
#include "products/skyblip_go/input/gesture.h"
#include "products/skyblip_go/pages/boot.h"
#include "products/skyblip_go/pages/confirm.h"
#include "products/skyblip_go/pages/gmeter.h"
#include "products/skyblip_go/pages/menu.h"
#include "products/skyblip_go/pages/nearby.h"
#include "products/skyblip_go/pages/page.h"
#include "products/skyblip_go/pages/radar.h"
#include "products/skyblip_go/pages/radio_log.h"
#include "products/skyblip_go/pages/sats.h"
#include "products/skyblip_go/pages/sixpack.h"
#include "products/skyblip_go/pages/status.h"
#include "products/skyblip_go/services/alarm.h"
#include "runtime/service.h"

namespace skyblip::go {

enum class Mode : uint8_t { Page, Menu };

class ScreenService : public runtime::Service {
   public:
    static constexpr uint32_t kRenderPeriodMs = 1000;
    static constexpr uint32_t kPresentFloorMs = 1000;

    // INFO: fc 06sep26 Good Display rates the glass 0..50 C, read on a die above ambient
    static constexpr int16_t kHoldAboveDeciCelsius = 500;

    // INFO: cf 02aug26 at this level a menu in front of converging traffic is a bug, so it goes
    static constexpr traffic::Level kAlarmTakesGlass = traffic::Level::Advisory;

    ScreenService(runtime::Context& context, Settings& settings, comms::ConfigService& config,
                  AlarmService& alarm, const BootSnapshot& self_test)
        : runtime::Service(context),
          settings_(settings),
          config_(config),
          alarm_(alarm),
          self_test_(self_test) {}

    void tick(uint32_t now_ms) override;

    void next_page();
    void set_backlight(bool on);
    void set_power(bool on);
    void settle_park(uint32_t now_ms);
    void park_for_install();
    void park_for_stow();
    void set_range_nm(int32_t nm) {
        range_nm_ = nm;
        dirty_ = true;
    }

    enum class Thermal : uint8_t { Refresh, Hold };
    Thermal thermal() const;

    Page page() const { return page_; }
    Mode mode() const { return mode_; }
    comms::Pending prompt() const { return prompt_; }
    const MenuEditor& editor() const { return editor_; }
    int32_t range_nm() const { return range_nm_; }
    bool backlight() const { return backlight_; }
    bool powered() const { return powered_; }
    bool parking() const { return park_ != ParkStep::None; }
    const Glass& framebuffer() const { return fb_; }
    void mark_dirty() { dirty_ = true; }

   private:
    void render(uint32_t now_ms);
    void change_screen();
    void draw_prompt();
    void draw_menu_page();
    void enter_menu(uint32_t now_ms);
    void leave_menu();
    void page_forward(uint32_t now_ms);
    void go_home();
    void show_radar();
    void show_page(Page page);
    void handle_input(uint32_t now_ms);
    void obey(Command command, uint32_t now_ms);
    void sync_editor(uint32_t now_ms);
    void step_editor(uint32_t now_ms);
    MenuValues menu_values() const;
    void resolve(Gesture gesture);
    enum class Change : uint8_t { None, Asked, Wiped };
    bool refresh_allowed() const;
    void wipe_glass(uint32_t now_ms);
    bool may_present_park_frame() const;
    enum class ParkFrame : uint8_t { Wordmark, Installing, Blank };
    enum class ParkStep : uint8_t { None, Frame, Sleep };
    void park(ParkFrame frame);
    void draw_park_frame(ParkFrame frame);
    void note_presented(uint32_t now_ms);

    int32_t climb_fpm() const {
        return to_feet_per_minute(MillimetresPerSec(context_.state.own.climb_mm_s)).v;
    }

    bool climb_measured() const {
        return context_.state.own.climb_valid &&
               (context_.state.own.fix_valid || context_.state.baro.active);
    }

    bool alarm_takes_glass() const { return context_.state.alarm_live >= kAlarmTakesGlass; }

    bool alarm_stands() const { return context_.state.alarm_live != traffic::Level::None; }

    bool alarm_flashing() const { return alarm_stands(); }

    void flash_alarm();

    bool receiver_listening() const {
        return ports::has(context_.roles.capabilities, ports::Capability::Rf) &&
               context_.state.clock.pps_locked;
    }

    bool taxiing() const {
        return context_.state.own.fix_valid && !context_.state.flight.running &&
               context_.state.flight.rolling;
    }

    Settings& settings_;
    comms::ConfigService& config_;
    AlarmService& alarm_;
    const BootSnapshot& self_test_;
    comms::Pending prompt_{comms::Pending::None};
    Controls controls_{};
    ConfirmGesture gesture_{};
    MenuEditor editor_{};

    // INFO: cf 02aug26 a prompt is answered once read and once the thumb has stopped, never sooner
    uint32_t last_press_ms_{0};
    uint32_t prompt_since_ms_{0};
    bool pressed_once_{false};
    bool prompt_on_glass_{false};

    Glass fb_{};
    Glass presented_{};
    RadarTarget targets_[kMaxRadarTargets]{};
    traffic::RangeRow nearby_rows_[kNearbyRows]{};
    Page page_{Page::Radar};
    Mode mode_{Mode::Page};
    int32_t range_nm_{kDefaultRangeNm};
    uint32_t last_tick_ms_{0};
    uint32_t last_render_ms_{0};
    uint32_t last_present_ms_{0};
    traffic::Level last_live_{traffic::Level::None};
    bool alarm_flash_{false};
    bool dirty_{true};
    Change change_{Change::Asked};
    bool presented_once_{false};
    ParkStep park_{ParkStep::None};
    ParkFrame park_frame_{ParkFrame::Wordmark};
    bool backlight_{false};
    bool powered_{true};
};

}  // namespace skyblip::go

#endif
