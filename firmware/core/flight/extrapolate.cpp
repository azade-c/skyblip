#include "core/flight/extrapolate.h"

#include "core/model/aircraft.h"
#include "core/model/ownship.h"
#include "core/util/intmath.h"

namespace skyblip::flight {

namespace {

constexpr int64_t kTrigOne = 16384;
constexpr int kTrackC9ToAngle = 7;
constexpr int32_t kTrackC9Mask = 0x1FF;
constexpr int64_t kTurn16 = 65536;
constexpr int64_t kMilliDegreesPerTurn = 360000;
// The tree's one figure for the size of the earth, as core/protocol/nmea_out
// carries it: 1e-7 degree of latitude is 11132 micrometres.
constexpr int64_t kMicrometresPerE7 = 11132;
constexpr int64_t kSpeedQPerMps = 4;
constexpr int64_t kClimbE8PerMps = 8;
constexpr int64_t kMicrometresPerMetre = 1000000;
constexpr int64_t kMsPerS = 1000;
constexpr int64_t kE7PerTurn = 3600000000LL;
// Below this cosine a metre of easting is more than a degree of longitude and
// the scaling stops meaning anything. 88 degrees of latitude is 500 km further
// north than anything this device will fly over.
constexpr int64_t kMinLatCosine = 512;

int64_t div_round(int64_t num, int64_t den) {
    return num >= 0 ? (num + den / 2) / den : -((-num + den / 2) / den);
}

int64_t iabs64(int64_t v) { return v < 0 ? -v : v; }

int16_t lat_angle16(int32_t lat_1e7) {
    return static_cast<int16_t>((static_cast<int64_t>(lat_1e7) * kTurn16) / kE7PerTurn);
}

int64_t lat_cosine(int32_t lat_1e7) {
    const int64_t c = icos(lat_angle16(lat_1e7));
    return c < kMinLatCosine ? kMinLatCosine : c;
}

int16_t angle16_of(uint16_t track_c9) {
    return static_cast<int16_t>(
        static_cast<uint16_t>((track_c9 & kTrackC9Mask) << kTrackC9ToAngle));
}

// Three angle units meet in this file: degrees per second off the turn rate,
// cordic9 on the wire, and the 16-bit cordic the sine table is indexed by.
// This is the only conversion between the first and the last.
int32_t turn_angle16(int16_t turn_dps, int32_t dt_ms) {
    return static_cast<int32_t>(
        div_round(static_cast<int64_t>(turn_dps) * dt_ms * kTurn16, kMilliDegreesPerTurn));
}

struct Motion {
    int32_t lat_1e7;
    int32_t lon_1e7;
    int32_t alt_m;
    int32_t alt_msl_m;
    uint16_t speed_q;
    uint16_t track_c9;
    int16_t climb_e8;
    int16_t turn_dps;
    bool known;
    bool climbs;
};

Prediction carry(const Motion& m, int32_t dt_ms) {
    Prediction out{};
    out.lat_1e7 = m.lat_1e7;
    out.lon_1e7 = m.lon_1e7;
    out.alt_m = m.alt_m;
    out.alt_msl_m = m.alt_msl_m;
    out.track_c9 = static_cast<uint16_t>(m.track_c9 & kTrackC9Mask);
    out.valid = false;

    if (!m.known) return out;
    if (dt_ms > kMaxExtrapolationMs || dt_ms < -kMaxExtrapolationMs) return out;
    out.valid = true;
    if (dt_ms == 0) return out;

    const int32_t turn16 = turn_angle16(m.turn_dps, dt_ms);
    const int16_t heading =
        static_cast<int16_t>(static_cast<uint16_t>(angle16_of(m.track_c9) + turn16 / 2));

    const int64_t scale = kTrigOne * kMicrometresPerE7;
    const int64_t travel =
        static_cast<int64_t>(m.speed_q) * dt_ms * kMicrometresPerMetre / (kSpeedQPerMps * kMsPerS);
    out.lat_1e7 = m.lat_1e7 + static_cast<int32_t>(div_round(travel * icos(heading), scale));
    const int64_t east = div_round(travel * isin(heading), scale);
    out.lon_1e7 =
        m.lon_1e7 + static_cast<int32_t>(div_round(east * kTrigOne, lat_cosine(m.lat_1e7)));

    if (m.climbs) {
        const int32_t rise = static_cast<int32_t>(
            div_round(static_cast<int64_t>(m.climb_e8) * dt_ms, kClimbE8PerMps * kMsPerS));
        out.alt_m = m.alt_m + rise;
        out.alt_msl_m = m.alt_msl_m + rise;
    }

    const uint16_t track16 =
        static_cast<uint16_t>(angle16_of(m.track_c9) + turn16 + (1 << (kTrackC9ToAngle - 1)));
    out.track_c9 = static_cast<uint16_t>((track16 >> kTrackC9ToAngle) & kTrackC9Mask);
    return out;
}

}  // namespace

Prediction extrapolate(const model::OwnState& own, int32_t dt_ms) {
    Motion m{};
    m.lat_1e7 = own.lat_1e7;
    m.lon_1e7 = own.lon_1e7;
    m.alt_m = own.alt_m;
    m.alt_msl_m = own.alt_msl_m;
    m.speed_q = own.speed_q;
    m.track_c9 = own.track_c9;
    m.climb_e8 = own.climb_e8;
    m.turn_dps = own.turn_dps;
    m.known = own.fix_valid;
    m.climbs = own.climb_valid;
    return carry(m, dt_ms);
}

// INFO: fc 13sep26 ADS-L carries no turn rate, so a neighbour is carried straight (G.1.8, G.1.10)
Prediction extrapolate(const model::AircraftObs& obs, int32_t dt_ms) {
    Motion m{};
    m.lat_1e7 = obs.lat_1e7;
    m.lon_1e7 = obs.lon_1e7;
    m.alt_m = obs.alt_m;
    m.alt_msl_m = obs.alt_m;
    m.speed_q = obs.speed_q;
    m.track_c9 = obs.track_c9;
    m.climb_e8 = obs.climb_e8;
    m.known = obs.position_valid && obs.speed_valid;
    m.climbs = obs.climb_valid;
    return carry(m, dt_ms);
}

model::OwnState carried_to(const model::OwnState& own, uint32_t now_ms) {
    const Prediction p = extrapolate(own, static_cast<int32_t>(now_ms - own.fix_ms));
    model::OwnState out = own;
    out.lat_1e7 = p.lat_1e7;
    out.lon_1e7 = p.lon_1e7;
    out.alt_m = p.alt_m;
    out.alt_msl_m = p.alt_msl_m;
    out.track_c9 = p.track_c9;
    return out;
}

model::AircraftObs carried_to(const model::AircraftObs& obs, uint32_t now_ms) {
    const Prediction p = extrapolate(obs, static_cast<int32_t>(now_ms - obs.at_ms));
    model::AircraftObs out = obs;
    out.lat_1e7 = p.lat_1e7;
    out.lon_1e7 = p.lon_1e7;
    out.alt_m = p.alt_m;
    return out;
}

uint32_t prediction_residual_m(const Prediction& predicted, int32_t lat_1e7, int32_t lon_1e7,
                               int32_t alt_m) {
    const int64_t dlat = static_cast<int64_t>(lat_1e7) - predicted.lat_1e7;
    const int64_t dlon = static_cast<int64_t>(lon_1e7) - predicted.lon_1e7;
    const int64_t north = div_round(dlat * kMicrometresPerE7, kMicrometresPerMetre);
    const int64_t east = div_round(dlon * kMicrometresPerE7 * lat_cosine(predicted.lat_1e7),
                                   kMicrometresPerMetre * kTrigOne);
    const int64_t up = static_cast<int64_t>(alt_m) - predicted.alt_m;
    return static_cast<uint32_t>(iabs64(north) + iabs64(east) + iabs64(up));
}

}  // namespace skyblip::flight
