#ifndef SKYBLIP_HARDWARE_MODEL_BHI260_H
#define SKYBLIP_HARDWARE_MODEL_BHI260_H

#include <cstdint>
#include <cstring>

#include "hardware/io/io.h"

namespace skyblip::models {

class Bhi260 : public io::I2c {
   public:
    static constexpr uint8_t kAddress = 0x28;
    static constexpr uint16_t kFirmwareMagic = 0x662B;
    static constexpr uint16_t kKernelVersion = 0x1234;
    static constexpr int kMaxStream = 64;

    bool write(uint8_t addr, const uint8_t* data, size_t len) override {
        if (addr != address || !answers) return false;
        if (len == 0) return true;
        pointer = data[0];
        if (pointer == kRegCommand) return accept_command(data + 1, len - 1);
        for (size_t i = 1; i < len; i++) {
            const uint8_t reg = static_cast<uint8_t>(pointer + i - 1);
            if (reg == kRegResetRequest && data[i] != 0) reset();
        }
        return true;
    }

    bool read(uint8_t addr, uint8_t* data, size_t len) override {
        if (addr != address || !answers) return false;
        if (pointer == kRegFifoNonWakeup) return read_fifo(data, len);
        for (size_t i = 0; i < len; i++)
            data[i] = register_value(static_cast<uint8_t>(pointer + i));
        return true;
    }

    void set_acceleration(int16_t x, int16_t y, int16_t z) {
        x_mg = x;
        y_mg = y;
        z_mg = z;
    }

    void report_meta_event(uint8_t event, uint8_t first, uint8_t second) {
        meta_event_ = event;
        meta_first_ = first;
        meta_second_ = second;
        meta_pending_ = true;
    }

    bool running() const { return booted && sample_rate_hz > 0; }
    uint32_t uploaded() const { return uploaded_; }

    uint8_t address{kAddress};
    bool answers{true};
    bool accepts_firmware{true};
    uint8_t product_id{0x89};
    bool announces_itself{true};
    uint8_t error_value{0};
    bool host_interface_ready{true};
    bool booted{false};
    bool verify_error{false};
    uint8_t accel_sensor_id{0};
    uint16_t accel_range_g{0};
    float sample_rate_hz{0};
    uint32_t latency_ms{0};
    int16_t x_mg{0};
    int16_t y_mg{1000};
    int16_t z_mg{0};
    int resets{0};

   private:
    static constexpr uint8_t kRegCommand = 0x00;
    static constexpr uint8_t kRegFifoNonWakeup = 0x02;
    static constexpr uint8_t kRegResetRequest = 0x14;
    static constexpr uint8_t kRegProductId = 0x1C;
    static constexpr uint8_t kRegKernelVersion = 0x20;
    static constexpr uint8_t kRegBootStatus = 0x25;
    static constexpr uint8_t kRegErrorValue = 0x2E;
    static constexpr uint8_t kBootHostInterfaceReady = 0x10;
    static constexpr uint8_t kBootFirmwareVerifyDone = 0x20;
    static constexpr uint8_t kBootFirmwareVerifyError = 0x40;
    static constexpr uint16_t kCmdUploadToProgramRam = 0x0002;
    static constexpr uint16_t kCmdBootProgramRam = 0x0003;
    static constexpr uint16_t kCmdConfigureSensor = 0x000D;
    static constexpr uint16_t kCmdChangeRange = 0x000E;
    static constexpr uint8_t kSensorAccelerometer = 0x04;
    static constexpr uint8_t kSysIdTimestampSmallDelta = 251;
    static constexpr uint8_t kSysIdMetaEvent = 254;
    static constexpr uint8_t kMetaEventInitialised = 16;
    static constexpr int kCountsPerRange = 32768;

    void reset() {
        resets++;
        booted = false;
        uploaded_ = 0;
        magic_ = 0;
        command_ = 0;
        expected_ = 0;
        received_ = 0;
        payload_bytes_ = 0;
        sample_rate_hz = 0;
        announced_ = false;
        stream_len_ = 0;
        stream_pos_ = 0;
    }

    bool accept_command(const uint8_t* data, size_t len) {
        size_t pos = 0;
        while (pos < len) {
            if (expected_ == 0 && received_ == 0 && command_ == 0) {
                if (len - pos < 4) return false;
                command_ = static_cast<uint16_t>(data[pos] | (data[pos + 1] << 8));
                const uint16_t declared =
                    static_cast<uint16_t>(data[pos + 2] | (data[pos + 3] << 8));
                expected_ = command_ == kCmdUploadToProgramRam ? declared * 4u : declared;
                pos += 4;
                payload_bytes_ = 0;
                if (expected_ == 0) finish_command();
                continue;
            }
            const size_t take =
                len - pos < expected_ - received_ ? len - pos : expected_ - received_;
            for (size_t i = 0; i < take; i++) {
                if (command_ == kCmdUploadToProgramRam) {
                    if (received_ + i == 0) magic_ = data[pos + i];
                    if (received_ + i == 1)
                        magic_ = static_cast<uint16_t>(magic_ | (data[pos + i] << 8));
                } else if (payload_bytes_ < sizeof(payload_)) {
                    payload_[payload_bytes_++] = data[pos + i];
                }
            }
            received_ += take;
            pos += take;
            if (received_ >= expected_) finish_command();
        }
        return true;
    }

