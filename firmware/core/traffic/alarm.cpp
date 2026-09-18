#include "core/traffic/alarm.h"

#include <algorithm>

#include "core/flight/arc.h"
#include "core/flight/extrapolate.h"
#include "core/model/aircraft.h"
#include "core/model/ownship.h"
#include "core/protocol/nmea_out.h"
#include "core/traffic/conflict.h"
#include "core/util/intmath.h"

namespace skyblip::traffic {

namespace {

constexpr int32_t kTrigOne = 16384;
constexpr int32_t kSpeedQPerMps = 4;
constexpr int kTrackC9ToAngle = 7;
constexpr int kTrackC9Mask = 0x1FF;
constexpr int kTrackC9Turn = 512;
constexpr uint16_t kUnknownTargetSpeedQ = kUnknownTargetSpeedMps * kSpeedQPerMps;

void velocity_ned(uint16_t speed_q, uint16_t track_c9, int32_t& north, int32_t& east) {
    const int16_t angle =
        static_cast<int16_t>(static_cast<uint16_t>((track_c9 & kTrackC9Mask) << kTrackC9ToAngle));
    north = static_cast<int32_t>(speed_q) * icos(angle);
    east = static_cast<int32_t>(speed_q) * isin(angle);
}

int32_t closing_from_vectors(const model::OwnState& own, const model::AircraftObs& target,
                             int32_t n_m, int32_t e_m, int32_t dist_m) {
    if (dist_m <= 0) return kUnknownTargetSpeedMps;

    int32_t own_n = 0, own_e = 0;
    velocity_ned(own.speed_q, own.track_c9, own_n, own_e);
    int32_t target_n = 0, target_e = 0;
    if (target.speed_valid) velocity_ned(target.speed_q, target.track_c9, target_n, target_e);

    const int64_t along =
        static_cast<int64_t>(target_n - own_n) * n_m + static_cast<int64_t>(target_e - own_e) * e_m;
    const int64_t scale = static_cast<int64_t>(dist_m) * kSpeedQPerMps * kTrigOne;
    int32_t closing = static_cast<int32_t>(-along / scale);
    if (!target.speed_valid) closing += kUnknownTargetSpeedMps;
    return closing;
}

int32_t iabs32(int32_t v) { return v < 0 ? -v : v; }

uint16_t track_c9_towards(int32_t north_m, int32_t east_m) {
    const int16_t bearing = iatan2(-east_m, -north_m);
    const int32_t c9 =
        ((static_cast<int32_t>(static_cast<uint16_t>(bearing)) >> kTrackC9ToAngle) + kTrackC9Turn) %
        kTrackC9Turn;
    return static_cast<uint16_t>(c9);
}

flight::Motion target_motion(const model::AircraftObs& target, int32_t n_m, int32_t e_m,
                             int32_t u_m, int16_t turn_dps, bool turn_valid) {
    flight::Motion m = flight::motion_of(target, turn_dps, turn_valid);
    m.north_m = n_m;
    m.east_m = e_m;
    m.up_m = u_m;
    if (!target.speed_valid) {
        m.speed_q = kUnknownTargetSpeedQ;
        m.track_c9 = track_c9_towards(n_m, e_m);
        m.turn_dps = 0;
        m.turning = false;
    }
    return m;
}

Level level_for(const AlarmAssessment& a) {
    Level level = Level::None;
    const bool in_window = iabs32(a.rel_vert_m) <= kVertWindowM;
    if (in_window && a.rel_dist_m <= kInfoDistM) level = Level::Info;
    if (!a.breaches) return level;
    if (level == Level::None) level = Level::Info;
    if (a.at_s <= kImportantTtiS) level = Level::Important;
    if (a.at_s <= kUrgentTtiS) level = Level::Urgent;
    return level;
}

}  // namespace

AlarmAssessment assess(const model::OwnState& own_fix, const model::AircraftObs& reported,
                       uint32_t now_ms) {
    return assess(own_fix, reported, 0, false, now_ms);
}

// INFO: fc 13sep26 two positions from different instants are not a separation, so both are carried
// to now
AlarmAssessment assess(const model::OwnState& own_fix, const model::AircraftObs& reported,
                       int16_t target_turn_dps, bool target_turn_valid, uint32_t now_ms) {
    const model::OwnState own = flight::carried_to(own_fix, now_ms);
    const model::AircraftObs target = flight::carried_to(reported, now_ms);
    AlarmAssessment a{};
    int32_t n_m, e_m, u_m;
    if (!protocol::relative_ned(own, target, n_m, e_m, u_m)) return a;
    a.valid = true;
    a.rel_vert_m = u_m;
    a.rel_dist_m = static_cast<int32_t>(idistance(n_m, e_m));
    a.closing_mps = closing_from_vectors(own, target, n_m, e_m, a.rel_dist_m);

    const int16_t brg = iatan2(e_m, n_m);
    const int own_deg = (static_cast<int>(own.track_c9) * 45) >> 6;
    const int brg_deg = (static_cast<int>(static_cast<uint16_t>(brg)) * 360) / 65536;
    a.rel_bearing_deg = static_cast<uint16_t>(((brg_deg - own_deg) % 360 + 360) % 360);

    const Conflict conflict =
        first_breach(flight::motion_of(own),
                     target_motion(target, n_m, e_m, u_m, target_turn_dps, target_turn_valid));
    a.breaches = conflict.breaches;
    a.at_s = conflict.breaches ? conflict.at_s : kNoImpactS;
    a.miss_m = conflict.breaches ? conflict.miss_m : conflict.closest_m;
    a.level = level_for(a);
    return a;
}

AlarmTracker::Decision AlarmTracker::update(const model::OwnState& own,
                                            const model::AircraftObs& target, uint32_t now_ms) {
    return update(own, target, 0, false, now_ms);
}

AlarmTracker::Decision AlarmTracker::update(const model::OwnState& own,
                                            const model::AircraftObs& target,
                                            int16_t target_turn_dps, bool target_turn_valid,
                                            uint32_t now_ms) {
    Decision d{};
    d.assessment = assess(own, target, target_turn_dps, target_turn_valid, now_ms);
    if (!d.assessment.valid) return d;

    Slot* slot = slot_for(target, now_ms);
    if (slot == nullptr) return d;

    const uint32_t key = target.received.at_s * 1000u + target.received.into_ms;
    if (key != slot->obs_key) {
        slot->obs_key = key;
        slot->seen_ms = now_ms;
    }

    if (now_ms - slot->seen_ms <= kAlertMaxAgeMs)
        d.notify = notify_for(*slot, d.assessment.level, now_ms, d.escalated);
    return d;
}

// A level is announced once. It is said again only while it is urgent, and the
// level we last said falls back only after the contact has been calmer than it
// for a whole re-notification window, so a target oscillating across a ring
// boundary is announced once, not twice a second.
bool AlarmTracker::notify_for(Slot& slot, Level level, uint32_t now_ms, bool& escalated) {
    bool speak = false;
    if (level > slot.notified_level) {
        speak = true;
        escalated = true;
    } else if (level == slot.notified_level && level >= kReminderLevel &&
               now_ms - slot.notified_ms >= kRenotifyMs) {
        speak = true;
    }

    if (level < slot.notified_level) {
        if (!slot.falling) {
            slot.falling = true;
            slot.falling_since_ms = now_ms;
        } else if (now_ms - slot.falling_since_ms >= kRenotifyMs) {
            slot.notified_level = level;
            slot.falling = false;
        }
    } else {
        slot.falling = false;
    }

    if (speak) {
        slot.notified_level = level;
        slot.notified_ms = now_ms;
    }
    return speak;
}

AlarmTracker::Slot* AlarmTracker::slot_for(const model::AircraftObs& target, uint32_t now_ms) {
    Slot* free_slot = nullptr;
    Slot* oldest = nullptr;
    for (Slot& s : slots_) {
        if (s.used && s.addr == target.addr && s.addr_table == target.addr_table) return &s;
        if (!s.used) {
            if (free_slot == nullptr) free_slot = &s;
            continue;
        }
        if (oldest == nullptr || now_ms - s.seen_ms > now_ms - oldest->seen_ms) oldest = &s;
    }
    Slot* slot = free_slot != nullptr ? free_slot : oldest;
    if (slot == nullptr) return nullptr;
    *slot = Slot{};
    slot->used = true;
    slot->addr = target.addr;
    slot->addr_table = target.addr_table;
    slot->seen_ms = now_ms;
    return slot;
}

void AlarmTracker::forget_stale(uint32_t now_ms) {
    for (Slot& s : slots_) {
        if (s.used && now_ms - s.seen_ms > kForgetMs) s = Slot{};
    }
}

Level AlarmTracker::announced_level(uint32_t now_ms) const {
    Level level = Level::None;
    for (const Slot& s : slots_) {
        if (!s.used || now_ms - s.seen_ms > kAlertMaxAgeMs) continue;
        level = std::max(s.notified_level, level);
    }
    return level;
}

}  // namespace skyblip::traffic
