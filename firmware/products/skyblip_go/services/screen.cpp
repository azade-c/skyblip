#include "products/skyblip_go/services/screen.h"

#include <cstring>

#include "core/flight/atmosphere.h"
#include "core/flight/extrapolate.h"
#include "core/power/cutoff.h"
#include "core/protocol/nmea_out.h"
#include "core/timing/transmit.h"
#include "core/util/units.h"
#include "ui/screens/installing.h"
#include "ui/widgets/wordmark.h"

namespace skyblip::go {

namespace {
bool settled_for_a_double_press(uint32_t now_ms, uint32_t since_ms) {
    return now_ms - since_ms >= ui::ConfirmGesture::kDoublePressMs;
}
}  // namespace

// INFO: cf 02aug26 a standing prompt takes the pad and the button both, so nothing pages or opens
void ScreenService::handle_input(uint32_t now_ms) {
    const comms::Pending pending = config_ ? config_->pending() : comms::Pending::None;
    if (pending != prompt_) {
        prompt_ = pending;
        prompt_since_ms_ = now_ms;
        change_screen();
        gesture_.disarm();
        prompt_on_glass_ = false;
    }
    if (prompt_ != comms::Pending::None && !gesture_.armed()) {
        // INFO: cf 02aug26 The two conditions that make a press an answer
        // rather than an accident: the question has reached the glass where it
        // can be read, and the thumb has stopped. A run of presses that began
        // before the prompt - pages being cycled, or a value being stepped on
        // the settings page - keeps the gesture disarmed until it ends, so
        // nothing already in flight can be spent on an authorisation. With no
        // panel fitted there is nothing to read and presence is all there is.
        const bool readable =
            prompt_on_glass_ || !hal::has(context_.roles.capabilities, hal::Capability::Display);
        const bool quiet = settled_for_a_double_press(now_ms, prompt_since_ms_) &&
                           (!pressed_once_ || settled_for_a_double_press(now_ms, last_press_ms_));
        if (readable && quiet) gesture_.arm(now_ms);
    }

    sync_editor(now_ms);

    messages::ButtonEvent event{};
    while (context_.bus.input.pop(event)) {
        if (event.id == messages::kPadTapped) {
            if (prompt_ == comms::Pending::None) page_forward(now_ms);
            continue;
        }
        if (event.id == messages::kPadHeld) {
            if (prompt_ == comms::Pending::None) show_radar();
            continue;
        }
        last_press_ms_ = now_ms;
        pressed_once_ = true;
        if (prompt_ != comms::Pending::None) {
            if (gesture_.armed()) resolve(gesture_.press(now_ms));
            continue;
        }
        if (editor_.active()) {
            if (showing_self_test_)
                dismiss_self_test(now_ms);
            else
                editor_.change(now_ms);
            continue;
        }
        enter_settings(now_ms);
    }

    if (prompt_ != comms::Pending::None) {
        resolve(gesture_.tick(now_ms));
        return;
    }
    step_editor(now_ms);
}

// INFO: cf 02aug26 the settings mode owns the button until a prompt takes it away unasked
void ScreenService::sync_editor(uint32_t now_ms) {
    const bool wanted = mode_ == Mode::Settings && prompt_ == comms::Pending::None;
    if (!wanted) showing_self_test_ = false;
    if (wanted == editor_.active()) return;
    if (wanted) {
        showing_self_test_ = self_test_ != nullptr;
        editor_.enter(now_ms);
        return;
    }
    editor_.leave();
}

void ScreenService::change_screen() {
    dirty_ = true;
    if (change_ != Change::Wiped) change_ = Change::Asked;
}

void ScreenService::dismiss_self_test(uint32_t now_ms) {
    showing_self_test_ = false;
    editor_.enter(now_ms);
    change_screen();
}

void ScreenService::enter_settings(uint32_t now_ms) {
    mode_ = Mode::Settings;
    sync_editor(now_ms);
    change_screen();
}

void ScreenService::page_forward(uint32_t now_ms) {
    if (mode_ != Mode::Settings) {
        next_page();
        return;
    }
    if (showing_self_test_)
        dismiss_self_test(now_ms);
    else
        editor_.next_row(now_ms);
}

void ScreenService::show_radar() {
    if (mode_ == Mode::Settings) leave_settings();
    page_ = Page::Radar;
    change_screen();
}

void ScreenService::leave_settings() {
    mode_ = Mode::Traffic;
    showing_self_test_ = false;
    editor_.leave();
    page_ = traffic_page();
    change_screen();
}

void ScreenService::step_editor(uint32_t now_ms) {
    if (!editor_.active()) return;

    ui::SettingsValues current;
    current.settings = context_.state.settings;
    current.qnh_pa = context_.state.qnh_pa;
    ui::SettingsValues next;

    switch (editor_.tick(now_ms, current, next)) {
        case ui::SettingsAction::Changed:
            context_.state.settings = next.settings;
            context_.state.qnh_pa = next.qnh_pa;
            // INFO: cf 02aug26 One owner of the flash blob. The page changes the
            // struct the config service was already given a reference to and
            // says so with the same flag the companion link raises; the write
            // itself stays in go::ConfigLinkService::persist, so there is never
            // a second writer and never two versions of the blob.
            if (config_ != nullptr) config_->note_settings_changed();
            dirty_ = true;
            break;
        case ui::SettingsAction::Moved: dirty_ = true; break;
        case ui::SettingsAction::Leave: leave_settings(); break;
        case ui::SettingsAction::None:
        default: break;
    }
}

// INFO: cf 02aug26 where the settings mode hands the glass back: the first page the mask leaves
Page ScreenService::traffic_page() const {
    const uint8_t mask = context_.state.settings.page_mask;
    for (int i = 0; i < static_cast<int>(Page::kCount); i++)
        if (mask & (1u << i)) return static_cast<Page>(i);
    return Page::Radar;
}

void ScreenService::resolve(ui::Gesture gesture) {
    if (gesture == ui::Gesture::None) return;
    if (gesture == ui::Gesture::Confirm)
        config_->confirm();
    else
        config_->cancel();
    prompt_ = comms::Pending::None;
    gesture_.disarm();
    prompt_on_glass_ = false;
    change_screen();
}

void ScreenService::tick(uint32_t now_ms) {
    last_tick_ms_ = now_ms;
    handle_input(now_ms);

    if (context_.state.alarm_level != last_alarm_) {
        last_alarm_ = context_.state.alarm_level;
        dirty_ = true;
    }

    if (mode_ == Mode::Settings && context_.state.alarm_level >= kAlarmTakesGlass) leave_settings();

    if (!hal::has(context_.roles.capabilities, hal::Capability::Display)) return;
    settle_park(now_ms);
    if (!powered_) return;

    if (change_ != Change::Wiped && !refresh_allowed()) {
        context_.roles.display.ready(now_ms);
        return;
    }

    if (!dirty_ && now_ms - last_render_ms_ < kRenderPeriodMs) return;
    if (!context_.roles.display.ready(now_ms)) return;

    if (change_ == Change::Asked) {
        change_ = Change::None;
        if (!alarm_standing()) {
            wipe_glass(now_ms);
            return;
        }
    }
    if (presented_once_ && change_ != Change::Wiped && now_ms - last_present_ms_ < kPresentFloorMs)
        return;

    last_render_ms_ = now_ms;
    dirty_ = false;
    render(now_ms);

    const bool changed = !presented_once_ || change_ == Change::Wiped ||
                         std::memcmp(fb_.data(), presented_.data(), ui::Framebuffer::kBytes) != 0;
    if (!changed) return;

    context_.roles.display.present(fb_, hal::Refresh::Partial, now_ms);
    note_presented(now_ms);
}

bool ScreenService::refresh_allowed() const {
    return thermal() == Thermal::Refresh &&
           power::may_refresh(context_.state.power_level, context_.state.supply_warned,
                              power::PanelRefresh::Routine);
}

// TODO: fc 12sep26 a cold glass is unmeasured, and no rule that returns is one full a frame (#62)
ScreenService::Thermal ScreenService::thermal() const {
    if (!context_.state.die_temperature_valid) return Thermal::Refresh;
    if (context_.state.die_decicelsius > kHoldAboveDeciCelsius) return Thermal::Hold;
    return Thermal::Refresh;
}

// INFO: fc 09mar26 SoftRF changes page on partials alone: all black through the waveform, then it
void ScreenService::wipe_glass(uint32_t now_ms) {
    fb_.clear(/*white=*/false);
    context_.roles.display.paint_black(now_ms);
    note_presented(now_ms);
    prompt_on_glass_ = false;
    last_render_ms_ = now_ms;
    dirty_ = true;
    change_ = Change::Wiped;
}

void ScreenService::note_presented(uint32_t now_ms) {
    change_ = Change::None;
    std::memcpy(presented_.data(), fb_.data(), ui::Framebuffer::kBytes);
    presented_once_ = true;
    context_.state.panel_presented = true;
    prompt_on_glass_ = prompt_ != comms::Pending::None;
    last_present_ms_ = now_ms;
}

void ScreenService::next_page() {
    const int n = static_cast<int>(Page::kCount);
    for (int i = 1; i <= n; i++) {
        const int cand = (static_cast<int>(page_) + i) % n;
        if (context_.state.settings.page_mask & (1u << cand)) {
            page_ = static_cast<Page>(cand);
            break;
        }
    }
    change_screen();
}

void ScreenService::set_backlight(bool on) {
    backlight_ = on;
    context_.roles.display.set_backlight(on);
}

void ScreenService::set_power(bool on) {
    powered_ = on;
    if (on) {
        park_ = ParkStep::None;
        context_.roles.display.power_on();
        change_screen();
        return;
    }
    dirty_ = true;
    park(ParkFrame::Wordmark);
}

// INFO: fc 01aug25 pushed before power-off: the glass wears it while off
void ScreenService::park(ParkFrame frame) {
    powered_ = false;
    park_frame_ = frame;
    park_ = may_present_park_frame() ? ParkStep::Frame : ParkStep::Sleep;
}

// INFO: fc 12sep26 both steps are commands, and a command sent over a live BUSY is lost
void ScreenService::settle_park(uint32_t now_ms) {
    if (park_ == ParkStep::None) return;
    if (!context_.roles.display.ready(now_ms)) return;
    if (park_ == ParkStep::Frame) {
        park_ = ParkStep::Sleep;
        draw_park_frame(park_frame_);
        context_.roles.display.present(fb_, hal::Refresh::Full, now_ms);
        return;
    }
    park_ = ParkStep::None;
    context_.roles.display.power_off();
    set_backlight(false);
}

void ScreenService::draw_park_frame(ParkFrame frame) {
    switch (frame) {
        case ParkFrame::Installing: ui::draw_installing(fb_); return;
        // INFO: fc 12sep26 months of one image is the ghosting an e-paper never fully loses
        case ParkFrame::Blank: fb_.clear(/*white=*/true); return;
        case ParkFrame::Wordmark:
        default:
            fb_.clear(/*white=*/true);
            ui::draw_wordmark(fb_, ui::Framebuffer::kW / 2, ui::Framebuffer::kH / 2);
            return;
    }
}

bool ScreenService::may_present_park_frame() const {
    if (thermal() == Thermal::Hold) return false;
    return power::may_refresh(context_.state.power_level, context_.state.supply_warned,
                              power::PanelRefresh::Park);
}

// INFO: fc 07sep26 the glass wears this through the swap; a frozen prompt invites a reset
void ScreenService::park_for_install() { park(ParkFrame::Installing); }

void ScreenService::park_for_stow() { park(ParkFrame::Blank); }

void ScreenService::draw_prompt() {
    ui::ConfirmSnapshot snapshot;
    snapshot.title = comms::pending_title(prompt_);
    snapshot.detail = comms::pending_detail(prompt_);
    snapshot.timeout_s = comms::kConfirmWindowMs / 1000;
    ui::draw_confirm(fb_, snapshot);
}

void ScreenService::draw_settings_page() {
    ui::SettingsSnapshot snapshot;
    snapshot.values.settings = context_.state.settings;
    snapshot.values.qnh_pa = context_.state.qnh_pa;
    snapshot.focus = editor_.focus();
    ui::draw_settings(fb_, snapshot);
}

void ScreenService::render(uint32_t now_ms) {
    if (prompt_ != comms::Pending::None) {
        draw_prompt();
        return;
    }
    if (mode_ == Mode::Settings) {
        if (showing_self_test_)
            ui::draw_boot(fb_, *self_test_);
        else
            draw_settings_page();
        return;
    }

    fb_.clear(/*white=*/true);

    const messages::OwnState& own = context_.state.own;
    const settings::Settings& settings = context_.state.settings;

    switch (page_) {
        case Page::Radar: {
            ui::RadarSnapshot snap;
            snap.have_fix = own.fix_valid;
            snap.range_nm = range_nm_;
            snap.track_deg = to_degrees(Cordic9(own.track_c9)).v;
            snap.speed_mps = to_mps(QuarterMetresPerSec(own.speed_q)).v;
            snap.flight_seconds = context_.state.flight_seconds;
            snap.have_flight_time = context_.state.flight_time_valid;
            snap.airborne = context_.state.flight_running;
            snap.receiver_listening = receiver_listening();
            snap.max_alarm = context_.state.alarm_level;
            int n = 0;
            if (own.fix_valid) {
                const messages::OwnState own_now = flight::carried_to(own, now_ms);
                for (int i = 0; i < traffic::TrafficTable::kCapacity && n < kMaxRadarTargets; i++) {
                    const traffic::Target* t = context_.state.traffic.at(i);
                    if (!t || !t->used) continue;
                    const messages::AircraftObs obs = flight::carried_to(t->obs, now_ms);
                    int32_t north = 0, east = 0, up = 0;
                    if (!protocol::relative_ned(own_now, obs, north, east, up)) continue;
                    targets_[n].north_m = north;
                    targets_[n].east_m = east;
                    targets_[n].up_m = up;
                    targets_[n].alarm_level = t->alarm_level;
                    targets_[n].climb_e8 = obs.climb_e8;
                    targets_[n].has_climb = obs.has_climb;
                    targets_[n].speed_mps =
                        obs.has_speed ? to_mps(QuarterMetresPerSec(obs.speed_q)).v : 0;
                    targets_[n].track_deg = to_degrees(Cordic9(obs.track_c9)).v;
                    n++;
                }
            }
            snap.n_targets = n;
            snap.targets = targets_;
            ui::draw_radar(fb_, snap);
            break;
        }
        case Page::SixPack: {
            ui::SixPackSnapshot snap;
            snap.have_data = own.fix_valid;
            snap.units = settings.units;
            // 1 m/s = 1.94384 kt, from quarter-m/s.
            snap.speed_kt = (static_cast<int32_t>(own.speed_q) * 194384) / (4 * 100000);
            snap.alt_ft = to_feet(Metres(own.alt_m)).v;
            snap.vs_fpm = climb_fpm();
            snap.track_deg = to_degrees(Cordic9(own.track_c9)).v;
            snap.turn_dps = own.turn_dps;
            snap.flight_seconds = context_.state.flight_seconds;
            snap.have_flight_time = context_.state.flight_time_valid;
            snap.airborne = context_.state.flight_running;
            ui::draw_sixpack(fb_, snap);
            break;
        }
        case Page::Signal: {
            ui::SignalSnapshot snap;
            snap.have_fix = own.fix_valid;
            snap.n_heard = context_.state.traffic.count();
            snap.n_rows =
                traffic::rank_by_range(context_.state.traffic, own, signal_rows_, ui::kSignalRows);
            snap.rows = signal_rows_;
            ui::draw_signal(fb_, snap);
            break;
        }
        case Page::RadioLog: {
            ui::RadioLogSnapshot snap;
            snap.gnss.fix_valid = own.fix_valid;
            snap.gnss.utc_valid = own.utc_valid;
            snap.gnss.sats = own.sats;
            snap.gnss.hdop_e2 = own.hdop_e2;
            snap.gnss.vdop_e2 = own.vdop_e2;
            snap.gnss.solutions = context_.state.gnss_solutions;
            snap.rx_ok = context_.state.rx_ok;
            snap.tx_ok = context_.state.tx_ok;
            snap.n_rows = context_.state.radio_log.count();
            snap.log = &context_.state.radio_log;
            ui::draw_radio_log(fb_, snap);
            break;
        }
        case Page::Status:
        default: {
            ui::StatusSnapshot snap;
            snap.device_addr = settings.device_addr;
            snap.callsign = settings.callsign;
            snap.fix_valid = own.fix_valid;
            snap.utc_valid = own.utc_valid;
            snap.transmitting = timing::own_ship_transmits(own, context_.state.clock);
            snap.sats = own.sats;
            snap.lat_1e7 = own.lat_1e7;
            snap.lon_1e7 = own.lon_1e7;
            snap.alt_m = own.alt_m;
            snap.speed_q = own.speed_q;
            snap.track_c9 = own.track_c9;
            snap.climb_mm_s = own.climb_mm_s;
            snap.utc = own.utc;
            snap.n_targets = context_.state.traffic.count();
            snap.baro_valid = context_.state.baro_active;
            snap.battery_valid = context_.state.battery.valid;
            snap.battery_mv = context_.state.battery.millivolts;
            snap.battery_percent = context_.state.battery.percent;
            snap.charging = context_.state.battery.charging;
            snap.charge = context_.state.charge;
            // The decision belongs to core/power's CutoffMonitor, which has
            // already debounced it, ignored a cell on the cable and thrown out a
            // floating sense. The page reports what it decided.
            const power::PowerLevel level = context_.state.power_level;
            snap.battery_low =
                level == power::PowerLevel::Low || level == power::PowerLevel::Cutoff;
            snap.pressure_mpa = context_.state.pressure_mpa;
            snap.qnh_pa = context_.state.qnh_pa;
            if (context_.state.baro_active) {
                const uint32_t pa = context_.state.pressure_mpa / 1000;
                snap.alt_qnh_m = flight::alt_cm_on_setting(pa, context_.state.qnh_pa) / 100;
                snap.alt_std_m = flight::pressure_to_alt_cm(pa) / 100;
            }
            ui::draw_status(fb_, snap);
            break;
        }
    }
}

}  // namespace skyblip::go
