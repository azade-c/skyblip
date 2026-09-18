#ifndef SKYBLIP_PRODUCTS_SKYBLIP_GO_SERVICES_OWNSHIP_H
#define SKYBLIP_PRODUCTS_SKYBLIP_GO_SERVICES_OWNSHIP_H

#include "core/events/sensor.h"
#include "core/flight/extrapolate.h"
#include "core/flight/ground.h"
#include "core/flight/state.h"
#include "core/flight/timer.h"
#include "core/gnss/first_fix.h"
#include "core/model/ownship.h"
#include "products/skyblip_go/settings.h"
#include "runtime/service.h"
#include "runtime/tasks.h"

namespace skyblip::go {

// Owns state.own: sensor readings become the one state the protocol encoder, the
// alarm logic and every screen read.
class OwnshipService : public runtime::Service {
   public:
    OwnshipService(runtime::Context& context, const Settings& settings)
        : runtime::Service(context), settings_(settings) {}

    void tick(uint32_t now_ms) override;

    bool baro_active() const { return baro_ref_ms_ != 0; }

    // ADS-L G.1.4 FlightState, decided by core/flight from the fix stream.
    flight::FlightState flight_state_from(const model::OwnState& own, uint32_t now_ms);

    // The one copy of "has the receiver settled": the transmit gate reads it
    // through state.own.tx_settled, and whoever annunciates the first fix takes
    // the edge from here rather than keeping a second watch of its own.
    const gnss::FirstFix& first_fix() const { return settle_; }
    bool take_fix_acquired() { return settle_.take_acquired(); }

   private:
    void apply_solution(const gnss::GnssSolution& solution, uint32_t now_ms);
    uint32_t solution_instant(const gnss::GnssSolution& solution, uint32_t now_ms) const;
    void anchor_utc(const gnss::GnssSolution& solution);
    void apply_baro(const events::BaroSample& sample);
    void update_derived_qnh(const events::BaroSample& sample);
    void update_turn_rate(uint32_t now_ms);
    void update_residual(const model::OwnState& previous);
    void adopt_climb(int32_t mm_s);
    static bool vs_from_alt_mm(int32_t alt_mm, uint32_t now_ms, uint32_t window_ms,
                               int32_t& ref_alt_mm, uint32_t& ref_ms, int32_t& out_mm_s);

    flight::FlightMonitor flight_{};
    flight::FlightTimer timer_{};
    flight::GroundLatch ground_{};
    gnss::FirstFix settle_{};
    int32_t vs_ref_alt_mm_{0};
    uint32_t vs_ref_ms_{0};
    int32_t baro_ref_alt_mm_{0};
    uint32_t baro_ref_ms_{0};
    uint32_t turn_ref_ms_{0};
    uint16_t turn_ref_track_c9_{0};
    int32_t qnh_filter_acc_{0};

    static constexpr uint32_t kVsWindowMs = 2000;
    static constexpr uint32_t kBaroVsWindowMs = runtime::kBaroPeriodMs / 2;
    static constexpr uint32_t kTurnWindowMs = 1000;
    static constexpr int32_t kQnhHalfMinuteSamples = 32;
    static constexpr int32_t kQnhSteadyClimbMmS = 4000;
    const Settings& settings_;
};

}  // namespace skyblip::go

#endif
