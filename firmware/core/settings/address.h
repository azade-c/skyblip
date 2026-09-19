#ifndef SKYBLIP_CORE_SETTINGS_ADDRESS_H
#define SKYBLIP_CORE_SETTINGS_ADDRESS_H

#include <cstdint>

namespace skyblip::settings {

constexpr uint32_t kAddressMask = 0x00FFFFFFu;

constexpr uint32_t kUnusableLow = 0x000000u;
constexpr uint32_t kUnusableHigh = kAddressMask;
constexpr uint32_t kFallbackAddress = 0x5BCAFEu;

// TODO: fc 19sep26 claim our own AMT page once registry@ads-l.aero assigns one (F.2.2)
constexpr uint8_t kAddrTableOgn = 7;

uint32_t air_address(uint32_t addr);

}  // namespace skyblip::settings

#endif
