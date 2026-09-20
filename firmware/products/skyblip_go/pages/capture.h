#ifndef SKYBLIP_PRODUCTS_SKYBLIP_GO_PAGES_CAPTURE_H
#define SKYBLIP_PRODUCTS_SKYBLIP_GO_PAGES_CAPTURE_H

#include <cstdint>

#include "core/bus/state.h"
#include "products/skyblip_go/glass.h"

namespace skyblip::go {

struct CaptureSnapshot {
    uint32_t uptime_s{0};
    bus::CaptureState capture{};
    bool arming{false};
};

void draw_capture(ui::Canvas& fb, const CaptureSnapshot& snap);

}  // namespace skyblip::go

#endif
