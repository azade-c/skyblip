#include "core/flight/atmosphere.h"

#include <algorithm>

namespace skyblip::flight {

namespace {

// ISA troposphere barometric formula, tabulated:
//   h = 44330.77 * (1 - (p/101325)^0.190263)   [ICAO standard atmosphere]
// 500 Pa steps, altitude in centimetres. Worst linear-interpolation error
// between entries is 24 cm, which is under the noise of any real sensor.
constexpr uint32_t kLoPa = 26000;
constexpr uint32_t kStepPa = 500;

constexpr int32_t kAltCm[] = {
    1010851, 998426, 986189, 974134, 962256, 950548, 939005, 927622, 916395, 905318, 894387, 883597,
    872946,  862429, 852041, 841781, 831644, 821626, 811726, 801939, 792264, 782696, 773235, 763876,
    754617,  745457, 736393, 727422, 718543, 709753, 701051, 692434, 683900, 675449, 667078, 658785,
    650569,  642428, 634361, 626367, 618443, 610588, 602802, 595082, 587428, 579838, 572312, 564847,
    557443,  550099, 542813, 535585, 528414, 521298, 514237, 507230, 500275, 493373, 486521, 479720,
    472968,  466265, 459610, 453002, 446440, 439924, 433452, 427025, 420642, 414302, 408003, 401747,
    395531,  389356, 383221, 377126, 371068, 365050, 359068, 353124, 347217, 341345, 335510, 329709,
    323943,  318212, 312514, 306849, 301218, 295619, 290052, 284516, 279012, 273539, 268096, 262683,
    257300,  251947, 246622, 241326, 236059, 230819, 225607, 220423, 215265, 210134, 205030, 199951,
    194899,  189872, 184870, 179893, 174940, 170013, 165109, 160229, 155373, 150540, 145730, 140943,
    136179,  131437, 126717, 122019, 117342, 112688, 108054, 103442, 98850,  94279,  89728,  85198,
    80687,   76197,  71726,  67274,  62842,  58428,  54034,  49658,  45301,  40962,  36641,  32338,
    28053,   23786,  19536,  15304,  11088,  6890,   2709,   -1456,  -5604,  -9735,  -13851, -17950,
    -22033,  -26100, -30152, -34188, -38208, -42214, -46204, -50178, -54138, -58084, -62014, -65930,
    -69831,
};

constexpr int kN = static_cast<int>(sizeof(kAltCm) / sizeof(kAltCm[0]));
constexpr uint32_t kHiPa = kLoPa + static_cast<uint32_t>(kN - 1) * kStepPa;

constexpr uint32_t kMilli = 1000;
constexpr uint32_t kLoMpa = kLoPa * kMilli;
constexpr uint32_t kHiMpa = kHiPa * kMilli;
constexpr uint32_t kStepMpa = kStepPa * kMilli;

constexpr int32_t alt_mm_at(int i) { return kAltCm[i] * 10; }

}  // namespace

int32_t pressure_to_alt_mm(uint32_t mpa) {
    if (mpa <= kLoMpa) return alt_mm_at(0);
    if (mpa >= kHiMpa) return alt_mm_at(kN - 1);

    const uint32_t off = mpa - kLoMpa;
    const int i = static_cast<int>(off / kStepMpa);
    const uint32_t frac = off % kStepMpa;
    if (frac == 0) return alt_mm_at(i);

    // Table descends with pressure, so the step is negative. Interpolate on it.
    const int64_t span = alt_mm_at(i + 1) - alt_mm_at(i);
    return alt_mm_at(i) + static_cast<int32_t>((span * frac) / kStepMpa);
}

int32_t pressure_to_alt_cm(uint32_t pa) {
    if (pa >= kHiPa) return kAltCm[kN - 1];
    return pressure_to_alt_mm(pa * kMilli) / 10;
}

uint32_t alt_mm_to_pressure_mpa(int32_t alt_mm) {
    if (alt_mm >= alt_mm_at(0)) return kLoMpa;
    if (alt_mm <= alt_mm_at(kN - 1)) return kHiMpa;

    uint32_t lo = kLoMpa, hi = kHiMpa;
    while (hi - lo > 1) {
        const uint32_t mid = lo + (hi - lo) / 2;
        if (pressure_to_alt_mm(mid) > alt_mm)
            lo = mid;  // still too high up: raise the pressure
        else
            hi = mid;
    }
    return hi;
}

uint32_t alt_cm_to_pressure(int32_t alt_cm) {
    return (alt_mm_to_pressure_mpa(alt_cm * 10) + kMilli / 2) / kMilli;
}

int32_t alt_cm_on_setting(uint32_t pa, uint32_t setting_pa) {
    return pressure_to_alt_cm(pa) - pressure_to_alt_cm(setting_pa);
}

bool qnh_from_alt(uint32_t pa, int32_t alt_msl_cm, uint32_t& out_pa) {
    const uint32_t qnh = alt_cm_to_pressure(pressure_to_alt_cm(pa) - alt_msl_cm);
    if (qnh < kQnhMinPa || qnh > kQnhMaxPa) return false;
    out_pa = qnh;
    return true;
}

bool climb_mm_s_from_alt(int32_t alt_mm_now, int32_t alt_mm_then, uint32_t dt_ms,
                         int32_t& out_mm_s) {
    if (dt_ms < kMinWindowMs || dt_ms > kMaxWindowMs) return false;

    const int64_t d_mm = static_cast<int64_t>(alt_mm_now) - alt_mm_then;
    out_mm_s = static_cast<int32_t>((d_mm * 1000) / static_cast<int64_t>(dt_ms));
    return true;
}

int16_t climb_e8_from_mm_s(int32_t mm_s) {
    const int64_t eighths = static_cast<int64_t>(mm_s) * 8;
    int64_t e8 = (eighths >= 0 ? eighths + 500 : eighths - 500) / 1000;
    e8 = std::clamp<int64_t>(e8, -32768, 32767);
    return static_cast<int16_t>(e8);
}

}  // namespace skyblip::flight
