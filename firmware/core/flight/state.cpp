#include "core/flight/state.h"

namespace skyblip::flight {

namespace {

int32_t derated(int32_t evidence, uint16_t hdop_e2) {
    if (hdop_e2 <= kDopUnityE2) return evidence;
    return evidence * kDopUnityE2 / static_cast<int32_t>(hdop_e2);
}

}  // namespace

bool flight_evidence(const FlightSample& sample) {
    return derated(sample.speed_q, sample.hdop_e2) >= kFlightSpeedQ;
}

bool ground_evidence(const FlightSample& sample) { return sample.speed_q < kGroundSpeedQ; }

FlightState state_from(uint8_t adsl_code) {
    switch (static_cast<FlightState>(adsl_code)) {
        case FlightState::OnGround: return FlightState::OnGround;
        case FlightState::Airborne: return FlightState::Airborne;
        case FlightState::Unknown: break;
    }
    return FlightState::Unknown;
}

bool FlightMonitor::jerky(uint16_t previous_q, uint16_t now_q) {
    const int32_t previous = previous_q;
    const int32_t now = now_q;
    return now > previous * kJerkSpeedRatio || previous > now * kJerkSpeedRatio;
}

FlightState FlightMonitor::update(const FlightSample& sample) {
    if (!sample.fix_valid) {
        armed_ = false;
        return FlightState::Unknown;
    }

    const uint16_t previous_speed_q = last_speed_q_;
    const bool comparable = armed_;
    armed_ = true;
    last_speed_q_ = sample.speed_q;

    if (state_ == FlightState::Unknown) {
        state_ = flight_evidence(sample) ? FlightState::Airborne : FlightState::OnGround;
        return state_;
    }

    if (state_ == FlightState::Airborne) {
        if (ground_evidence(sample)) state_ = FlightState::OnGround;
        return state_;
    }

    if (flight_evidence(sample) && !(comparable && jerky(previous_speed_q, sample.speed_q)))
        state_ = FlightState::Airborne;
    return state_;
}

}  // namespace skyblip::flight
