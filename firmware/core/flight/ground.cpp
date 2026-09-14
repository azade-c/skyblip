#include "core/flight/ground.h"

namespace skyblip::flight {

void GroundLatch::update(FlightState reported) {
    if (reported == FlightState::Airborne) airborne_latched_ = true;
    if (reported == FlightState::OnGround) airborne_latched_ = false;

    if (airborne_latched_)
        state_ = FlightState::Airborne;
    else if (reported == FlightState::OnGround)
        state_ = FlightState::OnGround;
    else
        state_ = FlightState::Unknown;
}

}  // namespace skyblip::flight
