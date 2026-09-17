#ifndef SKYBLIP_PORTS_GNSS_H
#define SKYBLIP_PORTS_GNSS_H

#include <cstdint>

namespace skyblip::ports {

enum class Restart : uint8_t { Hot, Warm, Cold, Factory };

class Gnss {
   public:
    virtual ~Gnss() = default;

    virtual void request_restart(Restart) {}
};

}  // namespace skyblip::ports

#endif
