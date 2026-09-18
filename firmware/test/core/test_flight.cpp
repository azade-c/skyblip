// Airborne or not, from the fix stream: it gates the DFU lockout and the transmit rate, never a
// page.
#include "core/flight/ground.h"
#include "core/flight/state.h"
#include "doctest/doctest.h"

using namespace skyblip;
using namespace skyblip::flight;

namespace {

FlightSample solution(double mps, double climb_mps, uint16_t hdop_e2 = 90) {
    FlightSample s{};
    s.speed_q = static_cast<uint16_t>(mps * 4);
    s.climb_e8 = static_cast<int16_t>(climb_mps * 8);
    s.climb_valid = true;
    s.hdop_e2 = hdop_e2;
    s.fix_valid = true;
    return s;
}

// One second of solutions at a time, the cadence the receiver is configured for.
FlightState hold(FlightMonitor& monitor, int seconds, double mps, double climb_mps,
                 uint16_t hdop_e2 = 90) {
    FlightState state = monitor.state();
    for (int i = 0; i < seconds; i++) state = monitor.update(solution(mps, climb_mps, hdop_e2));
    return state;
}

}  // namespace

// 23 kt is a speed no aircraft taxis at and every takeoff roll passes it well before it flies.
TEST_CASE("flight: a takeoff is the speed no taxi holds, and it waits for nothing") {
    FlightMonitor monitor;
    REQUIRE(hold(monitor, 3, 0.0, 0.0) == FlightState::OnGround);

    CHECK(hold(monitor, 1, 6.0, 0.0) == FlightState::OnGround);   // a brisk taxi
    CHECK(hold(monitor, 1, 11.0, 0.0) == FlightState::OnGround);  // the roll, accelerating
    CHECK(hold(monitor, 1, 12.0, 0.0) == FlightState::Airborne);
}

// Wind on the nose at the ridge: flying, working, and its ground speed is a walking pace.
TEST_CASE("flight: a ridge start with the wind on the nose is flying") {
    FlightMonitor monitor;
    REQUIRE(hold(monitor, 3, 0.0, 0.0) == FlightState::OnGround);

    // 3 m/s over the ground, 2.5 m/s up: no ground vehicle does the second half.
    CHECK(hold(monitor, 1, 3.0, 2.5) == FlightState::Airborne);
    CHECK(hold(monitor, 30, 3.0, 0.2) == FlightState::Airborne);
}

// A glider towed to the grid, a tug taxiing back, a trailer on the perimeter track.
TEST_CASE("flight: a taxi does not take off, and one bad solution does not either") {
    FlightMonitor monitor;
    REQUIRE(hold(monitor, 3, 0.0, 0.0) == FlightState::OnGround);
    // A tug hurrying back to the grid does 10 m/s, and it is what this threshold is set over.
    CHECK(hold(monitor, 30, 10.0, 0.0) == FlightState::OnGround);

    CHECK(monitor.update(solution(45.0, 0.0)) == FlightState::OnGround);
    CHECK(hold(monitor, 20, 10.0, 0.0) == FlightState::OnGround);
}

// The rollout is the landing: slow and level, with nothing left to explain.
TEST_CASE("flight: a landing is a standstill, and a fast rollout is not one yet") {
    FlightMonitor monitor;
    REQUIRE(hold(monitor, 3, 25.0, 0.0) == FlightState::Airborne);

    CHECK(hold(monitor, 1, 20.0, -1.0) == FlightState::Airborne);
    CHECK(hold(monitor, 1, 8.0, 0.0) == FlightState::Airborne);
    CHECK(hold(monitor, 1, 2.0, 0.0) == FlightState::Airborne);
    CHECK(hold(monitor, 1, 0.5, 0.0) == FlightState::OnGround);
}

// A climb is only evidence from something that is moving: a parked barometer is not.
TEST_CASE("flight: a vertical rate at a standstill takes nothing off") {
    FlightMonitor monitor;
    REQUIRE(hold(monitor, 3, 0.0, 0.0) == FlightState::OnGround);

    CHECK(hold(monitor, 5, 0.5, 4.0) == FlightState::OnGround);
    CHECK(hold(monitor, 5, 0.5, -4.0) == FlightState::OnGround);
}

// A wing soaring a ridge in wind hangs over one spot: its sink rate is all that says it flies.
TEST_CASE("flight: a wing hovering over the ground in strong wind has not landed") {
    FlightMonitor monitor;
    REQUIRE(hold(monitor, 3, 25.0, 0.0) == FlightState::Airborne);

    // Minimum sink is 0.9 m/s for a hang glider and 1.0 for a paraglider, both below the wind.
    CHECK(hold(monitor, 60, 0.5, -0.9) == FlightState::Airborne);
    CHECK(hold(monitor, 60, 0.0, 0.8) == FlightState::Airborne);

    // A standstill shows the receiver's own vertical noise, and lands on the first solution.
    CHECK(hold(monitor, 1, 0.5, 0.5) == FlightState::OnGround);
}

// What no hold costs: a wing lifting during a fast rollout is a second session in the log.
TEST_CASE("flight: a bounce during a rollout is a second takeoff and a second landing") {
    FlightMonitor monitor;
    REQUIRE(hold(monitor, 3, 25.0, 0.0) == FlightState::Airborne);
    REQUIRE(hold(monitor, 1, 0.5, 0.0) == FlightState::OnGround);

    CHECK(hold(monitor, 1, 5.0, 2.5) == FlightState::Airborne);
    CHECK(hold(monitor, 1, 0.5, 0.0) == FlightState::OnGround);
}

