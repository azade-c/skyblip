// Every emitter we can hear, nearest first, with the strength it arrived at and
// the transmit power that strength implies at that range. Two aircraft at the
// same distance with 12 dB between their implied e.r.p. is an antenna finding,
// and this is the page that shows it. Range here is slant range, the path the
// wave took, not the horizontal separation the radar and the alarm work in.
#ifndef SKYBLIP_PRODUCTS_SKYBLIP_GO_PAGES_SIGNAL_H
#define SKYBLIP_PRODUCTS_SKYBLIP_GO_PAGES_SIGNAL_H

#include <cstdint>

#include "core/traffic/link.h"
#include "products/skyblip_go/glass.h"
#include "products/skyblip_go/settings.h"

namespace skyblip::go {

constexpr int kSignalRows = 9;

struct SignalSnapshot {
    bool fix_valid{false};
    go::Units units{go::Units::Nautical};
    int n_heard{0};
    int n_rows{0};
    const traffic::LinkRow* rows{nullptr};
};

void draw_signal(ui::Canvas& fb, const SignalSnapshot& snap);

}  // namespace skyblip::go

#endif
