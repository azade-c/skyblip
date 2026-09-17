#ifndef SKYBLIP_CORE_EVENTS_INPUT_H
#define SKYBLIP_CORE_EVENTS_INPUT_H

#include <cstdint>

namespace skyblip::events {
struct ButtonEvent {
    uint8_t id;
};

constexpr uint8_t kButtonPressed = 0;
constexpr uint8_t kPadHeld = 1;
constexpr uint8_t kPadTapped = 2;

}  // namespace skyblip::events

#endif
