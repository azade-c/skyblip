#include "core/flight/arc.h"

#include "core/util/intmath.h"

namespace skyblip::flight {

namespace {

constexpr int64_t kTrigOne = 16384;
constexpr int kTrackC9ToAngle = 7;
constexpr uint16_t kTrackC9Mask = 0x1FF;
constexpr int64_t kTurn16 = 65536;
constexpr int64_t kMilliDegreesPerTurn = 360000;
constexpr int32_t kMmPerSpeedQ = 250;
constexpr int32_t kMmPerClimbE8 = 125;
constexpr int64_t kMsPerS = 1000;
constexpr int64_t kMmPerM = 1000;

int64_t div_round(int64_t num, int64_t den) {
    return num >= 0 ? (num + den / 2) / den : -((-num + den / 2) / den);
}

int16_t angle16_of(uint16_t track_c9) {
    return static_cast<int16_t>(
        static_cast<uint16_t>((track_c9 & kTrackC9Mask) << kTrackC9ToAngle));
}

int16_t turn_angle16(int16_t turn_dps, int32_t dt_ms) {
    return static_cast<int16_t>(static_cast<uint16_t>(
        div_round(static_cast<int64_t>(turn_dps) * dt_ms * kTurn16, kMilliDegreesPerTurn)));
}

}  // namespace

int16_t clamped_turn_dps(int16_t turn_dps) {
    if (turn_dps > kMaxTurnDps) return kMaxTurnDps;
    if (turn_dps < -kMaxTurnDps) return -kMaxTurnDps;
    return turn_dps;
}

Motion motion_of(const model::OwnState& own) {
    Motion m{};
    m.speed_q = own.speed_q;
    m.track_c9 = own.track_c9;
    m.turn_dps = clamped_turn_dps(own.turn_dps);
    m.turning = true;
    m.climb_e8 = own.climb_e8;
    m.climbing = own.climb_valid;
    return m;
}

Motion motion_of(const model::AircraftObs& obs, int16_t turn_dps, bool turn_valid) {
    Motion m{};
    m.speed_q = obs.speed_valid ? obs.speed_q : 0;
    m.track_c9 = obs.track_c9;
    m.turn_dps = turn_valid ? clamped_turn_dps(turn_dps) : 0;
    m.turning = turn_valid;
    m.climb_e8 = obs.climb_e8;
    m.climbing = obs.climb_valid;
    return m;
}

Arc::Arc(const Motion& motion, uint32_t step_ms)
    : north_mm_(static_cast<int64_t>(motion.north_m) * kMmPerM),
      east_mm_(static_cast<int64_t>(motion.east_m) * kMmPerM),
      up_mm_(static_cast<int64_t>(motion.up_m) * kMmPerM),
      climb_mm_s_(motion.climbing ? static_cast<int32_t>(motion.climb_e8) * kMmPerClimbE8 : 0),
      step_ms_(static_cast<int32_t>(step_ms)) {
    const int16_t angle = angle16_of(motion.track_c9);
    const int32_t speed_mm_s = static_cast<int32_t>(motion.speed_q) * kMmPerSpeedQ;
    vel_north_mm_s_ = static_cast<int32_t>(div_round(static_cast<int64_t>(speed_mm_s) * icos(angle),
                                                     kTrigOne));
    vel_east_mm_s_ = static_cast<int32_t>(div_round(static_cast<int64_t>(speed_mm_s) * isin(angle),
                                                    kTrigOne));
    const int16_t half =
        motion.turning ? turn_angle16(clamped_turn_dps(motion.turn_dps), step_ms_ / 2) : 0;
    half_cos_ = icos(half);
    half_sin_ = isin(half);
}

Position Arc::here() const {
    Position p{};
    p.north_m = static_cast<int32_t>(div_round(north_mm_, kMmPerM));
    p.east_m = static_cast<int32_t>(div_round(east_mm_, kMmPerM));
    p.up_m = static_cast<int32_t>(div_round(up_mm_, kMmPerM));
    return p;
}

void Arc::rotate_half() {
    const int64_t n = vel_north_mm_s_, e = vel_east_mm_s_;
    vel_north_mm_s_ = static_cast<int32_t>(div_round(n * half_cos_ - e * half_sin_, kTrigOne));
    vel_east_mm_s_ = static_cast<int32_t>(div_round(n * half_sin_ + e * half_cos_, kTrigOne));
}

Position Arc::advance() {
    rotate_half();
    north_mm_ += div_round(static_cast<int64_t>(vel_north_mm_s_) * step_ms_, kMsPerS);
    east_mm_ += div_round(static_cast<int64_t>(vel_east_mm_s_) * step_ms_, kMsPerS);
    up_mm_ += div_round(static_cast<int64_t>(climb_mm_s_) * step_ms_, kMsPerS);
    rotate_half();
    return here();
}

}  // namespace skyblip::flight
