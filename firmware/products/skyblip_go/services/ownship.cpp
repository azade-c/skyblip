#include "products/skyblip_go/services/ownship.h"

#include "core/events/sensor.h"
#include "core/flight/arc.h"
#include "core/flight/atmosphere.h"
#include "core/flight/turn.h"
#include "core/model/ownship.h"
#include "core/units/units.h"
#include "core/util/intmath.h"

namespace skyblip::go {

flight::FlightState OwnshipService::flight_state_from(const model::OwnState& own) {
    flight::FlightSample sample{};
    sample.speed_mm_s = own.speed_mm_s;
    sample.hdop_e2 = own.hdop_e2;
    sample.fix_valid = own.fix_valid;
    return flight_.update(sample);
}

void OwnshipService::tick(uint32_t now_ms) {
    gnss::GnssSolution solution{};
    while (context_.bus.gnss.pop(solution)) apply_solution(solution, now_ms);

    events::BaroSample sample{};
    while (context_.bus.baro.pop(sample)) apply_baro(sample);

    events::AccelSample specific_force{};
    while (context_.bus.accel.pop(specific_force)) apply_accel(specific_force);
    events::RateSample rotation{};
    while (context_.bus.rate.pop(rotation)) apply_rate(rotation, now_ms);
    publish_inertial(now_ms);

    timer_.update(flight_.state(), now_ms);
    context_.state.flight.confirmed_state = ground_.state();
    context_.state.flight.seconds = timer_.seconds();
    context_.state.flight.time_valid = timer_.flown();
    context_.state.flight.running = timer_.running();
    context_.state.flight.rolling = flight_.rolling();

    acquisition_.tick(now_ms);
    context_.state.gnss.stage = acquisition_.stage();
    context_.state.gnss.stage_s = acquisition_.stage_ms(now_ms) / 1000;

    context_.state.baro.active = baro_active();
    context_.state.own.tx_settled = settle_.settled(now_ms);
    context_.state.own.fix_acquired = settle_.take_acquired();
}

void OwnshipService::apply_solution(const gnss::GnssSolution& solution, uint32_t now_ms) {
    model::OwnState& own = context_.state.own;
    const model::OwnState previous = own;
    context_.state.flight.gnss_solutions++;
    acquisition_.observe(solution, now_ms);
    context_.state.gnss.fix_mode = solution.fix_mode;

    own.fix_valid = solution.is_fix;
    own.utc_valid = solution.utc_valid;
    own.lat_1e7 = solution.lat_1e7;
    own.lon_1e7 = solution.lon_1e7;
    own.alt_mm = solution.alt_mm;
    own.alt_msl_mm = solution.alt_msl_mm;
    own.geoid_separation_measured = solution.geoid_separation_measured;
    own.speed_mm_s = solution.speed_mm_s;
    own.track_cdeg = solution.track_cdeg;
    own.hdop_e2 = solution.hdop_e2;
    own.vdop_e2 = solution.vdop_e2;
    own.utc = solution.utc;
    own.fix_ms = solution_instant(solution, now_ms);
    own.sats = solution.sats;
    own.aircraft_cat = settings_.aircraft_type;

    context_.state.clock.utc_valid = solution.utc_valid;
    anchor_utc(solution);

    // A barometer, once it has spoken, owns vertical speed. The GNSS reference
    // keeps moving anyway so losing the sensor falls back seamlessly.
    int32_t mm_s = 0;
    const bool have =
        vs_from_alt_mm(solution.alt_mm, now_ms, kVsWindowMs, vs_ref_alt_mm_, vs_ref_ms_, mm_s);
    if (have && !baro_active()) adopt_climb(mm_s);

    const flight::FlightState declared = flight_state_from(own);
    own.flight_state = static_cast<uint8_t>(declared);
    ground_.update(declared);
    update_turn_rate(now_ms);
    update_residual(previous);
    settle_.update(convergence_of(own), now_ms);
}

gnss::Convergence OwnshipService::convergence_of(const model::OwnState& own) {
    gnss::Convergence c{};
    c.fix_valid = own.fix_valid;
    c.resid_valid = own.pred_resid_valid;
    c.height_solved = own.vdop_e2 != 0;
    c.resid_m = own.pred_resid_m;
    return c;
}

void OwnshipService::update_residual(const model::OwnState& previous) {
    model::OwnState& own = context_.state.own;
    if (!previous.fix_valid || !own.fix_valid) {
        own.pred_resid_valid = false;
        return;
    }
    const int32_t dt_ms = static_cast<int32_t>(own.fix_ms - previous.fix_ms);
    if (dt_ms <= 0) {
        own.pred_resid_valid = false;
        return;
    }
    const flight::Prediction p = flight::extrapolate(previous, dt_ms);
    if (!p.valid) {
        own.pred_resid_valid = false;
        return;
    }
    const uint32_t resid = flight::prediction_residual_m(p, own.lat_1e7, own.lon_1e7, own.alt_mm);
    own.pred_resid_m = resid > 0xFFFF ? 0xFFFF : static_cast<uint16_t>(resid);
    own.pred_resid_valid = true;
}

// INFO: fc 16sep26 a sentence names the second its own edge opened, and only that edge dates it
void OwnshipService::anchor_utc(const gnss::GnssSolution& solution) {
    timing::ClockState& clock = context_.state.clock;
    if (!solution.utc_valid || !clock.pps_locked) return;
    if (context_.state.own.fix_ms != static_cast<uint32_t>(clock.pps_edge_us / 1000)) return;
    clock.utc_s = solution.utc;
    clock.utc_edge_us = clock.pps_edge_us;
}

// INFO: fc 13sep26 the latched edge dates the solution exactly, the estimate only when it is lost
uint32_t OwnshipService::solution_instant(const gnss::GnssSolution& solution,
                                          uint32_t now_ms) const {
    const timing::ClockState& clock = context_.state.clock;
    return gnss::solution_instant_ms(
        solution, now_ms, static_cast<uint32_t>(clock.pps_edge_us / 1000), clock.pps_locked);
}

void OwnshipService::apply_baro(const events::BaroSample& sample) {
    context_.state.baro.pressure_mpa = sample.pressure_mpa;
    const int32_t alt_mm = flight::pressure_to_alt_mm(sample.pressure_mpa);
    int32_t mm_s = 0;
    if (vs_from_alt_mm(alt_mm, sample.at_ms, kBaroVsWindowMs, baro_ref_alt_mm_, baro_ref_ms_, mm_s))
        adopt_climb(mm_s);
    update_derived_qnh(sample);
}

void OwnshipService::apply_accel(const events::AccelSample& sample) {
    ball_.update(sample.right_mg, sample.up_mg, sample.aft_mg, sample.at_ms);
    force_ = flight::SpecificForce{sample.right_mg, sample.up_mg, sample.aft_mg};
    force_seen_ = true;
    gmeter_.observe(force_, sample.at_ms);
}

void OwnshipService::apply_rate(const events::RateSample& sample, uint32_t now_ms) {
    if (!force_seen_) return;
    const flight::BodyRate rate{sample.roll_cdps, sample.pitch_cdps, sample.yaw_cdps};
    const model::OwnState& own = context_.state.own;

    gyro_turn_.observe(rate, force_, sample.at_ms);
    if (flight_.state() == flight::FlightState::OnGround &&
        own.speed_mm_s <= flight::kGroundSpeedMmS)
        gyro_turn_.trim_to(0);

    const int32_t speed_mps = own.fix_valid ? to_mps(MillimetresPerSec(own.speed_mm_s)).v : 0;
    const int32_t turn_cdps = gyro_turn_.valid(now_ms) ? gyro_turn_.cdps() : 0;
    bank_.observe(rate, force_, speed_mps, turn_cdps, sample.at_ms);
}

void OwnshipService::publish_inertial(uint32_t now_ms) {
    context_.state.slip.valid = ball_.valid(now_ms);
    context_.state.slip.lateral_mg = ball_.mg();

    context_.state.bank.valid = bank_.valid(now_ms) && context_.state.own.fix_valid;
    context_.state.bank.deg = bank_.deg();

    context_.state.gload.valid = gmeter_.valid(now_ms);
    context_.state.gload.now = gmeter_.now();
    context_.state.gload.most = gmeter_.most();
    context_.state.gload.least = gmeter_.least();

    const bool flying = timer_.running();
    if (flying && !flying_) gmeter_.reset();
    flying_ = flying;

    if (!gyro_turn_.valid(now_ms)) return;
    context_.state.own.turn_cdps = gyro_turn_.cdps();
}

void OwnshipService::adopt_climb(int32_t mm_s) {
    model::OwnState& own = context_.state.own;
    own.climb_mm_s = mm_s;
    own.climb_valid = true;
}

void OwnshipService::update_derived_qnh(const events::BaroSample& sample) {
    const model::OwnState& own = context_.state.own;
    if (!own.fix_valid) {
        qnh_filter_acc_ = 0;
        context_.state.baro.derived_qnh_pa = 0;
        return;
    }
    const bool manoeuvring = own.climb_valid && (own.climb_mm_s > kQnhSteadyClimbMmS ||
                                                 own.climb_mm_s < -kQnhSteadyClimbMmS);
    if (manoeuvring) return;

    uint32_t qnh_pa = 0;
    if (!flight::qnh_from_alt(div_round<uint32_t>(sample.pressure_mpa, 1000),
                              div_round(own.alt_msl_mm, 10), qnh_pa))
        return;

    const int32_t sampled = static_cast<int32_t>(qnh_pa) * kQnhHalfMinuteSamples;
    qnh_filter_acc_ = qnh_filter_acc_ == 0
                          ? sampled
                          : qnh_filter_acc_ + (sampled - qnh_filter_acc_) / kQnhHalfMinuteSamples;
    context_.state.baro.derived_qnh_pa = static_cast<uint32_t>(
        (qnh_filter_acc_ + kQnhHalfMinuteSamples / 2) / kQnhHalfMinuteSamples);
}

void OwnshipService::update_turn_rate(uint32_t now_ms) {
    const CentiDegrees track{context_.state.own.track_cdeg};
    if (turn_ref_ms_ == 0) {
        turn_ref_ms_ = now_ms == 0 ? 1 : now_ms;
        turn_ref_track_cdeg_ = track.v;
        return;
    }
    const uint32_t dt = now_ms - turn_ref_ms_;
    if (dt < kTurnWindowMs) return;

    const int16_t gnss_cdps = flight::clamped_turn_cdps(
        flight::turn_rate_cdps(track, CentiDegrees(turn_ref_track_cdeg_), dt));
    turn_ref_ms_ = now_ms;
    turn_ref_track_cdeg_ = track.v;

    if (gyro_turn_.valid(now_ms)) {
        if (flight_.airborne() && context_.state.own.fix_valid) gyro_turn_.trim_to(gnss_cdps);
        return;
    }
    context_.state.own.turn_cdps = gnss_cdps;
}

bool OwnshipService::vs_from_alt_mm(int32_t alt_mm, uint32_t now_ms, uint32_t window_ms,
                                    int32_t& ref_alt_mm, uint32_t& ref_ms, int32_t& out_mm_s) {
    if (ref_ms == 0) {
        ref_ms = now_ms == 0 ? 1 : now_ms;
        ref_alt_mm = alt_mm;
        return false;
    }
    if (now_ms - ref_ms < window_ms) return false;

    const bool ok = flight::climb_mm_s_from_alt(alt_mm, ref_alt_mm, now_ms - ref_ms, out_mm_s);
    ref_ms = now_ms;
    ref_alt_mm = alt_mm;
    return ok;
}

}  // namespace skyblip::go
