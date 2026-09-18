#ifndef SKYBLIP_HARDWARE_PARTS_BHI260_H
#define SKYBLIP_HARDWARE_PARTS_BHI260_H

#include <cstdint>

#include "core/util/result.h"
#include "core/util/span.h"
#include "hardware/io/io.h"

namespace skyblip::parts {

struct Acceleration {
    int16_t x_mg{0};
    int16_t y_mg{0};
    int16_t z_mg{0};
    uint32_t at_ms{0};
};

class Bhi260 {
   public:
    static constexpr uint8_t kAddress = 0x28;
    static constexpr uint8_t kAddressAlternate = 0x29;
    static constexpr uint8_t kProductId = 0x89;

    static constexpr int32_t kRangeG = 4;
    static constexpr uint32_t kSampleRateBits = 0x41480000;
    static constexpr uint32_t kSamplePeriodMs = 200;
    static constexpr uint16_t kUploadChunkBytes = 240;
    static constexpr uint16_t kFifoReadBytes = 64;

    static constexpr uint32_t kResetSettleMs = 100;
    static constexpr uint32_t kStatusPollMs = 50;
    static constexpr uint32_t kHostInterfaceTimeoutMs = 2000;
    static constexpr uint32_t kBootTimeoutMs = 5000;
    static constexpr uint32_t kInitialisedTimeoutMs = 2000;

    enum class Stage : uint8_t {
        Absent,
        Idle,
        Resetting,
        HostInterface,
        Uploading,
        Booting,
        Initialising,
        Configuring,
        Running,
        Failed,
    };

    explicit Bhi260(io::I2c& bus) : bus_(bus) {}

    Status probe();
    void load(ConstByteSpan image, uint32_t now_ms);
    void service(uint32_t now_ms);
    bool poll();

    const Acceleration& acceleration() const { return sample_; }
    Stage stage() const { return stage_; }
    Status fault() const { return fault_; }
    const char* stage_text() const;
    const char* fault_text() const;
    bool running() const { return stage_ == Stage::Running; }
    uint8_t address() const { return address_; }
    uint16_t kernel_version() const { return kernel_version_; }
    uint32_t uploaded_bytes() const { return uploaded_; }
    uint32_t unparsed_events() const { return unparsed_; }
    uint32_t fifo_bytes() const { return fifo_bytes_; }
    uint8_t hub_error() const { return error_; }
    uint8_t meta_event() const { return meta_event_; }
    uint8_t sensor_error() const { return sensor_error_; }
    uint8_t errored_sensor() const { return errored_sensor_; }

   private:
    static constexpr uint8_t kRegCommand = 0x00;
    static constexpr uint8_t kRegFifoNonWakeup = 0x02;
    static constexpr uint8_t kRegChipControl = 0x05;
    static constexpr uint8_t kRegHostInterfaceControl = 0x06;
    static constexpr uint8_t kRegResetRequest = 0x14;
    static constexpr uint8_t kRegProductId = 0x1C;
    static constexpr uint8_t kRegKernelVersion = 0x20;
    static constexpr uint8_t kRegBootStatus = 0x25;
    static constexpr uint8_t kRegErrorValue = 0x2E;

    static constexpr uint16_t kCmdUploadToProgramRam = 0x0002;
    static constexpr uint16_t kCmdBootProgramRam = 0x0003;
    static constexpr uint16_t kCmdConfigureSensor = 0x000D;
    static constexpr uint16_t kCmdChangeRange = 0x000E;

    static constexpr uint8_t kBootHostInterfaceReady = 0x10;
    static constexpr uint8_t kBootFirmwareVerifyDone = 0x20;
    static constexpr uint8_t kBootFirmwareVerifyError = 0x40;

    static constexpr uint16_t kFirmwareMagic = 0x662B;
    static constexpr uint8_t kErrorHostChannelEmpty = 0x77;
    static constexpr uint8_t kSensorAccelerometer = 0x04;
    static constexpr uint8_t kAccelEventBytes = 7;
    static constexpr int32_t kCountsPerRange = 32768;
    static constexpr uint16_t kCommandHeaderBytes = 4;

    bool write_registers(uint8_t reg, const uint8_t* data, uint16_t len);
    bool read_registers(uint8_t reg, uint8_t* out, uint16_t len);
    bool command(uint16_t cmd, const uint8_t* payload, uint16_t len);
    bool boot_status(uint8_t& out);
    void fail(Status why);

    void step_reset(uint32_t now_ms);
    void step_host_interface(uint32_t now_ms);
    void step_upload(uint32_t now_ms);
    void step_boot(uint32_t now_ms);
    void step_initialise(uint32_t now_ms);
    void step_configure(uint32_t now_ms);
    void step_running(uint32_t now_ms);

    bool configure_accelerometer();
    void read_hub_error();
    void drain_fifo(uint32_t now_ms);
    uint16_t parse_fifo(const uint8_t* data, uint16_t len, uint32_t now_ms);
    void note_meta_event(const uint8_t* event);
    static uint8_t event_bytes(uint8_t id);
    static int16_t to_milli_g(const uint8_t* le16);

    io::I2c& bus_;
    ConstByteSpan image_{};
    Stage stage_{Stage::Absent};
    Status fault_{Status::Ok};
    uint8_t address_{kAddress};
    Stage failed_stage_{Stage::Absent};
    uint32_t since_ms_{0};
    uint32_t polled_ms_{0};
    uint32_t uploaded_{0};
    uint32_t unparsed_{0};
    uint32_t fifo_bytes_{0};
    uint8_t error_{0};
    uint8_t meta_event_{0};
    uint8_t sensor_error_{0};
    uint8_t errored_sensor_{0};
    uint16_t fifo_remaining_{0};
    uint16_t kernel_version_{0};
    Acceleration sample_{};
    bool fresh_{false};
    bool resync_{false};
    uint8_t frame_[1 + kCommandHeaderBytes + kUploadChunkBytes]{};
    uint8_t fifo_[kFifoReadBytes]{};
    uint16_t carried_{0};
};

}  // namespace skyblip::parts

#endif
