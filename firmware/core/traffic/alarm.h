#ifndef SKYBLIP_CORE_TRAFFIC_ALARM_H
#define SKYBLIP_CORE_TRAFFIC_ALARM_H

#include <array>
#include <cstdint>

#include "core/model/aircraft.h"
#include "core/model/ownship.h"

namespace skyblip::traffic {

enum class Level : uint8_t { None, Info, Important, Urgent };

constexpr uint8_t to_number(Level level) { return static_cast<uint8_t>(level); }

struct AlarmAssessment {
    Level level{Level::None};
    uint16_t rel_bearing_deg{0};
    int32_t rel_dist_m{0};
    int32_t rel_vert_m{0};
    int32_t closing_mps{0};
    bool breaches{false};
    int32_t at_s{0};
    int32_t miss_m{0};
    bool valid{false};
};

constexpr int32_t kVertWindowM = 300;
constexpr int32_t kInfoDistM = 3000;
constexpr int32_t kUrgentTtiS = 15;
constexpr int32_t kImportantTtiS = 25;

// INFO: fc 14sep26 a relayed target arrives with no velocity, and zero would make it the safest dot
constexpr int32_t kUnknownTargetSpeedMps = 30;

constexpr int32_t kNoImpactS = 32767;

// INFO: al 02aug26 SoftRF alerts only on targets seen within ALERT_EXPIRATION_TIME
// (5 s) and re-checks no more often than every 2 s
// (oss/SoftRF-lyusupov .../src/TrafficHelper.h:58-59, .../src/TrafficHelper.cpp:236-260).
constexpr uint32_t kAlertMaxAgeMs = 5000;
constexpr uint32_t kRenotifyMs = 2000;

constexpr Level kReminderLevel = Level::Urgent;

constexpr int kTrackedTargets = 48;
constexpr uint32_t kForgetMs = 30000;

// INFO: fc 13sep26 now_ms is the instant both sides are carried to, on ports::Clock
AlarmAssessment assess(const model::OwnState& own_fix, const model::AircraftObs& reported,
                       uint32_t now_ms);
AlarmAssessment assess(const model::OwnState& own_fix, const model::AircraftObs& reported,
                       int16_t target_turn_dps, bool target_turn_valid, uint32_t now_ms);

class AlarmTracker {
   public:
    struct Decision {
        AlarmAssessment assessment{};
        bool notify{false};
        bool escalated{false};
    };

    Decision update(const model::OwnState& own, const model::AircraftObs& target,
                    int16_t target_turn_dps, bool target_turn_valid, uint32_t now_ms);
    Decision update(const model::OwnState& own, const model::AircraftObs& target, uint32_t now_ms);

    void forget_stale(uint32_t now_ms);
    void withdraw(uint8_t addr_table, uint32_t addr);

    Level announced_level(uint32_t now_ms) const;

   private:
    struct Slot {
        bool used{false};
        uint8_t addr_table{0};
        uint32_t addr{0};
        uint32_t obs_key{0};
        uint32_t seen_ms{0};
        Level notified_level{Level::None};
        uint32_t notified_ms{0};
        bool falling{false};
        uint32_t falling_since_ms{0};
    };

    Slot* slot_for(const model::AircraftObs& target, uint32_t now_ms);
    static bool notify_for(Slot& slot, Level level, uint32_t now_ms, bool& escalated);

    std::array<Slot, kTrackedTargets> slots_{};
};

}  // namespace skyblip::traffic

#endif