    void finish_command() {
        switch (command_) {
            case kCmdUploadToProgramRam: uploaded_ = received_; break;
            case kCmdBootProgramRam:
                booted = accepts_firmware && magic_ == kFirmwareMagic && uploaded_ > 0;
                verify_error = !booted;
                if (booted && announces_itself) report_meta_event(kMetaEventInitialised, 0, 0);
                break;
            case kCmdChangeRange:
                if (!announced_) break;
                if (payload_bytes_ >= 3 && payload_[0] == kSensorAccelerometer)
                    accel_range_g = static_cast<uint16_t>(payload_[1] | (payload_[2] << 8));
                break;
            case kCmdConfigureSensor:
                if (!announced_) break;
                if (payload_bytes_ >= 8) {
                    accel_sensor_id = payload_[0];
                    uint32_t bits = 0;
                    for (int i = 0; i < 4; i++)
                        bits |= static_cast<uint32_t>(payload_[1 + i]) << (8 * i);
                    float rate = 0;
                    std::memcpy(&rate, &bits, sizeof(rate));
                    sample_rate_hz = rate;
                    latency_ms = static_cast<uint32_t>(payload_[5] | (payload_[6] << 8) |
                                                       (payload_[7] << 16));
                }
                break;
            default: break;
        }
        command_ = 0;
        expected_ = 0;
        received_ = 0;
    }

    uint8_t register_value(uint8_t reg) const {
        switch (reg) {
            case kRegProductId: return product_id;
            case kRegKernelVersion: return booted ? (kKernelVersion & 0xFF) : 0;
            case kRegKernelVersion + 1: return booted ? (kKernelVersion >> 8) : 0;
            case kRegBootStatus: return boot_status();
            case kRegErrorValue: return error_value;
            default: return 0;
        }
    }

    uint8_t boot_status() const {
        uint8_t status = host_interface_ready ? kBootHostInterfaceReady : 0;
        if (booted) status |= kBootFirmwareVerifyDone;
        if (verify_error && !booted) status |= kBootFirmwareVerifyError;
        return status;
    }

    bool read_fifo(uint8_t* data, size_t len) {
        if (stream_pos_ >= stream_len_) fill_stream();
        for (size_t i = 0; i < len; i++)
            data[i] = stream_pos_ < stream_len_ ? stream_[stream_pos_++] : 0;
        return true;
    }

    void fill_stream() {
        stream_len_ = 0;
        stream_pos_ = 0;
        uint8_t events[16];
        int n = 0;
        if (meta_pending_) {
            events[n++] = kSysIdMetaEvent;
            events[n++] = meta_event_;
            events[n++] = meta_first_;
            events[n++] = meta_second_;
            meta_pending_ = false;
            if (meta_event_ == kMetaEventInitialised) announced_ = true;
        }
        if (running()) {
            events[n++] = kSysIdTimestampSmallDelta;
            events[n++] = 1;
            events[n++] = kSensorAccelerometer;
            n += put_counts(events + n, x_mg);
            n += put_counts(events + n, y_mg);
            n += put_counts(events + n, z_mg);
        }
        stream_[stream_len_++] = static_cast<uint8_t>(n & 0xFF);
        stream_[stream_len_++] = static_cast<uint8_t>(n >> 8);
        for (int i = 0; i < n; i++) stream_[stream_len_++] = events[i];
    }

    int put_counts(uint8_t* out, int16_t milli_g) const {
        const int range = accel_range_g == 0 ? 1 : accel_range_g;
        const int32_t counts = static_cast<int32_t>(milli_g) * kCountsPerRange / (range * 1000);
        out[0] = static_cast<uint8_t>(counts & 0xFF);
        out[1] = static_cast<uint8_t>((counts >> 8) & 0xFF);
        return 2;
    }

    uint8_t pointer{0};
    uint16_t command_{0};
    uint32_t expected_{0};
    uint32_t received_{0};
    uint32_t uploaded_{0};
    uint16_t magic_{0};
    uint8_t payload_[8]{};
    size_t payload_bytes_{0};
    uint8_t meta_event_{0};
    uint8_t meta_first_{0};
    uint8_t meta_second_{0};
    bool meta_pending_{false};
    bool announced_{false};
    uint8_t stream_[kMaxStream]{};
    size_t stream_len_{0};
    size_t stream_pos_{0};
};

}  // namespace skyblip::models

#endif
