#ifndef SKYBLIP_CORE_EVENTS_INPUT_H
#define SKYBLIP_CORE_EVENTS_INPUT_H

#include <cstdint>

namespace skyblip::events {

enum class Contact : uint8_t { Button, Pad };

struct ContactEvent {
    Contact contact{Contact::Button};
    bool down{false};
    uint32_t at_ms{0};
};

}  // namespace skyblip::events

#endif
