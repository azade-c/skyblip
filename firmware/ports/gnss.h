#ifndef SKYBLIP_PORTS_GNSS_H
#define SKYBLIP_PORTS_GNSS_H

#include <cstdint>

#include "core/gnss/validity.h"

namespace skyblip::ports {

enum class Restart : uint8_t { Hot, Warm, Cold, Factory };

enum class GnssConfig : uint8_t {
    Idle,
    Restarting,
    Waking,
    Identifying,
    Sending,
    Verifying,
    Confirming,
    Ready,
    Degraded
};

constexpr const char* to_string(GnssConfig state) {
    switch (state) {
        case GnssConfig::Idle: return "IDLE";
        case GnssConfig::Restarting: return "RESTART";
        case GnssConfig::Waking: return "WAKING";
        case GnssConfig::Identifying: return "IDENT";
        case GnssConfig::Sending: return "SENDING";
        case GnssConfig::Verifying: return "VERIFY";
        case GnssConfig::Confirming: return "CONFIRM";
        case GnssConfig::Ready: return "READY";
        case GnssConfig::Degraded: return "DEGRADED";
    }
    return "?";
}

struct GnssHealth {
    uint32_t baud{0};
    uint32_t overruns{0};
    uint32_t sentences{0};
    uint32_t rejected{0};
    uint16_t pps_latency_ms{0};
    gnss::FixReject reject{gnss::FixReject::None};
    GnssConfig config{GnssConfig::Idle};
    bool identified{false};
};

class Gnss {
   public:
    virtual ~Gnss() = default;

    virtual void request_restart(Restart) {}

    virtual GnssHealth health() const { return {}; }
};

}  // namespace skyblip::ports

#endif
