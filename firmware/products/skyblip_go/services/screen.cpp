#include "products/skyblip_go/services/screen.h"

#include <cstring>

#include "core/events/input.h"
#include "core/flight/atmosphere.h"
#include "core/flight/extrapolate.h"
#include "core/flight/state.h"
#include "core/model/aircraft.h"
#include "core/model/ownship.h"
#include "core/power/cutoff.h"
#include "core/protocol/nmea_out.h"
#include "core/timing/transmit.h"
#include "core/units/units.h"
#include "core/util/intmath.h"
#include "products/skyblip_go/pages/installing.h"
#include "ui/widgets/wordmark.h"

namespace skyblip::go {

namespace {
bool settled_for_a_double_press(uint32_t now_ms, uint32_t since_ms) {
    return now_ms - since_ms >= ConfirmGesture::kDoublePressMs;
}

// INFO: fc 17sep26 one edge a second, so a phase older than this is an edge that never came
constexpr uint32_t kPpsEdgeMissedMs = 1500;

PpsState pps_state(const timing::ClockState& clock) {
    if (!clock.pps_locked) return PpsState::None;
    return clock.ms_since_pps >= kPpsEdgeMissedMs ? PpsState::Holdover : PpsState::Lock;
}
}  // namespace

// INFO: cf 02aug26 a standing prompt takes the pad and the button both, so nothing pages or opens
void ScreenService::handle_input(uint32_t now_ms) {
    const comms::Pending pending = config_.pending();
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
        const bool readable = prompt_on_glass_ ||
                              !ports::has(context_.roles.capabilities, ports::Capability::Display);
        const bool quiet = settled_for_a_double_press(now_ms, prompt_since_ms_) &&
                           (!pressed_once_ || settled_for_a_double_press(now_ms, last_press_ms_));
        if (readable && quiet) gesture_.arm(now_ms);
    }

    sync_editor(now_ms);

    events::ContactEvent event{};
    while (context_.bus.input.pop(event)) obey(controls_.read(event), now_ms);
    obey(controls_.tick(now_ms), now_ms);

    if (prompt_ != comms::Pending::None) {
        resolve(gesture_.tick(now_ms));
        return;
    }
    step_editor(now_ms);
}

void ScreenService::obey(Command command, uint32_t now_ms) {
    switch (command) {
        case Command::Next:
            if (prompt_ == comms::Pending::None) page_forward(now_ms);
            return;
        case Command::Home:
            if (prompt_ == comms::Pending::None) go_home();
            return;
        case Command::Act: break;
        case Command::None: return;
    }

    last_press_ms_ = now_ms;
    pressed_once_ = true;
    if (prompt_ != comms::Pending::None) {
        if (gesture_.armed()) resolve(gesture_.press(now_ms));
        return;
    }
    if (editor_.active()) {
        editor_.change(now_ms);
        return;
    }
    enter_menu(now_ms);
}

// INFO: cf 02aug26 the menu owns the button until a prompt takes it away unasked
void ScreenService::sync_editor(uint32_t now_ms) {
    const bool wanted = mode_ == Mode::Menu && prompt_ == comms::Pending::None;
    if (wanted == editor_.active()) return;
    if (wanted) {
        editor_.enter(page_, now_ms);
        return;
    }
    editor_.leave();
}

void ScreenService::change_screen() {
    dirty_ = true;
    if (change_ != Change::Wiped) change_ = Change::Asked;
}

void ScreenService::enter_menu(uint32_t now_ms) {
    if (menu_for(page_).n == 0) return;
    mode_ = Mode::Menu;
    sync_editor(now_ms);
    change_screen();
}

void ScreenService::page_forward(uint32_t now_ms) {
    if (mode_ != Mode::Menu) {
        next_page();
        return;
    }
    editor_.next_row(now_ms);
}

void ScreenService::next_page() { show_page(next_fitted_page(page_)); }

