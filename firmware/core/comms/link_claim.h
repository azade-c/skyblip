#ifndef SKYBLIP_CORE_COMMS_LINK_CLAIM_H
#define SKYBLIP_CORE_COMMS_LINK_CLAIM_H

#include <cstdint>

namespace skyblip::comms {

class LinkClaim {
   public:
    bool grant(uint16_t session_id) {
        if (!held_) {
            held_ = true;
            holder_ = session_id;
        }
        return holder_ == session_id;
    }

    bool holds(uint16_t session_id) const { return held_ && holder_ == session_id; }
    bool held() const { return held_; }
    uint16_t holder() const { return holder_; }

    void release(uint16_t session_id) {
        if (holds(session_id)) release();
    }

    void release() {
        held_ = false;
        holder_ = 0;
    }

   private:
    uint16_t holder_{0};
    bool held_{false};
};

}

#endif
