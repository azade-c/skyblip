#ifndef SKYBLIP_CORE_UNITS_UNITS_H
#define SKYBLIP_CORE_UNITS_UNITS_H

#include <cstdint>

namespace skyblip {

struct Metres {
    int32_t v{0};
    constexpr Metres() = default;
    constexpr explicit Metres(int32_t m) : v(m) {}
    constexpr bool operator==(Metres o) const { return v == o.v; }
};

struct Feet {
    int32_t v{0};
    constexpr Feet() = default;
    constexpr explicit Feet(int32_t f) : v(f) {}
};

constexpr Metres to_metres(Feet f) { return Metres((f.v * 2497 + 4096) >> 13); }
constexpr Feet to_feet(Metres m) { return Feet((m.v * 3360 + 512) >> 10); }

constexpr int32_t kMetresPerNm = 1852;

struct NauticalMilesE1 {
    int32_t v{0};
    constexpr NauticalMilesE1() = default;
    constexpr explicit NauticalMilesE1(int32_t tenths) : v(tenths) {}
};

constexpr NauticalMilesE1 to_nm_e1(Metres m) { return NauticalMilesE1(m.v * 10 / kMetresPerNm); }

struct QuarterMetresPerSec {
    uint16_t v{0};
    constexpr QuarterMetresPerSec() = default;
    constexpr explicit QuarterMetresPerSec(uint16_t q) : v(q) {}
};
struct MetresPerSec {
    int32_t v{0};
    constexpr MetresPerSec() = default;
    constexpr explicit MetresPerSec(int32_t m) : v(m) {}
};
constexpr MetresPerSec to_mps(QuarterMetresPerSec q) { return MetresPerSec(q.v / 4); }

struct Knots {
    int32_t v{0};
    constexpr Knots() = default;
    constexpr explicit Knots(int32_t kt) : v(kt) {}
};
struct KilometresPerHour {
    int32_t v{0};
    constexpr KilometresPerHour() = default;
    constexpr explicit KilometresPerHour(int32_t kmh) : v(kmh) {}
};

// INFO: fc 18sep26 1 m/s = 1.94384 kt = 3.6 km/h, rounded: truncated, 60 kt flown reads 59
constexpr Knots to_knots(QuarterMetresPerSec q) {
    return Knots((static_cast<int32_t>(q.v) * 194384 + 200000) / 400000);
}
constexpr KilometresPerHour to_kmh(QuarterMetresPerSec q) {
    return KilometresPerHour((static_cast<int32_t>(q.v) * 36 + 20) / 40);
}

struct EighthMetresPerSec {
    int16_t v{0};
    constexpr EighthMetresPerSec() = default;
    constexpr explicit EighthMetresPerSec(int16_t e) : v(e) {}
};

struct MillimetresPerSec {
    int32_t v{0};
    constexpr MillimetresPerSec() = default;
    constexpr explicit MillimetresPerSec(int32_t mm) : v(mm) {}
};
struct FeetPerMinute {
    int32_t v{0};
    constexpr FeetPerMinute() = default;
    constexpr explicit FeetPerMinute(int32_t f) : v(f) {}
};
constexpr FeetPerMinute to_feet_per_minute(MillimetresPerSec mm) {
    const int64_t scaled = static_cast<int64_t>(mm.v) * 19685;
    const int64_t half = scaled < 0 ? -50000 : 50000;
    return FeetPerMinute(static_cast<int32_t>((scaled + half) / 100000));
}

struct Cordic9 {
    uint16_t v{0};
    constexpr Cordic9() = default;
    constexpr explicit Cordic9(uint16_t c) : v(static_cast<uint16_t>(c & 0x1FF)) {}
};
struct Degrees {
    uint16_t v{0};
    constexpr Degrees() = default;
    constexpr explicit Degrees(uint16_t d) : v(d) {}
};
constexpr Degrees to_degrees(Cordic9 c) {
    return Degrees(static_cast<uint16_t>((static_cast<uint32_t>(c.v) * 45 + 32) >> 6) % 360);
}

struct MilliVolts {
    int16_t v{0};
    constexpr MilliVolts() = default;
    constexpr explicit MilliVolts(int16_t mv) : v(mv) {}
};

}

#endif
