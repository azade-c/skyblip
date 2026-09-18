#ifndef SKYBLIP_PRODUCTS_SKYBLIP_GO_PAGES_RADAR_H
#define SKYBLIP_PRODUCTS_SKYBLIP_GO_PAGES_RADAR_H

#include <cstdint>

#include "core/traffic/alarm.h"
#include "core/units/units.h"
#include "products/skyblip_go/glass.h"
#include "products/skyblip_go/settings.h"

namespace skyblip::go {

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
    go::Units units{go::Units::Nautical};
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

void draw_radar(ui::Canvas& fb, const RadarSnapshot& snap);

}

#endif
