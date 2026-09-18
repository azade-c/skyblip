#include "core/traffic/conflict.h"

#include "core/util/intmath.h"

namespace skyblip::traffic {

namespace {

constexpr int32_t kMmPerM = 1000;

int32_t iabs32(int32_t v) { return v < 0 ? -v : v; }

int64_t squared(int32_t v) { return static_cast<int64_t>(v) * v; }

}  // namespace

int32_t protection_radius_m(int32_t lead_s) {
    const int32_t spread = (kSpreadMmPerS * lead_s) / kMmPerM;
    return kProtectionRadiusM + (spread > kMaxSpreadM ? kMaxSpreadM : spread);
}

int32_t protection_vert_m(int32_t lead_s) {
    const int32_t spread = (kVertSpreadMmPerS * lead_s) / kMmPerM;
    return kProtectionVertM + (spread > kMaxVertSpreadM ? kMaxVertSpreadM : spread);
}

Conflict first_breach(const flight::Motion& own, const flight::Motion& target) {
    Conflict out{};
    flight::Arc own_arc(own, kStepMs);
    flight::Arc target_arc(target, kStepMs);
    const int32_t step_s = static_cast<int32_t>(kStepMs / 1000);
    int64_t closest_sq = -1;

    for (int32_t lead_s = step_s; lead_s <= kHorizonS; lead_s += step_s) {
        const flight::Position o = own_arc.advance();
        const flight::Position g = target_arc.advance();
        const int32_t north = g.north_m - o.north_m;
        const int32_t east = g.east_m - o.east_m;
        const int32_t vert_m = g.up_m - o.up_m;
        const int64_t miss_sq = squared(north) + squared(east);

        if (closest_sq < 0 || miss_sq < closest_sq) {
            closest_sq = miss_sq;
            out.closest_m = static_cast<int32_t>(idistance(north, east));
            out.closest_at_s = lead_s;
        }
        if (out.breaches) continue;
        if (miss_sq > squared(protection_radius_m(lead_s))) continue;
        if (iabs32(vert_m) > protection_vert_m(lead_s)) continue;
        out.breaches = true;
        out.at_s = lead_s;
        out.miss_m = static_cast<int32_t>(idistance(north, east));
        out.vert_m = vert_m;
    }
    return out;
}

}  // namespace skyblip::traffic
