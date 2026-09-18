#ifndef SKYBLIP_PRODUCTS_SKYBLIP_GO_INPUT_CONTROLS_H
#define SKYBLIP_PRODUCTS_SKYBLIP_GO_INPUT_CONTROLS_H

#include <cstdint>

#include "core/events/input.h"
#include "core/power/shutdown.h"

namespace skyblip::go {

enum class Command : uint8_t { None, Act, Next, Home };

class Controls {
   public:
    static constexpr uint32_t kHomeTouchMs = 1000;

    static_assert(kHomeTouchMs < power::kLongPressMs,
                  "the pad's way home has to resolve before the hold that stows the device");

    Command read(const events::ContactEvent& event) {
        return event.contact == events::Contact::Button ? button(event) : pad(event);
    }

    Command tick(uint32_t now_ms) {
        if (!touching_ || touch_spent_ || now_ms - touched_ms_ < kHomeTouchMs) return Command::None;
        touch_spent_ = true;
        return Command::Home;
    }

    bool touching() const { return touching_; }

   private:
    Command button(const events::ContactEvent& event) {
        if (event.down) {
            pressing_ = true;
            pressed_ms_ = event.at_ms;
            touch_spent_ = touch_spent_ || touching_;
            return Command::None;
        }
        pressing_ = false;
        return released_before_the_stow(event.at_ms) ? Command::Act : Command::None;
    }

    Command pad(const events::ContactEvent& event) {
        if (event.down) {
            touching_ = true;
            touched_ms_ = event.at_ms;
            touch_spent_ = pressing_;
            return Command::None;
        }
        touching_ = false;
        if (touch_spent_) return Command::None;
        touch_spent_ = true;
        return touched_shorter_than_the_way_home(event.at_ms) ? Command::Next : Command::Home;
    }

    bool released_before_the_stow(uint32_t at_ms) const {
        return at_ms - pressed_ms_ < power::kLongPressMs;
    }

    bool touched_shorter_than_the_way_home(uint32_t at_ms) const {
        return at_ms - touched_ms_ < kHomeTouchMs;
    }

    uint32_t pressed_ms_{0};
    uint32_t touched_ms_{0};
    bool pressing_{false};
    bool touching_{false};
    bool touch_spent_{false};
};

}  // namespace skyblip::go

#endif
