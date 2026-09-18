#ifndef SKYBLIP_PRODUCTS_SKYBLIP_GO_PAGES_NEARBY_H
#define SKYBLIP_PRODUCTS_SKYBLIP_GO_PAGES_NEARBY_H

#include <cstdint>

#include "core/traffic/range.h"
#include "products/skyblip_go/glass.h"
#include "products/skyblip_go/settings.h"

namespace skyblip::go {

constexpr int kNearbyRows = 8;

struct NearbySnapshot {
    bool fix_valid{false};
    go::Units units{go::Units::Nautical};
    int n_heard{0};
    int n_rows{0};
    const traffic::RangeRow* rows{nullptr};
};

constexpr int kNearbyScale = 2;
constexpr int kNearbyCellW = 6 * kNearbyScale;
constexpr int kNearbyIdX = 4;
constexpr int kNearbySlantEnd = 152;
constexpr int kNearbyRelEnd = 196;
constexpr int kNearbyHeaderY = 13;
constexpr int kNearbyUnitsY = 23;
constexpr int kNearbyFirstRowY = 36;
constexpr int kNearbyLineH = 20;

void draw_nearby(ui::Canvas& fb, const NearbySnapshot& snap);

}  // namespace skyblip::go

#endif
