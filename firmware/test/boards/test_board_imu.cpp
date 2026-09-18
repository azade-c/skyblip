// The sensor hub on the board: probed, booted while the device flies, then read.
#include "boards/lilygo/t_echo_plus/board.h"
#include "core/bus/bus.h"
#include "core/bus/state.h"
#include "core/events/sensor.h"
#include "doctest/doctest.h"
#include "hardware/platform/host/platform.h"
#include "runtime/tasks.h"

using namespace skyblip;

namespace {

using Board = boards::TEchoPlus<platform::host::Platform>;

uint32_t run(platform::host::Platform& platform, Board& board, bus::State& state, uint32_t from_ms,
             uint32_t for_ms) {
    uint32_t t = from_ms;
    for (; t < from_ms + for_ms; t += runtime::kServiceStepMs) {
        platform.clock().set_millis(t);
        board.poll(state, t);
    }
    return t;
}

bool last_sample(bus::Bus& bus, events::AccelSample& out) {
    bool any = false;
    events::AccelSample sample{};
    while (bus.accel.pop(sample)) {
        out = sample;
        any = true;
    }
    return any;
}

bool last_rate(bus::Bus& bus, events::RateSample& out) {
    bool any = false;
    events::RateSample sample{};
    while (bus.rate.pop(sample)) {
        out = sample;
        any = true;
    }
    return any;
}

}  // namespace

TEST_CASE("board: a sensor hub that answers and names itself is an inclinometer") {
    platform::host::Platform platform;
    bus::Bus bus;
    Board board{platform, bus};

    CHECK(ports::has(board.capabilities(), ports::Capability::Inclinometer));
    CHECK(board.inventory().has_i2c_address(boards::t_echo_plus::kImuAddress));
}

TEST_CASE("board: an address that answers and is not a BHI260 is no inclinometer") {
    platform::host::Platform platform;
    platform.chips().imu.product_id = 0x00;
    bus::Bus bus;
    Board board{platform, bus};

    CHECK_FALSE(ports::has(board.capabilities(), ports::Capability::Inclinometer));
    CHECK(board.inventory().has_i2c_address(boards::t_echo_plus::kImuAddress));
}

TEST_CASE("board: a plain T-Echo has nothing at 0x28 and no inclinometer") {
    platform::host::Platform platform;
    platform.i2c_bus().answer(boards::t_echo_plus::kImuAddress, false);
    bus::Bus bus;
    Board board{platform, bus};

    CHECK_FALSE(ports::has(board.capabilities(), ports::Capability::Inclinometer));
    CHECK_FALSE(board.inventory().has_i2c_address(boards::t_echo_plus::kImuAddress));
}

// The chip is a quarter turn from the case and face down with it: +X is up, +Y right, +Z forward.
TEST_CASE("board: the chip's axes are turned into the case's before anything reads them") {
    platform::host::Platform platform;
    platform.chips().imu.set_acceleration(990, -150, -20);
    bus::Bus bus;
    Board board{platform, bus};
    bus::State state;

    run(platform, board, state, 0, 2000);

    events::AccelSample sample{};
    REQUIRE(last_sample(bus, sample));
    CHECK(sample.right_mg == doctest::Approx(-150).epsilon(0.02));
    CHECK(sample.up_mg == doctest::Approx(990).epsilon(0.02));
    CHECK(sample.aft_mg == doctest::Approx(20).epsilon(0.1));
}

TEST_CASE("board: the hub is booted from the loop, and reports once it runs") {
    platform::host::Platform platform;
    platform.chips().imu.set_acceleration(990, -150, -20);
    bus::Bus bus;
    Board board{platform, bus};
    bus::State state;

    const uint32_t t = run(platform, board, state, 0, 2000);
    CHECK(board.imu().stage() == parts::Bhi260::Stage::Running);
    CHECK(platform.chips().imu.booted);

    events::AccelSample sample{};
    REQUIRE(last_sample(bus, sample));
    CHECK(sample.right_mg == doctest::Approx(-150).epsilon(0.02));
    CHECK(sample.up_mg == doctest::Approx(990).epsilon(0.02));
    CHECK(sample.aft_mg == doctest::Approx(20).epsilon(0.1));
    CHECK(sample.at_ms < t);
}

// Roll about the nose, pitch about the right wing, yaw about the mast, positive the way it moves.
TEST_CASE("board: the chip's rotation rates reach the bus as body rates") {
    platform::host::Platform platform;
    platform.chips().imu.set_angular_rate(-700, 300, 500);
    bus::Bus bus;
    Board board{platform, bus};
    bus::State state;

    run(platform, board, state, 0, 2000);

    events::RateSample rate{};
    REQUIRE(last_rate(bus, rate));
    CHECK(rate.yaw_cdps == doctest::Approx(700).epsilon(0.02));
    CHECK(rate.pitch_cdps == doctest::Approx(300).epsilon(0.02));
    CHECK(rate.roll_cdps == doctest::Approx(500).epsilon(0.02));
}

TEST_CASE("board: nothing is asked of a bus with no hub on it") {
    platform::host::Platform platform;
    platform.i2c_bus().answer(boards::t_echo_plus::kImuAddress, false);
    bus::Bus bus;
    Board board{platform, bus};
    bus::State state;

    run(platform, board, state, 0, 2000);

    CHECK(board.imu().stage() == parts::Bhi260::Stage::Absent);
    events::AccelSample sample{};
    CHECK_FALSE(last_sample(bus, sample));
}
