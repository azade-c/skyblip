#ifndef SKYBLIP_UI_WIDGETS_STONE_H
#define SKYBLIP_UI_WIDGETS_STONE_H

#include <cstdint>

#include "ui/canvas.h"

namespace skyblip::ui {

enum class Cut : uint8_t { Crown, Diamond, Pavilion };
enum class Band : uint8_t { Near, Far };
enum class Trend : uint8_t { Level, Climbing, Sinking };

void draw_stone(Canvas& fb, int x, int y, Cut cut, Band band, Trend trend, bool alarm);

int stone_above(Cut cut, Band band, Trend trend);
int stone_below(Cut cut, Band band, Trend trend);
int stone_beside(Band band);

}  // namespace skyblip::ui

#endif
