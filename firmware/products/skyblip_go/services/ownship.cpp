#include "products/skyblip_go/services/ownship.h"

#include "core/flight/atmosphere.h"
#include "core/flight/turn.h"

namespace skyblip::go {

flight::FlightState OwnshipService::flight_state_from(const messages::OwnState& own,
                                                      uint32_t now_ms) {
    flight::FlightSample sample{};
    sample.at_ms = now_ms;
    sample.speed_q = own.speed_q;
    sample.climb_e8 = own.climb_e8;
    sample.alt_msl_m = own.alt_msl_m;
    sample.hdop_e2 = own.hdop_e2;
    sample.fix_valid = own.fix_valid;
    sample.climb_valid = own.climb_valid;
    return flight_.update(sample);
}

void OwnshipService::tick(uint32_t now_ms) {
    gnss::GnssSolution solution{};
    while (context_.bus.gnss.pop(solution)) apply_solution(solution, now_ms);

    messages::BaroSample sample{};
    while (context_.bus.baro.pop(sample)) apply_baro(sample);

    timer_.update(flight_.state(), now_ms);
    context_.state.confirmed_flight_state = ground_.state();
    context_.state.flight_seconds = timer_.seconds();
    context_.state.flight_time_valid = timer_.flown();
    context_.state.flight_running = timer_.running();

    context_.state.baro_active = baro_active();
    context_.state.own.tx_settled = settle_.settled(now_ms);
    context_.state.own.fix_acquired = settle_.take_acquired();
}

void OwnshipService::apply_solution(const gnss::GnssSolution& f, uint32_t now_ms) {
    messages::OwnState& own = context_.state.own;
    const messages::OwnState previous = own;
    context_.state.gnss_solutions++;
    settle_.update(f.is_fix, now_ms);

    own.fix_valid = f.is_fix;
    own.utc_valid = f.utc_valid;
    own.lat_1e7 = f.lat_1e7;
    own.lon_1e7 = f.lon_1e7;
    own.alt_m = f.alt_m;
    own.alt_msl_m = f.alt_msl_m;
    own.geoid_separation_measured = f.geoid_separation_measured;
    own.speed_q = f.speed_q;
    own.track_c9 = f.track_c9;
    own.hdop_e2 = f.hdop_e2;
    own.vdop_e2 = f.vdop_e2;
    own.utc = f.utc;
    own.fix_ms = solution_instant(f, now_ms);
    own.sats = f.sats;
    own.aircraft_cat = context_.state.settings.aircraft_type;

    context_.state.clock.utc_valid = f.utc_valid;

    // A barometer, once it has spoken, owns vertical speed. The GNSS reference
    // keeps moving anyway so losing the sensor falls back seamlessly.
    int32_t mm_s = 0;
    const bool have =
        vs_from_alt_mm(f.alt_m * 1000, now_ms, kVsWindowMs, vs_ref_alt_mm_, vs_ref_ms_, mm_s);
    if (have && !baro_active()) adopt_climb(mm_s);

    const flight::FlightState declared = flight_state_from(own, now_ms);
    own.flight_state = static_cast<uint8_t>(declared);
    ground_.update(declared);
    update_turn_rate(now_ms);
    update_residual(previous);
}

// The model, run over the interval that has just elapsed, against the fix that
// closed it. Nothing acts on the answer: it is the bench's measure of whether
// the extrapolation the transmitter applies is describing this aircraft.
void OwnshipService::update_residual(const messages::OwnState& previous) {
    messages::OwnState& own = context_.state.own;
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
    const uint32_t resid = flight::prediction_residual_m(p, own.lat_1e7, own.lon_1e7, own.alt_m);
    own.pred_resid_m = resid > 0xFFFF ? 0xFFFF : static_cast<uint16_t>(resid);
    own.pred_resid_valid = true;
}

// INFO: fc 13sep26 the latched edge dates the solution exactly, the estimate only when it is lost
uint32_t OwnshipService::solution_instant(const gnss::GnssSolution& f, uint32_t now_ms) const {
    const timing::ClockState& clock = context_.state.clock;
    return gnss::solution_instant_ms(f, now_ms, static_cast<uint32_t>(clock.pps_edge_us / 1000),
                                     clock.pps_locked);
}

void OwnshipService::apply_baro(const messages::BaroSample& sample) {
    context_.state.pressure_mpa = sample.pressure_mpa;
    const int32_t alt_mm = flight::pressure_to_alt_mm(sample.pressure_mpa);
    int32_t mm_s = 0;
    if (vs_from_alt_mm(alt_mm, sample.at_ms, kBaroVsWindowMs, baro_ref_alt_mm_, baro_ref_ms_, mm_s))
        adopt_climb(mm_s);
    update_derived_qnh(sample);
}

void OwnshipService::adopt_climb(int32_t mm_s) {
    messages::OwnState& own = context_.state.own;
    own.climb_mm_s = mm_s;
    own.climb_e8 = flight::climb_e8_from_mm_s(mm_s);
    own.climb_valid = true;
}

void OwnshipService::update_derived_qnh(const messages::BaroSample& sample) {
    const messages::OwnState& own = context_.state.own;
    if (!own.fix_valid) {
        qnh_filter_acc_ = 0;
        context_.state.derived_qnh_pa = 0;
        return;
    }
    const bool manoeuvring = own.climb_valid && (own.climb_mm_s > kQnhSteadyClimbMmS ||
                                                 own.climb_mm_s < -kQnhSteadyClimbMmS);
    if (manoeuvring) return;

    uint32_t qnh_pa = 0;
    if (!flight::qnh_from_alt(sample.pressure_mpa / 1000, own.alt_msl_m * 100, qnh_pa)) return;

    const int32_t sampled = static_cast<int32_t>(qnh_pa) * kQnhHalfMinuteSamples;
    qnh_filter_acc_ = qnh_filter_acc_ == 0
                          ? sampled
                          : qnh_filter_acc_ + (sampled - qnh_filter_acc_) / kQnhHalfMinuteSamples;
    context_.state.derived_qnh_pa = static_cast<uint32_t>(
        (qnh_filter_acc_ + kQnhHalfMinuteSamples / 2) / kQnhHalfMinuteSamples);
}

void OwnshipService::update_turn_rate(uint32_t now_ms) {
    const uint16_t track_c9 = context_.state.own.track_c9;
    if (turn_ref_ms_ == 0) {
        turn_ref_ms_ = now_ms == 0 ? 1 : now_ms;
        turn_ref_track_c9_ = track_c9;
        return;
    }
    const uint32_t dt = now_ms - turn_ref_ms_;
    if (dt < kTurnWindowMs) return;

    context_.state.own.turn_dps = flight::turn_rate_dps(track_c9, turn_ref_track_c9_, dt);
    turn_ref_ms_ = now_ms;
    turn_ref_track_c9_ = track_c9;
}

bool OwnshipService::vs_from_alt_mm(int32_t alt_mm, uint32_t now_ms, uint32_t window_ms,
                                    int32_t& ref_alt_mm, uint32_t& ref_ms,
                                    int32_t& out_mm_s) const {
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
