// Airborne or not, from the fix stream: it gates the DFU lockout and the transmit rate, never a
// page.
#include "core/flight/ground.h"
#include "core/flight/state.h"
#include "doctest/doctest.h"

using namespace skyblip;
using namespace skyblip::flight;

namespace {

FlightSample solution(double mps, uint16_t hdop_e2 = 90) {
    FlightSample s{};
    s.speed_q = static_cast<uint16_t>(mps * 4);
    s.hdop_e2 = hdop_e2;
    s.fix_valid = true;
    return s;
}

// One second of solutions at a time, the cadence the receiver is configured for.
FlightState hold(FlightMonitor& monitor, int seconds, double mps, uint16_t hdop_e2 = 90) {
    FlightState state = monitor.state();
    for (int i = 0; i < seconds; i++) state = monitor.update(solution(mps, hdop_e2));
    return state;
}

}  // namespace

// 23 kt is a speed no aircraft taxis at and every takeoff roll passes it well before it flies.
TEST_CASE("flight: a takeoff is the speed no taxi holds, and it waits for nothing") {
    FlightMonitor monitor;
    REQUIRE(hold(monitor, 3, 0.0) == FlightState::OnGround);

    CHECK(hold(monitor, 1, 6.0) == FlightState::OnGround);   // a brisk taxi
    CHECK(hold(monitor, 1, 11.0) == FlightState::OnGround);  // the roll, accelerating
    CHECK(hold(monitor, 1, 12.0) == FlightState::Airborne);
}

// A glider towed to the grid, a tug taxiing back, a trailer on the perimeter track.
TEST_CASE("flight: a taxi does not take off, and one bad solution does not either") {
    FlightMonitor monitor;
    REQUIRE(hold(monitor, 3, 0.0) == FlightState::OnGround);
    // A tug hurrying back to the grid does 10 m/s, and it is what this threshold is set over.
    CHECK(hold(monitor, 30, 10.0) == FlightState::OnGround);

    CHECK(monitor.update(solution(45.0)) == FlightState::OnGround);
    CHECK(hold(monitor, 20, 10.0) == FlightState::OnGround);
}

// The rollout is the landing: a stop, and nothing slower than a stop.
TEST_CASE("flight: a landing is a standstill, and a fast rollout is not one yet") {
    FlightMonitor monitor;
    REQUIRE(hold(monitor, 3, 25.0) == FlightState::Airborne);

    CHECK(hold(monitor, 1, 20.0) == FlightState::Airborne);
    CHECK(hold(monitor, 1, 8.0) == FlightState::Airborne);
    CHECK(hold(monitor, 1, 2.0) == FlightState::Airborne);
    CHECK(hold(monitor, 1, 0.5) == FlightState::OnGround);
}

// A tug that lands and taxis straight back never stops, so its flight has not ended.
TEST_CASE("flight: rolling in from the runway without stopping is still the same flight") {
    FlightMonitor monitor;
    REQUIRE(hold(monitor, 3, 25.0) == FlightState::Airborne);

    CHECK(hold(monitor, 120, 5.0) == FlightState::Airborne);
    CHECK(hold(monitor, 1, 0.2) == FlightState::OnGround);
}

// Evidence is worth what the fix behind it is worth, so OGN divides by any DOP above 1.0.
TEST_CASE("flight: a fix nobody should trust does not take off on its own") {
    FlightMonitor monitor;
    REQUIRE(hold(monitor, 3, 0.0, 3000) == FlightState::OnGround);
    CHECK(hold(monitor, 20, 12.0, 3000) == FlightState::OnGround);

    // The same movement on a fix worth trusting is a takeoff.
    CHECK(hold(monitor, 1, 12.0, 90) == FlightState::Airborne);
}

// The derating cuts one way: a poor sky may refuse a takeoff, never declare a landing.
TEST_CASE("flight: the dilution of precision cannot land an aircraft") {
    CHECK_FALSE(flight_evidence(solution(12.0, 3000)));
    CHECK_FALSE(ground_evidence(solution(12.0, 3000)));
    CHECK(flight_evidence(solution(12.0, 90)));

    CHECK_FALSE(flight_evidence(solution(11.9)));
    CHECK(ground_evidence(solution(0.9)));
    CHECK_FALSE(ground_evidence(solution(1.2)));
    // A receiver that reports no DOP at all is not punished for it.
    CHECK(flight_evidence(solution(12.0, 0)));
}

// The third band: stopped or moving, which is the word the glass prints while ADS-L says OnGround.
TEST_CASE("flight: a parked receiver's own noise is not a taxi, and a taxi that slows is still one") {
    FlightMonitor monitor;
    REQUIRE(hold(monitor, 3, 0.2) == FlightState::OnGround);
    CHECK_FALSE(monitor.rolling());

    // A metre a second of multipath on a device nobody has touched.
    CHECK_FALSE(hold(monitor, 5, 1.25) == FlightState::Airborne);
    CHECK_FALSE(monitor.rolling());

    monitor.update(solution(2.0));  // pushed to the grid at a brisk walk
    CHECK(monitor.rolling());

    hold(monitor, 5, 1.25);  // slowing for the turn, and the word holds
    CHECK(monitor.rolling());

    hold(monitor, 2, 0.75);
    CHECK_FALSE(monitor.rolling());
}

// A taxi does not stop because the antenna did.
TEST_CASE("flight: an outage leaves the aircraft rolling as it was") {
    FlightMonitor monitor;
    REQUIRE(monitor.update(solution(6.0)) == FlightState::OnGround);
    REQUIRE(monitor.rolling());

    FlightSample lost{};
    CHECK(monitor.update(lost) == FlightState::Unknown);
    CHECK(monitor.rolling());
}

// The first solution decides on its own evidence: a device rebooted in flight must not wait.
TEST_CASE("flight: a device switched on in the air says so at once") {
    FlightMonitor airborne;
    CHECK(airborne.update(solution(30.0)) == FlightState::Airborne);

    // Switched on while being towed to the grid, which is the common half of this.
    FlightMonitor rolling;
    CHECK(rolling.update(solution(6.0)) == FlightState::OnGround);

    FlightMonitor parked;
    CHECK(parked.update(solution(0.4)) == FlightState::OnGround);
}

// Without a fix there is no claim to make, and the aircraft is still where it was.
TEST_CASE("flight: no fix is not a landing") {
    FlightMonitor monitor;
    REQUIRE(hold(monitor, 3, 30.0) == FlightState::Airborne);

    FlightSample lost{};
    CHECK(monitor.update(lost) == FlightState::Unknown);
    CHECK(monitor.state() == FlightState::Airborne);

    CHECK(monitor.update(solution(30.0)) == FlightState::Airborne);
}

// The jerk gate compares two consecutive solutions, so an outage must not arm it.
TEST_CASE("flight: the first solution after an outage is not judged as a jerk") {
    FlightMonitor monitor;
    REQUIRE(hold(monitor, 3, 0.0) == FlightState::OnGround);

    FlightSample lost{};
    REQUIRE(monitor.update(lost) == FlightState::Unknown);
    CHECK(monitor.update(solution(30.0)) == FlightState::Airborne);
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
