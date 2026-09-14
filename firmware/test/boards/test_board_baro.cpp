// When the board reads the barometer: that interval is what a vertical speed is taken over.
#include <vector>

#include "boards/lilygo/t_echo_plus/board.h"
#include "core/bus/bus.h"
#include "core/bus/state.h"
#include "doctest/doctest.h"
#include "hardware/platform/host/platform.h"
#include "runtime/tasks.h"

using namespace skyblip;

namespace {

using Board = boards::TEchoPlus<platform::host::Platform>;

std::vector<uint32_t> sample_instants(platform::host::Platform& platform, Board& board,
                                      bus::Bus& bus, bus::State& state, uint32_t from_ms,
                                      uint32_t to_ms) {
    std::vector<uint32_t> at;
    for (uint32_t t = from_ms; t < to_ms; t += runtime::kServiceStepMs) {
        platform.clock().set_millis(t);
        board.poll(state, t);
        messages::BaroSample sample{};
        while (bus.baro.pop(sample)) at.push_back(sample.at_ms);
    }
    return at;
}

}  // namespace

TEST_CASE("board: the barometer is read once a second, on the PPS edge") {
    platform::host::Platform platform;
    bus::Bus bus;
    Board board{platform, bus};
    bus::State state;

    const std::vector<uint32_t> at = sample_instants(platform, board, bus, state, 0, 10000);

    REQUIRE(at.size() == 9);
    CHECK(at.front() == 1000);
    for (size_t i = 0; i < at.size(); i++) {
        CHECK(at[i] % 1000 < runtime::kBaroPpsWindowMs);
        if (i > 0) CHECK(at[i] - at[i - 1] == runtime::kBaroPeriodMs);
    }
}

TEST_CASE("board: with no PPS the barometer keeps its own second") {
    platform::host::Platform platform;
    platform.pps().set_locked(false);
    bus::Bus bus;
    Board board{platform, bus};
    bus::State state;

    const std::vector<uint32_t> at = sample_instants(platform, board, bus, state, 55, 10055);

    REQUIRE(at.size() >= 9);
    for (size_t i = 1; i < at.size(); i++) CHECK(at[i] - at[i - 1] == runtime::kBaroPeriodMs);
}

TEST_CASE("board: a sample the sensor refused is not a sample of the last pressure") {
    platform::host::Platform platform;
    platform.baro().present = false;
    bus::Bus bus;
    Board board{platform, bus};
    bus::State state;

    CHECK(sample_instants(platform, board, bus, state, 0, 5000).empty());
}
