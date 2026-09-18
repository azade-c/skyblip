#ifndef SKYBLIP_PRODUCTS_SKYBLIP_GO_GLASS_H
#define SKYBLIP_PRODUCTS_SKYBLIP_GO_GLASS_H

#include "boards/lilygo/t_echo_plus/glass.h"
#include "ui/canvas.h"

namespace skyblip::go {

constexpr int kGlassW = boards::t_echo_plus::kGlassW;
constexpr int kGlassH = boards::t_echo_plus::kGlassH;

using Glass = ui::Panel<kGlassW, kGlassH>;

}  // namespace skyblip::go

#endif
