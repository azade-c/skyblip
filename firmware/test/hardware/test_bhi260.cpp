// The sensor hub, against its model: no program of its own until the host uploads one.
#include "doctest/doctest.h"
#include "hardware/parts/bhi260/bhi260.h"
#include "hardware/parts/bhi260/model.h"

using namespace skyblip;

namespace {

using Stage = parts::Bhi260::Stage;

constexpr uint8_t kImage[] = {0x2B, 0x66, 0x00, 0x00, 0x11, 0x22, 0x33, 0x44};

ConstByteSpan image() { return ConstByteSpan(kImage); }

uint32_t bring_up(parts::Bhi260& imu, uint32_t from_ms = 0) {
    uint32_t now_ms = from_ms;
    imu.load(image(), now_ms);
    while (imu.stage() != Stage::Running && imu.stage() != Stage::Failed &&
           now_ms < from_ms + 60000) {
        now_ms += 10;
        imu.service(now_ms);
    }
    return now_ms;
}

}  // namespace

TEST_CASE("bhi260: the part is present when it answers and names itself") {
    models::Bhi260 chip;
    parts::Bhi260 imu{chip};

    CHECK(imu.probe() == Status::Ok);
    CHECK(imu.stage() == Stage::Idle);
    CHECK(int(imu.address()) == 0x28);
}

TEST_CASE("bhi260: something else at 0x28 is not a sensor hub") {
    models::Bhi260 chip;
    chip.product_id = 0x00;
    parts::Bhi260 imu{chip};

    CHECK(imu.probe() == Status::Unsupported);
    CHECK(imu.stage() == Stage::Absent);
}

TEST_CASE("bhi260: a bus nobody answers is a part that is absent") {
    models::Bhi260 chip;
    chip.answers = false;
    parts::Bhi260 imu{chip};

    CHECK(imu.probe() == Status::Down);
    CHECK(imu.stage() == Stage::Absent);
}

TEST_CASE("bhi260: bring-up resets the part, uploads the image and boots it from RAM") {
    models::Bhi260 chip;
    parts::Bhi260 imu{chip};
    REQUIRE(imu.probe() == Status::Ok);

    bring_up(imu);

    CHECK(imu.stage() == Stage::Running);
    CHECK(chip.resets == 1);
    CHECK(chip.booted);
    CHECK(chip.uploaded() == sizeof(kImage));
    CHECK(imu.uploaded_bytes() == sizeof(kImage));
    CHECK(imu.kernel_version() == models::Bhi260::kKernelVersion);
}

TEST_CASE("bhi260: the accelerometer is configured for the range and rate the ball is read at") {
    models::Bhi260 chip;
    parts::Bhi260 imu{chip};
    REQUIRE(imu.probe() == Status::Ok);

    bring_up(imu);

    CHECK(int(chip.accel_sensor_id) == 4);
    CHECK(int(chip.accel_range_g) == parts::Bhi260::kRangeG);
    CHECK(chip.sample_rate_hz == doctest::Approx(12.5));
    CHECK(chip.latency_ms == 0);
}

TEST_CASE("bhi260: an image with no Bosch magic is refused before the bus is touched") {
    models::Bhi260 chip;
    parts::Bhi260 imu{chip};
    REQUIRE(imu.probe() == Status::Ok);

    const uint8_t rubbish[] = {0x00, 0x00, 0x00, 0x00};
    imu.load(ConstByteSpan(rubbish), 0);

    CHECK(imu.stage() == Stage::Failed);
    CHECK(imu.fault() == Status::Invalid);
    CHECK(chip.resets == 0);
}

TEST_CASE("bhi260: firmware the part refuses to verify leaves it failed, not running") {
    models::Bhi260 chip;
    chip.accepts_firmware = false;
    parts::Bhi260 imu{chip};
    REQUIRE(imu.probe() == Status::Ok);

    bring_up(imu);

    CHECK(imu.stage() == Stage::Failed);
    CHECK(imu.fault() == Status::Crc);
}

TEST_CASE("bhi260: a host interface that never comes ready times out rather than hanging") {
    models::Bhi260 chip;
    chip.host_interface_ready = false;
    parts::Bhi260 imu{chip};
    REQUIRE(imu.probe() == Status::Ok);

    bring_up(imu);

    CHECK(imu.stage() == Stage::Failed);
    CHECK(imu.fault() == Status::Timeout);
}

TEST_CASE("bhi260: the upload is paced, so no single pass of the loop owns the bus") {
    static uint8_t big[1024];
    big[0] = 0x2B;
    big[1] = 0x66;
    models::Bhi260 chip;
    parts::Bhi260 imu{chip};
    REQUIRE(imu.probe() == Status::Ok);

    imu.load(ConstByteSpan(big, sizeof(big)), 0);
    uint32_t now_ms = 0;
    int passes_while_uploading = 0;
    while (imu.stage() != Stage::Running && now_ms < 60000) {
        now_ms += 10;
        if (imu.stage() == Stage::Uploading) passes_while_uploading++;
        imu.service(now_ms);
    }

    CHECK(imu.stage() == Stage::Running);
    CHECK(chip.uploaded() == sizeof(big));
    // 1024 bytes at 240 a pass, the first one short by the command header.
    CHECK(passes_while_uploading == 5);
}

TEST_CASE("bhi260: a running part reports acceleration in milli-g") {
    models::Bhi260 chip;
    parts::Bhi260 imu{chip};
    REQUIRE(imu.probe() == Status::Ok);
    uint32_t now_ms = bring_up(imu);
    REQUIRE(imu.stage() == Stage::Running);

    chip.set_acceleration(-120, 1000, 30);
    CHECK_FALSE(imu.poll());

    now_ms += parts::Bhi260::kSamplePeriodMs;
    imu.service(now_ms);

    REQUIRE(imu.poll());
    CHECK(imu.acceleration().x_mg == doctest::Approx(-120).epsilon(0.02));
    CHECK(imu.acceleration().y_mg == doctest::Approx(1000).epsilon(0.02));
    CHECK(imu.acceleration().z_mg == doctest::Approx(30).epsilon(0.05));
    CHECK(imu.acceleration().at_ms == now_ms);
    CHECK_FALSE(imu.poll());
}

TEST_CASE("bhi260: a part that stops answering stops reporting") {
    models::Bhi260 chip;
    parts::Bhi260 imu{chip};
    REQUIRE(imu.probe() == Status::Ok);
    uint32_t now_ms = bring_up(imu);
    REQUIRE(imu.stage() == Stage::Running);

    chip.answers = false;
    now_ms += parts::Bhi260::kSamplePeriodMs;
    imu.service(now_ms);

    CHECK(imu.stage() == Stage::Failed);
    CHECK(imu.fault() == Status::Down);
    CHECK_FALSE(imu.poll());
}
