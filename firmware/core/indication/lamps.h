#ifndef SKYBLIP_CORE_INDICATION_LAMPS_H
#define SKYBLIP_CORE_INDICATION_LAMPS_H

#include <cstdint>

namespace skyblip::indication {

enum class Lamp : uint8_t { None, Green, Red, Blue };

}  // namespace skyblip::indication

#endif
