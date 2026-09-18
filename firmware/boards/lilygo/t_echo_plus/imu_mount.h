#ifndef SKYBLIP_BOARDS_T_ECHO_PLUS_IMU_MOUNT_H
#define SKYBLIP_BOARDS_T_ECHO_PLUS_IMU_MOUNT_H

#include <cstdint>

#include "core/events/sensor.h"
#include "hardware/parts/bhi260/bhi260.h"

namespace skyblip::boards::t_echo_plus {

// INFO: fc 18sep26 the BHI260AP's rotation in the case is unmeasured, a bench corrects it here
inline events::AccelSample device_frame(const parts::Acceleration& chip, uint32_t now_ms) {
    return events::AccelSample{chip.x_mg, chip.y_mg, chip.z_mg, now_ms};
}

}  // namespace skyblip::boards::t_echo_plus

#endif
