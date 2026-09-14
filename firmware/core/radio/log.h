#ifndef SKYBLIP_CORE_RADIO_LOG_H
#define SKYBLIP_CORE_RADIO_LOG_H

#include <cstdint>

#include "core/messages/messages.h"

namespace skyblip::radio {

enum class Event : uint8_t { Transmitted, Lost, Held, Unarmed, Received, BadCrc, Undecoded };

struct Entry {
    Event event{Event::Undecoded};
    messages::Band band{messages::Band::M};
    messages::Source source{messages::Source::AdslDirect};
    uint32_t addr{0};
    uint32_t at_s{0};
    uint16_t into_ms{0};
    uint16_t tx_span_us{0};
    int8_t rssi_dbm{0};
    uint8_t channel{0};
    uint8_t len{0};
    bool rssi_valid{false};
    bool utc{false};
    bool phase_valid{false};
    bool tx_span_valid{false};
};

struct Stamp {
    uint32_t at_s{0};
    uint16_t into_ms{0};
    bool phase_valid{false};
};

constexpr int64_t kStampReachUs = 2000000;
constexpr uint16_t kTxSpanLimitUs = 9999;

Stamp stamp_of(uint64_t at_us, uint64_t pps_edge_us, bool pps_locked, uint32_t now_s);
uint16_t tx_span_of(uint64_t done_at_us, uint64_t deadline_us);

class Log {
   public:
    static constexpr int kCapacity = 16;

    void record(const Entry& entry);
    void clear() { written_ = 0; }

    int count() const { return written_ < kCapacity ? static_cast<int>(written_) : kCapacity; }
    const Entry& newest(int i) const;

   private:
    Entry entry_[kCapacity]{};
    uint32_t written_{0};
};

}  // namespace skyblip::radio

#endif
