#ifndef SKYBLIP_UI_SCREENS_RADAR_H
#define SKYBLIP_UI_SCREENS_RADAR_H

#include <cstdint>

#include "core/settings/settings.h"
#include "core/traffic/alarm.h"
#include "core/units/units.h"
#include "ui/framebuffer.h"

namespace skyblip::ui {

using skyblip::kMetresPerNm;
constexpr int32_t kDefaultRangeNm = 4;
constexpr int kMaxRadarTargets = 12;

struct RadarTarget {
    int32_t north_m;
    int32_t east_m;
    int32_t up_m;
    traffic::Level alarm_level{traffic::Level::None};
    int16_t climb_e8{0};
    bool climb_valid{false};
    int32_t speed_mps{0};
    uint16_t track_deg{0};
};

struct RadarSnapshot {
    bool fix_valid{false};
    settings::Units units{settings::Units::Nautical};
    int32_t range_nm{kDefaultRangeNm};
    uint16_t track_deg{0};
    int32_t speed_mps{0};
    uint32_t flight_seconds{0};
    bool flight_time_valid{false};
    bool airborne{false};
    bool receiver_listening{false};
    int n_targets{0};
    const RadarTarget* targets{nullptr};
    traffic::Level max_alarm{traffic::Level::None};
};

void draw_radar(Framebuffer& fb, const RadarSnapshot& snap);

}

#endif
