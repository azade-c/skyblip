#ifndef SKYBLIP_UI_SCREENS_RADIO_LOG_H
#define SKYBLIP_UI_SCREENS_RADIO_LOG_H

#include <cstdint>

#include "core/radio/log.h"
#include "ui/framebuffer.h"

namespace skyblip::ui {

constexpr int kRadioLogRows = radio::Log::kCapacity;

enum class PpsState : uint8_t { None, Lock, Holdover };

struct GnssReception {
    bool fix_valid{false};
    uint8_t sats{0};
    PpsState pps{PpsState::None};
    uint16_t pps_age_s{0};
};

struct RadioLogSnapshot {
    GnssReception gnss{};
    uint32_t rx_ok{0};
    uint32_t tx_ok{0};
    uint32_t noise{0};
    int8_t band_dbm{0};
    bool airborne{false};
    int n_rows{0};
    const radio::Log* log{nullptr};
};

void draw_radio_log(Framebuffer& fb, const RadioLogSnapshot& snap);

}  // namespace skyblip::ui

#endif
