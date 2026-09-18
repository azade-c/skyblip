#ifndef SKYBLIP_PRODUCTS_SKYBLIP_GO_PAGES_SATS_H
#define SKYBLIP_PRODUCTS_SKYBLIP_GO_PAGES_SATS_H

#include <cstdint>

#include "core/gnss/acquisition.h"
#include "core/gnss/sky.h"
#include "products/skyblip_go/glass.h"

namespace skyblip::go {

struct SatsSnapshot {
    bool fix_valid{false};
    bool levels_live{false};
    gnss::Stage stage{gnss::Stage::Silent};
    uint32_t stage_s{0};
    uint8_t sats{0};
    uint16_t hdop_e2{0};
    uint16_t vdop_e2{0};
    const gnss::SkyView* sky{nullptr};
};

void draw_sats(ui::Canvas& fb, const SatsSnapshot& snap);

}  // namespace skyblip::go

#endif
