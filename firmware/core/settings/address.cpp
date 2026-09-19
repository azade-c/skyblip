#include "core/settings/address.h"

namespace skyblip::settings {

uint32_t air_address(uint32_t addr) {
    const uint32_t a = addr & kAddressMask;
    if (a == kUnusableLow || a == kUnusableHigh) return kFallbackAddress;
    return a;
}

}  // namespace skyblip::settings
