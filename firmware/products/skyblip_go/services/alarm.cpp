#include "products/skyblip_go/services/alarm.h"

#include <algorithm>

namespace skyblip::go {

bool AlarmService::silenced(traffic::Target& target, formation::State state,
                           const traffic::AlarmAssessment& a, uint32_t now_ms) {
    if (state == formation::State::None) return false;
    if (a.closing_mps >= formation::kClosingMps) {
        formation_.release(target.obs.addr_table, target.obs.addr, now_ms);
        target.in_formation = false;
        return false;
    }
    tracker_.withdraw(target.obs.addr_table, target.obs.addr);
    target.alarm_level = traffic::Level::None;
    return true;
}

formation::State AlarmService::watch_formation(traffic::Target& target, uint32_t now_ms) {
    const formation::Report r = formation_.observe(context_.state.own, target.obs, now_ms);
    const bool was = target.in_formation;
    target.in_formation = r.state == formation::State::Together;
    if (target.in_formation != was) dirty_ = true;
    return r.state;
}

void AlarmService::tick(uint32_t now_ms) {
    traffic::Level worst = traffic::Level::None;
    traffic::Level speak = traffic::Level::None;
    bool escalated = false;
    if (context_.state.own.fix_valid) {
        for (int i = 0; i < traffic::TrafficTable::kCapacity; i++) {
            traffic::Target* t = context_.state.traffic.at(i);
            if (!t || !t->used) continue;
            const formation::State f = watch_formation(*t, now_ms);
            const traffic::AlarmTracker::Decision d =
                tracker_.update(context_.state.own, t->obs, t->turn.dps, t->turn.valid, now_ms);
            t->alarm_level = d.assessment.level;
            if (silenced(*t, f, d.assessment, now_ms)) continue;
            worst = std::max(d.assessment.level, worst);
            if (!d.notify) continue;
            speak = std::max(d.assessment.level, speak);
            escalated = escalated || d.escalated;
        }
    }
    tracker_.forget_stale(now_ms);
    formation_.forget_stale(now_ms);
    context_.state.formation.members = formation_.members();

    if (worst != context_.state.alarm_level) {
        context_.state.alarm_level = worst;
        dirty_ = true;
    }

    annunciation::Situation situation{};
    // Not the raw worst: what is being announced, which the tracker already
    // holds through a contact bouncing across a ring boundary, and which falls
    // to nothing when the target that caused it stops being heard.
    situation.level = tracker_.announced_level(now_ms);
    situation.escalated = escalated;
    situation.first_fix = context_.state.own.fix_acquired;
    situation.enabled = settings_.alarm_enabled;
    situation.running = running_;
    drive(situation, now_ms);
    drive_lamp(now_ms, running_);

    // Haptics only on the way UP, and only from "important": this device rides in
    // a pocket or a harness where the buzzer is muffled, which is exactly when a
    // pilot needs to feel it. A standing urgent re-announces its tone every
    // couple of seconds and must not pulse the motor with it - escalated is
    // false on a re-notification, which is what keeps the two apart.
    if (!situation.enabled || !running_) return;
    if (escalated && speak >= kHapticFromLevel)
        context_.roles.annunciator.vibrate(speak >= traffic::Level::Urgent ? kHapticUrgentMs
                                                                           : kHapticImportantMs);
}

void AlarmService::park(uint32_t now_ms) {
    running_ = false;
    annunciation::Situation situation{};
    situation.running = false;
    drive(situation, now_ms);
    // Dark first, through the same table that lit it, and only then let go of the
    // pins. The order matters on silicon: these LEDs are active-low, so dark is a
    // pin driven high, and releasing before darkening would leave the last colour
    // lit on a floating line for as long as the rail lasts.
    drive_lamp(now_ms, /*running=*/false);
    context_.roles.indicator.park();
}

// Everything the table reads is already published on bus::State by the service
// that owns it - the cell and its level by PowerService, the fix by
// OwnshipService, the worst standing level by this one, a few lines above. Nothing
// is derived a second time here, which is what keeps the lamp saying LOW at
// exactly the voltage the panel and the tablet do.
void AlarmService::drive_lamp(uint32_t now_ms, bool running) {
    const power::BatteryState& battery = context_.state.power.battery;
    indication::Situation situation{};
    situation.running = running;
    situation.alarm_level = context_.state.alarm_level;
    situation.external_power = battery.external_power;
    // core/power/battery.h: charging is external power AND a cell still below the
    // float voltage, so the cable in with charging false is a charge that finished.
    situation.charge_complete = battery.external_power && !battery.charging;
    situation.power_level = context_.state.power.level;
    situation.fix_valid = context_.state.own.fix_valid;

    const indication::Command command = lamp_.update(situation, now_ms);
    if (command.changed) context_.roles.indicator.show(command.lamp);
}

void AlarmService::drive(const annunciation::Situation& situation, uint32_t now_ms) {
    const annunciation::Command command = policy_.update(situation, now_ms);
    if (!command.changed) return;
    if (!command.tone_on) {
        context_.roles.annunciator.silence();
        return;
    }
    const uint8_t volume = settings_.alarm_volume;
    if (command.tone_hz != 0)
        context_.roles.annunciator.tone(command.tone_hz, volume);
    else
        context_.roles.annunciator.alarm(command.tone_level, volume);
}

}  // namespace skyblip::go
