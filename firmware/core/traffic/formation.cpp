#include "core/traffic/formation.h"

#include "core/flight/extrapolate.h"
#include "core/protocol/nmea_out.h"
#include "core/units/units.h"
#include "core/util/intmath.h"

namespace skyblip::formation {

namespace {

constexpr int32_t kQ14One = 16384;

int32_t iabs32(int32_t v) { return v < 0 ? -v : v; }

void heading_up(int32_t north_m, int32_t east_m, int32_t track_cdeg, int32_t& ahead_m,
                int32_t& right_m) {
    const int16_t track = to_angle16(CentiDegrees(track_cdeg));
    const int64_t c = icos(track), s = isin(track);
    ahead_m = static_cast<int32_t>(div_round<int64_t>(north_m * c + east_m * s, kQ14One));
    right_m = static_cast<int32_t>(div_round<int64_t>(east_m * c - north_m * s, kQ14One));
}

}  // namespace

Report Tracker::observe(const model::OwnState& own_fix, const model::AircraftObs& reported,
                        uint32_t now_ms) {
    Report out{};
    const model::OwnState own = flight::carried_to(own_fix, now_ms);
    const model::AircraftObs target = flight::carried_to(reported, now_ms);
    int32_t north_m, east_m, up_m;
    if (!protocol::relative_ned(own, target, north_m, east_m, up_m)) return out;

    heading_up(north_m, east_m, own.track_cdeg, out.ahead_m, out.right_m);
    out.up_m = up_m;
    out.valid = true;

    Slot* slot = slot_for(target, now_ms);
    if (slot == nullptr) return out;
    slot->seen_ms = now_ms;

    const int32_t range_m = static_cast<int32_t>(idistance(north_m, east_m));
    if (range_m > kRangeM || iabs32(up_m) > kVertM) {
        *slot = Slot{};
        return out;
    }

    const int32_t drift_m = static_cast<int32_t>(
        idistance(out.ahead_m - slot->ref_ahead_m, out.right_m - slot->ref_right_m));
    const bool station_kept =
        drift_m <= kDriftM && iabs32(out.up_m - slot->ref_up_m) <= kVertDriftM;

    if (station_kept) {
        slot->drift_fixes = 0;
        if (slot->state != State::Together && now_ms - slot->steady_since_ms >= kTogetherHoldMs)
            slot->state = State::Together;
        out.state = slot->state;
        return out;
    }

    slot->drift_fixes++;
    slot->ref_ahead_m = out.ahead_m;
    slot->ref_right_m = out.right_m;
    slot->ref_up_m = out.up_m;
    slot->steady_since_ms = now_ms;
    if (slot->state == State::Together && slot->drift_fixes >= kBreakFixes) {
        slot->state = State::Parting;
        slot->drift_fixes = 0;
    }
    out.state = slot->state;
    return out;
}

void Tracker::release(uint8_t addr_table, uint32_t addr, uint32_t now_ms) {
    Slot* slot = find(addr_table, addr);
    if (slot == nullptr) return;
    slot->state = State::None;
    slot->drift_fixes = 0;
    slot->steady_since_ms = now_ms;
}

bool Tracker::together(uint8_t addr_table, uint32_t addr) const {
    const Slot* slot = find(addr_table, addr);
    return slot != nullptr && slot->state == State::Together;
}

int Tracker::members() const {
    int n = 0;
    for (const Slot& s : slots_)
        if (s.used && s.state == State::Together) n++;
    return n;
}

void Tracker::forget_stale(uint32_t now_ms) {
    for (Slot& s : slots_)
        if (s.used && now_ms - s.seen_ms > kContactForgetMs) s = Slot{};
}

Tracker::Slot* Tracker::slot_for(const model::AircraftObs& target, uint32_t now_ms) {
    Slot* free_slot = nullptr;
    Slot* oldest = nullptr;
    for (Slot& s : slots_) {
        if (s.used && s.addr == target.addr && s.addr_table == target.addr_table) return &s;
        if (!s.used) {
            if (free_slot == nullptr) free_slot = &s;
            continue;
        }
        if (s.state == State::Together) continue;
        if (oldest == nullptr || now_ms - s.seen_ms > now_ms - oldest->seen_ms) oldest = &s;
    }
    Slot* slot = free_slot != nullptr ? free_slot : oldest;
    if (slot == nullptr) return nullptr;
    *slot = Slot{};
    slot->used = true;
    slot->addr = target.addr;
    slot->addr_table = target.addr_table;
    slot->seen_ms = now_ms;
    slot->steady_since_ms = now_ms;
    return slot;
}

const Tracker::Slot* Tracker::find(uint8_t addr_table, uint32_t addr) const {
    for (const Slot& s : slots_)
        if (s.used && s.addr == addr && s.addr_table == addr_table) return &s;
    return nullptr;
}

Tracker::Slot* Tracker::find(uint8_t addr_table, uint32_t addr) {
    for (Slot& s : slots_)
        if (s.used && s.addr == addr && s.addr_table == addr_table) return &s;
    return nullptr;
}

}  // namespace skyblip::formation
