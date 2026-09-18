// Rate of turn from a gyroscope: the rotation about the earth's vertical.
#include "core/flight/rate.h"
#include "doctest/doctest.h"

using namespace skyblip;
using flight::BodyRate;
using flight::SpecificForce;

namespace {

constexpr int16_t kLevel = 1000;
constexpr SpecificForce level{0, kLevel, 0};

int16_t vertical(const BodyRate& rate, const SpecificForce& force = level) {
    int16_t out = 0;
    CHECK(flight::vertical_rate_cdps(rate, force, out));
    return out;
}

}  // namespace

TEST_CASE("rate: wings level, the turn is the yaw rate and nothing else") {
    CHECK(vertical({0, 0, 300}) == 300);
    CHECK(vertical({0, 0, -300}) == -300);
    CHECK(vertical({400, 0, 0}) == 0);
    CHECK(vertical({0, 400, 0}) == 0);
}

// Banked 90 degrees right, the pitch axis points at the ground: the nose-up pull is the turn.
TEST_CASE("rate: banked over, the yaw rate alone is no longer the rate of turn") {
    const SpecificForce knife_edge{-kLevel, 0, 0};
    CHECK(vertical({300, 0, 0}, knife_edge) == 0);
    CHECK(vertical({0, 300, 0}, knife_edge) == 300);
    CHECK(vertical({0, 0, 300}, knife_edge) == 0);
}

TEST_CASE("rate: free fall says nothing about which way the turn is") {
    int16_t out = 0;
    CHECK_FALSE(flight::vertical_rate_cdps({0, 0, 300}, SpecificForce{0, 0, 0}, out));
}

TEST_CASE("rate: a turn rate is what the instrument reads until a zero is learned") {
    flight::TurnRate turn;
    CHECK_FALSE(turn.valid(0));

    turn.observe({0, 0, 300}, level, 1000);
    CHECK(turn.valid(1000));
    CHECK(turn.cdps() == 300);
    CHECK_FALSE(turn.valid(1000 + flight::kRateStaleMs));
}

// A gyroscope standing still does not read zero, and that offset is in every reading after it.
TEST_CASE("rate: a zero learned while stopped comes off every turn afterwards") {
    flight::TurnRate turn;
    uint32_t t = 0;
    for (int i = 0; i < 400; i++) {
        t += 200;
        turn.observe({0, 0, 40}, level, t);
        turn.trim_to(0);
    }

    CHECK(turn.trim_cdps() == doctest::Approx(40).epsilon(0.05));
    CHECK(turn.cdps() == doctest::Approx(0).epsilon(0.05));

    turn.observe({0, 0, 340}, level, t + 200);
    CHECK(turn.cdps() == doctest::Approx(300).epsilon(0.05));
}

// The track rate is the same turn a second late: it cannot sharpen the instrument, only zero it.
TEST_CASE("rate: a gyroscope that reads nothing is dragged onto the track rate") {
    flight::TurnRate turn;
    uint32_t t = 0;
    for (int i = 0; i < 400; i++) {
        t += 200;
        turn.observe({0, 0, 0}, level, t);
        turn.trim_to(300);
    }

    CHECK(turn.cdps() == doctest::Approx(300).epsilon(0.05));
}

TEST_CASE("rate: a gyroscope that agrees with the track keeps its zero where it is") {
    flight::TurnRate turn;
    uint32_t t = 0;
    for (int i = 0; i < 400; i++) {
        t += 200;
        turn.observe({0, 0, 300}, level, t);
        turn.trim_to(300);
    }

    CHECK(turn.trim_cdps() == 0);
    CHECK(turn.cdps() == 300);
}