// A page whose sensor is not on the board is not a stop on the walk: a plain
// T-Echo has no inertial sensor, so the g-meter is not one of its pictures.
Page ScreenService::next_fitted_page(Page from) const {
    Page page = page_after(from);
    for (int i = 0; i < kWalkedPages && !sensor_fitted(page); i++) page = page_after(page);
    return page;
}

bool ScreenService::sensor_fitted(Page page) const {
    if (page != Page::GMeter) return true;
    return ports::has(context_.roles.capabilities, ports::Capability::Inclinometer);
}

void ScreenService::go_home() {
    if (alarm_stands()) {
        alarm_.dismiss();
        return;
    }
    show_radar();
}

void ScreenService::show_radar() { show_page(Page::Radar); }

void ScreenService::show_page(Page page) {
    if (mode_ == Mode::Menu) leave_menu();
    if (page_ == page) return;
    page_ = page;
    change_screen();
}

void ScreenService::leave_menu() {
    mode_ = Mode::Page;
    const Page owner = editor_.page();
    editor_.leave();
    page_ = walked(owner) ? owner : Page::Radar;
    change_screen();
}

MenuValues ScreenService::menu_values() const {
    MenuValues values;
    values.settings = settings_;
    values.range_step = range_step_;
    return values;
}

void ScreenService::step_editor(uint32_t now_ms) {
    if (!editor_.active()) return;

    const MenuValues current = menu_values();
    MenuValues next;

    switch (editor_.tick(now_ms, current, next)) {
        case MenuAction::Changed:
            settings_ = next.settings;
            range_step_ = next.range_step;
            // INFO: cf 02aug26 One owner of the flash blob. The page changes the
            // struct the config service was already given a reference to and
            // says so with the same flag the companion link raises; the write
            // itself stays in go::ConfigLinkService::persist, so there is never
            // a second writer and never two versions of the blob.
            config_.note_settings_changed();
            dirty_ = true;
            break;
        case MenuAction::Moved: dirty_ = true; break;
        case MenuAction::Open: show_page(editor_.opening()); break;
        case MenuAction::Leave: leave_menu(); break;
        case MenuAction::None:
        default: break;
    }
}

void ScreenService::resolve(Gesture gesture) {
    if (gesture == Gesture::None) return;
    if (gesture == Gesture::Confirm)
        config_.confirm();
    else
        config_.cancel();
    prompt_ = comms::Pending::None;
    gesture_.disarm();
    prompt_on_glass_ = false;
    change_screen();
}

void ScreenService::tick(uint32_t now_ms) {
    last_tick_ms_ = now_ms;
    handle_input(now_ms);
    context_.state.gnss.levels_wanted = showing_sky();

    if (context_.state.alarm_live != last_live_) {
        const bool escalated_into_glass =
            alarm_takes_glass() && context_.state.alarm_live > last_live_;
        last_live_ = context_.state.alarm_live;
        dirty_ = true;
        if (escalated_into_glass) show_radar();
    }

    if (mode_ == Mode::Menu && alarm_takes_glass()) leave_menu();

    if (!ports::has(context_.roles.capabilities, ports::Capability::Display)) return;
    settle_park(now_ms);
    if (!powered_) return;

    if (change_ != Change::Wiped && !refresh_allowed()) {
        context_.roles.display.ready(now_ms);
        return;
    }

    if (!dirty_ && now_ms - last_render_ms_ < kRenderPeriodMs) return;
    if (!context_.roles.display.ready(now_ms)) return;

    if (change_ == Change::Asked) {
        wipe_glass(now_ms);
        return;
    }
    if (presented_once_ && change_ != Change::Wiped && now_ms - last_present_ms_ < kPresentFloorMs)
        return;

    last_render_ms_ = now_ms;
    dirty_ = false;
    render(now_ms);

    const bool changed = !presented_once_ || change_ == Change::Wiped ||
                         std::memcmp(fb_.data(), presented_.data(), Glass::kBytes) != 0;
    if (!changed) return;

    context_.roles.display.present(fb_, ports::Refresh::Partial, now_ms);
    note_presented(now_ms);
    flash_alarm();
}

