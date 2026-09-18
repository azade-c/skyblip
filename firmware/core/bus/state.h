#ifndef SKYBLIP_CORE_BUS_STATE_H
#define SKYBLIP_CORE_BUS_STATE_H

#include "core/events/rf.h"
#include "core/flight/atmosphere.h"
#include "core/flight/ground.h"
#include "core/model/ownship.h"
#include "core/power/battery.h"
#include "core/power/charging.h"
#include "core/power/cutoff.h"
#include "core/radio/log.h"
#include "core/timing/channel.h"
#include "core/timing/durable_write.h"
#include "core/timing/slot.h"
#include "core/timing/timing_stats.h"
#include "core/traffic/table.h"

namespace skyblip::bus {

// Where the radio believes it is inside the second it is arming, stamped with
// the pass it said so on. The radio service is the only writer; whoever needs
// to know whether the core may be stalled reads it here rather than deriving
// the phase a second time (core/timing/durable_write.h).
struct RfState {
    timing::SlotPlan plan{};
    timing::DwellPhase dwell{};
    // The bench accumulator G6 reads out: boards/ is the one writer of
    // the PPS half, products/skyblip_go/services/radio.cpp of the dwell half.
    timing::SlotTimingStats timing_stats{};
    uint64_t tx_deadline_us{0};
    uint16_t last_tx_keyed_us{0};
    uint16_t last_tx_span_us{0};
    int8_t noise_dbm{timing::NoiseFloor::kSeedDbm};
};

struct PowerState {
    power::BatteryState battery{};
    // What the cutoff monitor made of the same samples the gauge saw. Whoever
    // draws a low cell reads this rather than comparing millivolts again: the
    // debounce, the charger and the sanity floor are decided once.
    power::PowerLevel level{power::PowerLevel::Unknown};
    power::ChargeCondition charge{power::ChargeCondition::Unknown};
    bool supply_warned{false};
    int16_t die_dc{0};
    bool die_valid{false};
};

struct FlightStatus {
    uint32_t gnss_solutions{0};
    uint32_t seconds{0};
    bool time_valid{false};
    bool running{false};
    flight::FlightState confirmed_state{flight::FlightState::Unknown};
};

struct BaroState {
    uint32_t pressure_mpa{0};
    // The altimeter subscale, as the pilot sets it: standard until told otherwise.
    uint32_t qnh_pa{flight::kIsaSeaLevelPa};
    uint32_t derived_qnh_pa{0};
    bool active{false};
};

struct FormationState {
    int members{0};
};

struct SlipState {
    int16_t lateral_mg{0};
    bool valid{false};
};

struct State {
    model::OwnState own{};
    timing::ClockState clock{};
    traffic::TrafficTable traffic{};
    radio::Log radio_log{};
    RfState rf{};
    PowerState power{};
    FlightStatus flight{};
    BaroState baro{};
    SlipState slip{};
    FormationState formation{};

    traffic::Level alarm_level{traffic::Level::None};
    bool alarm_dismissed{false};

    struct AirCounts {
        uint32_t rx_ok{0};
        uint32_t rx_bad{0};
        uint32_t rx_noise{0};
        uint32_t tx_ok{0};
        // INFO: fc 05aug26 The O-band uplink is its own path and is counted apart
        // from the M band's: every frame that arrived in the uplink dwell, the ones
        // Reed-Solomon refused, and the aircraft the rest of them put in the table.
        // The third is smaller than the aircraft the frames carried whenever the
        // ground station relayed one back that the table refuses - own-ship, or an
        // aircraft we are hearing better first-hand. Until 2026-08-05 an uplink
        // frame reached protocol::receive_mband, failed to frame as either M-band
        // system and landed in rx_bad: the whole feature was absent and its absence
        // looked like radio noise, which is what hid it.
        uint32_t uplink_frames{0};
        uint32_t uplink_bad{0};
        uint32_t uplink_targets{0};
        // The instant the executor actually reported completion for, published by
        // whoever already drains events::RfEvent (TrafficService) so the policy
        // layer that owns the deadline (RadioService) can measure against it
        // without a second reader of the bus.
        uint64_t last_tx_done_at_us{0};
    } air{};

    bool panel_presented{false};
    bool started{false};

    // The traffic table's single time base. Mixing GNSS epoch seconds with
    // boot-relative seconds underflows uint32 and ages every target out at once.
    uint32_t traffic_now(uint32_t now_ms) const {
        if (clock.pps_locked && clock.utc_s != 0) return clock.utc_s;
        return own.utc_valid ? own.utc : now_ms / 1000;
    }
};

}  // namespace skyblip::bus

#endif
