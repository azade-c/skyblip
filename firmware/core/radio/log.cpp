#include "core/radio/log.h"

namespace skyblip::radio {

namespace {
constexpr int64_t kSecondUs = 1000000;
}

Stamp stamp_of(uint64_t at_us, uint64_t pps_edge_us, bool pps_locked, uint32_t now_s) {
    Stamp stamp{};
    stamp.at_s = now_s;
    if (!pps_locked) return stamp;

    const int64_t rel_us = static_cast<int64_t>(at_us) - static_cast<int64_t>(pps_edge_us);
    if (rel_us <= -kStampReachUs || rel_us >= kStampReachUs) return stamp;

    int64_t seconds = rel_us / kSecondUs;
    int64_t into_us = rel_us % kSecondUs;
    if (into_us < 0) {
        seconds--;
        into_us += kSecondUs;
    }
    if (seconds < 0 && static_cast<uint64_t>(-seconds) > now_s) return stamp;

    stamp.at_s = static_cast<uint32_t>(static_cast<int64_t>(now_s) + seconds);
    stamp.into_ms = static_cast<uint16_t>(into_us / 1000);
    stamp.phase_valid = true;
    return stamp;
}

uint16_t tx_span_of(uint64_t done_at_us, uint64_t deadline_us) {
    if (done_at_us <= deadline_us) return 0;
    const uint64_t span_us = done_at_us - deadline_us;
    return span_us > kTxSpanLimitUs ? kTxSpanLimitUs : static_cast<uint16_t>(span_us);
}

void Log::record(const Entry& entry) {
    entry_[written_ % kCapacity] = entry;
    written_++;
}

const Entry& Log::newest(int i) const {
    const int held = count();
    if (held == 0) return entry_[0];
    const int back = i < 0 ? 0 : (i >= held ? held - 1 : i);
    return entry_[(written_ - 1u - static_cast<uint32_t>(back)) % kCapacity];
}

}  // namespace skyblip::radio