void ScreenService::flash_alarm() {
    if (!alarm_flashing()) return;
    alarm_flash_ = !alarm_flash_;
    dirty_ = true;
}

bool ScreenService::refresh_allowed() const {
    return thermal() == Thermal::Refresh &&
           power::may_refresh(context_.state.power.level, context_.state.power.supply_warned,
                              power::PanelRefresh::Routine);
}

// TODO: fc 12sep26 a cold glass is unmeasured, and no rule that returns is one full a frame (#62)
ScreenService::Thermal ScreenService::thermal() const {
    if (!context_.state.power.die_valid) return Thermal::Refresh;
    if (context_.state.power.die_dc > kHoldAboveDeciCelsius) return Thermal::Hold;
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
    std::memcpy(presented_.data(), fb_.data(), Glass::kBytes);
    presented_once_ = true;
    context_.state.panel_presented = true;
    prompt_on_glass_ = prompt_ != comms::Pending::None;
    last_present_ms_ = now_ms;
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
        context_.roles.display.present(fb_, ports::Refresh::Full, now_ms);
        return;
    }
    park_ = ParkStep::None;
    context_.roles.display.power_off();
    set_backlight(false);
}

void ScreenService::draw_park_frame(ParkFrame frame) {
    switch (frame) {
        case ParkFrame::Installing: draw_installing(fb_); return;
        // INFO: fc 12sep26 months of one image is the ghosting an e-paper never fully loses
        case ParkFrame::Blank: fb_.clear(/*white=*/true); return;
        case ParkFrame::Wordmark:
        default:
            fb_.clear(/*white=*/true);
            ui::draw_wordmark(fb_, kGlassW / 2, kGlassH / 2);
            return;
    }
}

bool ScreenService::may_present_park_frame() const {
    if (thermal() == Thermal::Hold) return false;
    return power::may_refresh(context_.state.power.level, context_.state.power.supply_warned,
                              power::PanelRefresh::Park);
}

// INFO: fc 07sep26 the glass wears this through the swap; a frozen prompt invites a reset
void ScreenService::park_for_install() { park(ParkFrame::Installing); }

void ScreenService::park_for_stow() { park(ParkFrame::Blank); }

void ScreenService::draw_prompt() {
    ConfirmSnapshot snapshot;
    snapshot.title = comms::pending_title(prompt_);
    snapshot.detail = comms::pending_detail(prompt_);
    snapshot.timeout_s = comms::kConfirmWindowMs / 1000;
    draw_confirm(fb_, snapshot);
}

void ScreenService::draw_menu_page() {
    MenuSnapshot snapshot;
    snapshot.page = editor_.page();
    snapshot.values = menu_values();
    snapshot.focus = editor_.focus();
    draw_menu(fb_, snapshot);
}

