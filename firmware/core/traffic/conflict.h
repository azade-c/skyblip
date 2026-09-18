#ifndef SKYBLIP_CORE_TRAFFIC_CONFLICT_H
#define SKYBLIP_CORE_TRAFFIC_CONFLICT_H

#include <cstdint>

#include "core/flight/arc.h"

namespace skyblip::traffic {

constexpr int32_t kProtectionRadiusM = 75;
constexpr int32_t kProtectionVertM = 50;
constexpr int32_t kSpreadMmPerS = 500;
constexpr int32_t kVertSpreadMmPerS = 250;
constexpr int32_t kMaxSpreadM = 30;
constexpr int32_t kMaxVertSpreadM = 15;

constexpr int32_t kHorizonS = 60;
constexpr uint32_t kStepMs = 2000;

struct Conflict {
    bool breaches{false};
    int32_t at_s{0};
    int32_t miss_m{0};
    int32_t vert_m{0};
    int32_t closest_m{0};
    int32_t closest_at_s{0};
};

int32_t protection_radius_m(int32_t lead_s);
int32_t protection_vert_m(int32_t lead_s);

Conflict first_breach(const flight::Motion& own, const flight::Motion& target);

}  // namespace skyblip::traffic

#endif
