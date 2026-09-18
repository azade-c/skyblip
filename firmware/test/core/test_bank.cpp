// Bank from the inertial sensors, which a gravity vector alone cannot give in a turn.
#include "core/flight/bank.h"
#include "doctest/doctest.h"

using namespace skyblip;
using flight::BodyRate;
using flight::SpecificForce;

namespace {

constexpr int32_t kHundredKnotsMps = 51;
constexpr int16_t kStandardRateCdps = 300;

int16_t settled_bank(int32_t speed_mps, int16_t turn_cdps, const SpecificForce& force) {
    flight::BankAngle bank;
    uint32_t t = 0;
    for (int i = 0; i < 200; i++) {
        t += 200;
        bank.observe(BodyRate{}, force, speed_mps, turn_cdps, t);
    }
    CHECK(bank.valid(t));
    return bank.deg();
}

}  // namespace

TEST_CASE("bank: the turn's own force is what an accelerometer cannot tell from gravity") {
    CHECK(flight::centripetal_mg(kHundredKnotsMps, kStandardRateCdps) ==
          doctest::Approx(272).epsilon(0.02));
    CHECK(flight::centripetal_mg(kHundredKnotsMps, -kStandardRateCdps) ==
          doctest::Approx(-272).epsilon(0.02));
    CHECK(flight::centripetal_mg(0, kStandardRateCdps) == 0);
}

TEST_CASE("bank: standing still, the bank is where gravity says it is") {
    CHECK(settled_bank(0, 0, SpecificForce{0, 1000, 0}) == 0);
    CHECK(settled_bank(0, 0, SpecificForce{-500, 866, 0}) == doctest::Approx(30).epsilon(0.05));
    CHECK(settled_bank(0, 0, SpecificForce{500, 866, 0}) == doctest::Approx(-30).epsilon(0.05));
}

// The leans: in a coordinated turn the accelerometer alone reads wings level at any bank.
TEST_CASE("bank: a coordinated turn reads its real bank, not the level the ball reads") {
    const SpecificForce coordinated{0, 1035, 0};
    CHECK(settled_bank(kHundredKnotsMps, kStandardRateCdps, coordinated) ==
          doctest::Approx(15).epsilon(0.15));
    CHECK(settled_bank(kHundredKnotsMps, -kStandardRateCdps, coordinated) ==
          doctest::Approx(-15).epsilon(0.15));
}

TEST_CASE("bank: a roll is on the glass before the accelerometer has agreed to it") {
    flight::BankAngle bank;
    const SpecificForce level{0, 1000, 0};
    bank.observe(BodyRate{}, level, 0, 0, 1000);
    REQUIRE(bank.deg() == 0);

    for (int i = 0; i < 5; i++) bank.observe(BodyRate{1000, 0, 0}, level, 0, 0, 1200 + i * 200u);

    CHECK(bank.deg() > 3);
    CHECK(bank.deg() < 10);
}

TEST_CASE("bank: nothing measured is not a bank of zero") {
    flight::BankAngle bank;
    CHECK_FALSE(bank.valid(0));
    bank.observe(BodyRate{}, SpecificForce{0, 1000, 0}, 0, 0, 1000);
    CHECK(bank.valid(1000));
    CHECK_FALSE(bank.valid(1000 + flight::kBankStaleMs));
}
