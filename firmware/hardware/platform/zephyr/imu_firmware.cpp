#include "hardware/platform/zephyr/imu_firmware.h"

namespace skyblip::platform::zephyr {

const uint8_t kImuFirmware[] = {
#include "bhi260ap_fw.inc"
};

const size_t kImuFirmwareBytes = sizeof(kImuFirmware);

}  // namespace skyblip::platform::zephyr
