#ifndef SKYBLIP_CORE_GNSS_ACQUISITION_H
#define SKYBLIP_CORE_GNSS_ACQUISITION_H

#include <cstdint>

#include "core/gnss/nmea.h"

namespace skyblip::gnss {

enum class Stage : uint8_t { Silent, Blind, Solving, Fixed };

const char* stage_name(Stage stage);

Stage stage_of(const GnssSolution& solution);

class Acquisition {
   public:
    void observe(const GnssSolution& solution, uint32_t now_ms);

    void tick(uint32_t now_ms);

    Stage stage() const { return stage_; }

    uint32_t stage_ms(uint32_t now_ms) const { return now_ms - since_ms_; }

   private:
    void enter(Stage stage, uint32_t now_ms);

    uint32_t since_ms_{0};
    uint32_t solution_ms_{0};
    Stage stage_{Stage::Silent};
    bool heard_{false};
};

}  // namespace skyblip::gnss

#endif