// Evidence is worth what the fix behind it is worth, so OGN divides by any DOP above 1.0.
TEST_CASE("flight: a fix nobody should trust does not take off on its own") {
    FlightMonitor monitor;
    REQUIRE(hold(monitor, 3, 0.0, 0.0, 3000) == FlightState::OnGround);
    CHECK(hold(monitor, 20, 12.0, 0.0, 3000) == FlightState::OnGround);

    // The same movement on a fix worth trusting is a takeoff.
    CHECK(hold(monitor, 1, 12.0, 0.0, 90) == FlightState::Airborne);
}

// The derating cuts one way: a poor sky may refuse a takeoff, never declare a landing.
TEST_CASE("flight: the dilution of precision cannot land an aircraft") {
    CHECK_FALSE(flight_evidence(solution(12.0, 0.0, 3000)));
    CHECK_FALSE(ground_evidence(solution(12.0, 0.0, 3000)));
    CHECK(flight_evidence(solution(12.0, 0.0, 90)));

    CHECK(flight_evidence(solution(3.0, 2.0)));
    CHECK(flight_evidence(solution(3.0, -2.0)));
    CHECK_FALSE(flight_evidence(solution(0.0, 4.0)));
    // A climb is armed at 2.5 m/s, which a wing being pushed to the grid in a gust does not have.
    CHECK_FALSE(flight_evidence(solution(1.2, 4.0)));
    CHECK(ground_evidence(solution(0.5, 0.6)));
    CHECK_FALSE(ground_evidence(solution(0.5, 0.9)));
    CHECK_FALSE(ground_evidence(solution(1.2, 0.0)));
    // A receiver that reports no DOP at all is not punished for it.
    CHECK(flight_evidence(solution(12.0, 0.0, 0)));
}

// The first solution decides on its own evidence: a device rebooted in flight must not wait.
TEST_CASE("flight: a device switched on in the air says so at once") {
    FlightMonitor airborne;
    CHECK(airborne.update(solution(30.0, 0.0)) == FlightState::Airborne);

    FlightMonitor thermalling;
    CHECK(thermalling.update(solution(12.0, 2.5)) == FlightState::Airborne);

    // Switched on while being towed to the grid, which is the common half of this.
    FlightMonitor rolling;
    CHECK(rolling.update(solution(6.0, 0.0)) == FlightState::OnGround);

    FlightMonitor parked;
    CHECK(parked.update(solution(0.4, 0.4)) == FlightState::OnGround);
}

// Without a fix there is no claim to make, and the aircraft is still where it was.
TEST_CASE("flight: no fix is not a landing") {
    FlightMonitor monitor;
    REQUIRE(hold(monitor, 3, 30.0, 0.0) == FlightState::Airborne);

    FlightSample lost{};
    CHECK(monitor.update(lost) == FlightState::Unknown);
    CHECK(monitor.state() == FlightState::Airborne);

    CHECK(monitor.update(solution(30.0, 0.0)) == FlightState::Airborne);
}

// The jerk gate compares two consecutive solutions, so an outage must not arm it.
TEST_CASE("flight: the first solution after an outage is not judged as a jerk") {
    FlightMonitor monitor;
    REQUIRE(hold(monitor, 3, 0.0, 0.0) == FlightState::OnGround);

    FlightSample lost{};
    REQUIRE(monitor.update(lost) == FlightState::Unknown);
    CHECK(monitor.update(solution(30.0, 0.0)) == FlightState::Airborne);
}

TEST_CASE("flight: only the two ADS-L G.1.2 codes name a state, every other value is unknown") {
    CHECK(state_from(static_cast<uint8_t>(FlightState::OnGround)) == FlightState::OnGround);
    CHECK(state_from(static_cast<uint8_t>(FlightState::Airborne)) == FlightState::Airborne);
    CHECK(state_from(static_cast<uint8_t>(FlightState::Unknown)) == FlightState::Unknown);
    CHECK(airborne(static_cast<uint8_t>(FlightState::Airborne)));
    CHECK_FALSE(airborne(static_cast<uint8_t>(FlightState::OnGround)));

    // Two bits, and we own neither the sender nor the future: an unknown code unlocks nothing.
    for (uint16_t code = 3; code < 256; code++)
        CHECK(state_from(static_cast<uint8_t>(code)) == FlightState::Unknown);
}

TEST_CASE("flight: a lost fix is not a landing, so the ground latch holds airborne") {
    GroundLatch latch;
    CHECK(latch.state() == FlightState::Unknown);
    CHECK_FALSE(latch.on_ground());

    latch.update(FlightState::OnGround);
    CHECK(latch.on_ground());

    // Unknown before anything was confirmed is not a ground: every gate behind this fails closed.
    latch.update(FlightState::Unknown);
    CHECK(latch.state() == FlightState::Unknown);
    CHECK_FALSE(latch.on_ground());

    latch.update(FlightState::Airborne);
    latch.update(FlightState::Unknown);
    CHECK(latch.state() == FlightState::Airborne);

    latch.update(FlightState::OnGround);
    CHECK(latch.on_ground());
}