void ScreenService::render(uint32_t now_ms) {
    if (prompt_ != comms::Pending::None) {
        draw_prompt();
        return;
    }
    if (mode_ == Mode::Menu) {
        draw_menu_page();
        return;
    }

    fb_.clear(/*white=*/true);

    const model::OwnState& own = context_.state.own;
    const go::Settings& settings = settings_;

    switch (page_) {
        case Page::Radar: {
            RadarSnapshot snap;
            snap.fix_valid = own.fix_valid;
            snap.stage = context_.state.gnss.stage;
            snap.units = settings.units;
            snap.range_step = range_step_;
            snap.track_cdeg = own.track_cdeg;
            snap.speed_mm_s = own.speed_mm_s;
            snap.turn_cdps = own.turn_cdps;
            snap.flight_seconds = context_.state.flight.seconds;
            snap.flight_time_valid = context_.state.flight.time_valid;
            snap.airborne = context_.state.flight.running;
            snap.taxiing = taxiing();
            snap.receiver_listening = receiver_listening();
            snap.alarm_flash = alarm_flash_;
            snap.formation_members = context_.state.formation.members;
            int n = 0;
            if (own.fix_valid) {
                const model::OwnState own_now = flight::carried_to(own, now_ms);
                for (int i = 0; i < traffic::TrafficTable::kCapacity && n < kMaxRadarTargets; i++) {
                    const traffic::Target* t = context_.state.traffic.at(i);
                    if (!t || !t->used) continue;
                    const model::AircraftObs obs = flight::carried_to(t->obs, now_ms);
                    int32_t north = 0, east = 0, up = 0;
                    if (!protocol::relative_ned(own_now, obs, north, east, up)) continue;
                    targets_[n].north_m = north;
                    targets_[n].east_m = east;
                    targets_[n].up_m = up;
                    targets_[n].alarm_level = t->alarm_level;
                    targets_[n].alarm_dismissed = t->alarm_dismissed;
                    targets_[n].climb_e8 = obs.climb_e8;
                    targets_[n].climb_valid = obs.climb_valid;
                    targets_[n].speed_mm_s =
                        obs.speed_valid ? to_mm_s(QuarterMetresPerSec(obs.speed_q)).v : 0;
                    targets_[n].track_cdeg = to_centi_degrees(Cordic9(obs.track_c9)).v;
                    targets_[n].turn_cdps = static_cast<int16_t>(t->turn.dps * 100);
                    targets_[n].turn_valid = t->turn.valid;
                    targets_[n].in_formation = t->in_formation;
                    n++;
                }
            }
            snap.n_targets = n;
            snap.targets = targets_;
            draw_radar(fb_, snap);
            break;
        }
        case Page::SixPack: {
            SixPackSnapshot snap;
            snap.data_valid = own.fix_valid;
            snap.units = settings.units;
            snap.speed_kt = to_knots(MillimetresPerSec(own.speed_mm_s)).v;
            snap.alt_ft = to_feet(Millimetres(own.alt_mm)).v;
            snap.vs_fpm = climb_fpm();
            snap.vs_valid = climb_measured();
            snap.track_deg = to_degrees(CentiDegrees(own.track_cdeg)).v;
            snap.turn_cdps = own.turn_cdps;
            snap.flight_seconds = context_.state.flight.seconds;
            snap.flight_time_valid = context_.state.flight.time_valid;
            snap.airborne = context_.state.flight.running;
            snap.taxiing = taxiing();
            snap.inclinometer_fitted =
                ports::has(context_.roles.capabilities, ports::Capability::Inclinometer);
            snap.lateral_valid = context_.state.slip.valid;
            snap.lateral_mg = context_.state.slip.lateral_mg;
            draw_sixpack(fb_, snap);
            break;
        }
        case Page::Sats: {
            SatsSnapshot snap;
            snap.fix_valid = own.fix_valid;
            snap.levels_live = context_.state.gnss.levels_live;
            snap.stage = context_.state.gnss.stage;
            snap.stage_s = context_.state.gnss.stage_s;
            snap.sats = own.sats;
            snap.hdop_e2 = own.hdop_e2;
            snap.vdop_e2 = own.vdop_e2;
            snap.sky = &context_.state.gnss.sky;
            draw_sats(fb_, snap);
            break;
        }
        case Page::Nearby: {
            NearbySnapshot snap;
            snap.own_addr = settings.device_addr;
            snap.fix_valid = own.fix_valid;
            snap.units = settings.units;
            snap.n_heard = context_.state.traffic.count();
            snap.n_rows =
                traffic::rank_by_range(context_.state.traffic, own, nearby_rows_, kNearbyRows);
            snap.rows = nearby_rows_;
            draw_nearby(fb_, snap);
            break;
        }
        case Page::SelfTest: draw_boot(fb_, self_test_); break;
        case Page::GMeter: {
            GMeterSnapshot snap;
            snap.fitted = ports::has(context_.roles.capabilities, ports::Capability::Inclinometer);
            snap.valid = context_.state.gload.valid;
            snap.now = context_.state.gload.now;
            snap.most = context_.state.gload.most;
            snap.least = context_.state.gload.least;
            draw_gmeter(fb_, snap);
            break;
        }
        case Page::RadioLog: {
            RadioLogSnapshot snap;
            snap.gnss.fix_valid = own.fix_valid;
            snap.gnss.sats = own.sats;
            snap.gnss.pps = pps_state(context_.state.clock);
            snap.gnss.pps_age_s = static_cast<uint16_t>(context_.state.clock.ms_since_pps / 1000);
            snap.rx_ok = context_.state.air.rx_ok;
            snap.tx_ok = context_.state.air.tx_ok;
            snap.noise = context_.state.air.rx_noise;
            snap.band_dbm = context_.state.rf.noise_dbm;
            snap.n_rows = context_.state.radio_log.count();
            snap.log = &context_.state.radio_log;
            draw_radio_log(fb_, snap);
            break;
        }
        case Page::Status:
        default: {
            StatusSnapshot snap;
            snap.device_addr = settings.device_addr;
            snap.callsign = settings.callsign;
            snap.fix_valid = own.fix_valid;
            snap.utc_valid = own.utc_valid;
            snap.transmitting = timing::own_ship_transmits(own, context_.state.clock);
            snap.sats = own.sats;
            snap.stage = context_.state.gnss.stage;
            snap.stage_s = context_.state.gnss.stage_s;
            snap.fix_mode = context_.state.gnss.fix_mode;
            snap.lat_1e7 = own.lat_1e7;
            snap.lon_1e7 = own.lon_1e7;
            snap.alt_mm = own.alt_mm;
            snap.speed_mm_s = own.speed_mm_s;
            snap.track_cdeg = own.track_cdeg;
            snap.climb_mm_s = own.climb_mm_s;
            snap.utc = own.utc;
            snap.n_targets = context_.state.traffic.count();
            snap.imu_stage = context_.state.imu.stage;
            snap.imu_fault = context_.state.imu.fault;
            snap.imu_fifo_bytes = context_.state.imu.fifo_bytes;
            snap.imu_unparsed = context_.state.imu.unparsed;
            snap.imu_error = context_.state.imu.error;
            snap.imu_interrupt = context_.state.imu.interrupt;
            snap.imu_meta = context_.state.imu.meta;
            snap.imu_sensor_error = context_.state.imu.sensor_error;
            snap.imu_errored_sensor = context_.state.imu.errored_sensor;
            snap.slip_valid = context_.state.slip.valid;
            snap.slip_mg = context_.state.slip.lateral_mg;
            snap.baro_valid = context_.state.baro.active;
            snap.battery_valid = context_.state.power.battery.valid;
            snap.battery_mv = context_.state.power.battery.millivolts;
            snap.battery_percent = context_.state.power.battery.percent;
            snap.charging = context_.state.power.battery.charging;
            snap.charge = context_.state.power.charge;
            // The decision belongs to core/power's CutoffMonitor, which has
            // already debounced it, ignored a cell on the cable and thrown out a
            // floating sense. The page reports what it decided.
            const power::PowerLevel level = context_.state.power.level;
            snap.battery_low =
                level == power::PowerLevel::Low || level == power::PowerLevel::Cutoff;
            snap.pressure_mpa = context_.state.baro.pressure_mpa;
            if (context_.state.baro.active) {
                const uint32_t pa = div_round<uint32_t>(context_.state.baro.pressure_mpa, 1000);
                snap.alt_std_m = div_round(flight::pressure_to_alt_cm(pa), 100);
            }
            draw_status(fb_, snap);
            break;
        }
    }
}

}  // namespace skyblip::go
