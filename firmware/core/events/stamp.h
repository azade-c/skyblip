#ifndef SKYBLIP_CORE_EVENTS_STAMP_H
#define SKYBLIP_CORE_EVENTS_STAMP_H

#include <cstdint>

namespace skyblip::events {

struct Stamp {
    uint32_t at_s{0};
    uint16_t into_ms{0};
    bool phase_valid{false};
};

constexpr int64_t kStampReachUs = 2000000;

Stamp stamp_of(uint64_t at_us, uint64_t pps_edge_us, bool pps_locked, uint32_t now_s);

}  // namespace skyblip::events

#endif
