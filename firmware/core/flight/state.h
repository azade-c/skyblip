#ifndef SKYBLIP_CORE_FLIGHT_STATE_H
#define SKYBLIP_CORE_FLIGHT_STATE_H

#include <cstdint>

namespace skyblip::flight {

// INFO: fc 18sep26 ADS-L 4 SRD860 issue 2 G.1.4 codes, the wire values themselves
enum class FlightState : uint8_t { Unknown = 0, OnGround = 1, Airborne = 2 };

struct FlightSample {
    uint16_t speed_q{0};  // quarter metres per second
    int16_t climb_e8{0};  // eighth metres per second
    uint16_t hdop_e2{0};  // hundredths; zero means the receiver did not report
    bool fix_valid{false};
    bool climb_valid{false};
};

constexpr uint16_t kFlightSpeedQ = 60;  // 15.0 m/s
constexpr int16_t kFlightClimbE8 = 16;  // 2.0 m/s
constexpr uint16_t kGroundSpeedQ = 10;  // 2.5 m/s
constexpr int16_t kGroundClimbE8 = 8;   // 1.0 m/s
constexpr uint16_t kDopUnityE2 = 100;

// INFO: fc 18sep26 moshe-braner's jerk gate: a speed jumping 4x between solutions is noise
constexpr int32_t kJerkSpeedRatio = 4;

bool flight_evidence(const FlightSample& sample);
bool ground_evidence(const FlightSample& sample);

FlightState state_from(uint8_t adsl_code);
inline bool airborne(uint8_t adsl_code) { return state_from(adsl_code) == FlightState::Airborne; }

class FlightMonitor {
   public:
    FlightState update(const FlightSample& sample);

    FlightState state() const { return state_; }
    bool airborne() const { return state_ == FlightState::Airborne; }

   private:
    static bool jerky(uint16_t previous_q, uint16_t now_q);

    FlightState state_{FlightState::Unknown};
    uint16_t last_speed_q_{0};
    bool armed_{false};
};

}  // namespace skyblip::flight

#endif
