#ifndef SKYBLIP_HARDWARE_PLATFORM_ZEPHYR_IMU_FIRMWARE_H
#define SKYBLIP_HARDWARE_PLATFORM_ZEPHYR_IMU_FIRMWARE_H

#include <cstddef>
#include <cstdint>

namespace skyblip::platform::zephyr {

extern const uint8_t kImuFirmware[];
extern const size_t kImuFirmwareBytes;

}  // namespace skyblip::platform::zephyr

#endif
