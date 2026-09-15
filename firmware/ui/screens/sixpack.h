#ifndef SKYBLIP_UI_SCREENS_SIXPACK_H
#define SKYBLIP_UI_SCREENS_SIXPACK_H

#include <cstdint>

#include "core/settings/settings.h"
#include "ui/framebuffer.h"

namespace skyblip::ui {

struct SixPackSnapshot {
    bool data_valid{false};
    settings::Units units{settings::Units::Nautical};
    int32_t speed_kt{0};
    int32_t alt_ft{0};
    int32_t vs_fpm{0};
    uint16_t track_deg{0};
    int16_t turn_dps{0};  // degrees per second, positive = right
    uint32_t flight_seconds{0};
    bool flight_time_valid{false};
    bool airborne{false};
    int16_t lateral_mg{0};  // right-positive, thousandths of g across the wings
    bool inclinometer_fitted{false};
    bool lateral_valid{false};
};

void draw_sixpack(Framebuffer& fb, const SixPackSnapshot& snap);

}  // namespace skyblip::ui

#endif
