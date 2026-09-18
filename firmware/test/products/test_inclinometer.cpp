// The ball, end to end: a hub on the bus, a rudder mistake, ink on the glass.
#include "doctest/doctest.h"
#include "products/skyblip_go/pages/sixpack.h"
#include "test/support/glass_text.h"
#include "test/support/product_rig.h"

using namespace skyblip;

namespace {

constexpr int kTurnCoordinatorCx = 34;
constexpr int kTurnCoordinatorCy = 133;
constexpr int kBallY = kTurnCoordinatorCy + 18;
constexpr uint32_t kBootToBall = 4000;

// The chip is a quarter turn from the case (imu_mount.h): its +X is down, its +Y is right.
void fly_uncoordinated(Rig& rig, int16_t right_mg) {
    rig.platform.chips().imu.set_acceleration(-1000, right_mg, 0);
}

bool ink_between(const go::Glass& fb, int from_x, int to_x, int y) {
    for (int x = from_x; x <= to_x; x++)
        if (fb.get_pixel(x, y)) return true;
    return false;
}

}  // namespace

TEST_CASE("inclinometer: a hub that boots gives the six-pack its cage and its ball") {
    Rig rig;
    REQUIRE(rig.setup() == Status::Ok);
    fly_uncoordinated(rig, 0);
    rig.run(0, kBootToBall);

    CHECK(rig.product.board().imu().stage() == parts::Bhi260::Stage::Running);
    CHECK(rig.state().slip.valid);
    CHECK(rig.state().slip.lateral_mg == 0);
}

TEST_CASE("inclinometer: the ball goes the way the pilot slides, and the rudder follows") {
    Rig rig;
    REQUIRE(rig.setup() == Status::Ok);
    fly_uncoordinated(rig, -150);
    rig.run(0, kBootToBall);

    CHECK(rig.state().slip.valid);
    CHECK(rig.state().slip.lateral_mg > 0);

    go::SixPackSnapshot snap;
    snap.inclinometer_fitted = true;
    snap.lateral_valid = rig.state().slip.valid;
    snap.lateral_mg = rig.state().slip.lateral_mg;
    go::Glass fb;
    go::draw_sixpack(fb, snap);

    CHECK(ink_between(fb, kTurnCoordinatorCx + 1, kTurnCoordinatorCx + 20, kBallY));
    CHECK_FALSE(ink_between(fb, kTurnCoordinatorCx - 20, kTurnCoordinatorCx - 8, kBallY));
}

TEST_CASE("inclinometer: a hub that stops answering takes the ball off the glass") {
    Rig rig;
    REQUIRE(rig.setup() == Status::Ok);
    fly_uncoordinated(rig, -150);
    rig.run(0, kBootToBall);
    REQUIRE(rig.state().slip.valid);

    rig.platform.chips().imu.answers = false;
    rig.run(kBootToBall, kBootToBall + flight::kSlipStaleMs + 1000);

    CHECK(rig.product.board().imu().stage() == parts::Bhi260::Stage::Failed);
    CHECK_FALSE(rig.state().slip.valid);
}

TEST_CASE("inclinometer: the status page names the stage the hub stopped in") {
    Rig rig;
    REQUIRE(rig.setup() == Status::Ok);
    fly_uncoordinated(rig, -150);
    rig.run(0, kBootToBall);

    uint32_t t = kBootToBall;
    rig.tap_pad(t);
    rig.tap_pad(t);
    REQUIRE(rig.product.screen().page() == go::Page::Status);
    rig.run(t, t + 1000);
    CHECK(reads_in(rig.product.screen().framebuffer(), "IMU RUN", 0, 55, 200, 70));

    rig.platform.chips().imu.answers = false;
    rig.run(t + 1000, t + 1000 + flight::kSlipStaleMs + 1000);
    CHECK(reads_in(rig.product.screen().framebuffer(), "IMU RUN DOWN", 0, 55, 200, 70));
}

TEST_CASE("inclinometer: a unit with no hub keeps the dial empty rather than centred") {
    constexpr ports::Capabilities kNoImu = static_cast<ports::Capabilities>(
        static_cast<uint32_t>(platform::host::Platform::kFullyFitted) &
        ~static_cast<uint32_t>(ports::Capability::Inclinometer));
    Rig rig(kNoImu);
    REQUIRE(rig.setup() == Status::Ok);
    rig.run(0, kBootToBall);

    CHECK_FALSE(ports::has(rig.product.capabilities(), ports::Capability::Inclinometer));
    CHECK_FALSE(rig.state().slip.valid);

    go::SixPackSnapshot snap;
    go::Glass fb;
    go::draw_sixpack(fb, snap);
    CHECK_FALSE(ink_between(fb, kTurnCoordinatorCx - 20, kTurnCoordinatorCx + 20, kBallY));
}
