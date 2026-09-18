#include "products/skyblip_go/settings.h"

#include <cstring>

#include "core/power/battery.h"
#include "core/settings/address.h"
#include "core/settings/blob.h"
#include "core/util/json_min.h"
#include "products/skyblip_go/pages/settings.h"

namespace skyblip::go {

namespace {

// The version-1 payload, byte for byte: the struct as it was declared when a
// device in the field last wrote its flash. Same members in the same order means
// the same layout, so a blob written then still decodes now.
struct SettingsV1 {
    uint8_t version{1};
    uint32_t device_addr{0};
    uint8_t addr_table{0};
    uint8_t aircraft_type{4};
    uint8_t region{0};
    bool alarm_enabled{true};
    uint8_t alarm_volume{3};
    bool stealth{false};
    Units units{Units::Metric};
    uint8_t rotation{0};
    uint8_t page_mask{0x0F};
    bool power_save{false};
    char callsign[kCallsignCap]{0};
};

// The version-2 payload, byte for byte: the same members version 3 has, less
// battery_offset_mv, in the order they were declared in. Same length as version
// 3 by accident of alignment, different layout by design, so nothing but the
// version byte tells them apart.
struct SettingsV2 {
    uint8_t version{1};
    uint32_t device_addr{0};
    uint8_t addr_table{0};
    uint8_t aircraft_type{4};
    bool alarm_enabled{true};
    uint8_t alarm_volume{3};
    bool stealth{false};
    Units units{Units::Metric};
    uint8_t page_mask{0x0F};
    char callsign[kCallsignCap]{0};
};

// The version-3 payload, byte for byte: the same members version 4 has, less
// freq_trim_e1_ppm.
struct SettingsV3 {
    uint8_t version{1};
    uint32_t device_addr{0};
    int16_t battery_offset_mv{0};
    uint8_t addr_table{0};
    uint8_t aircraft_type{4};
    bool alarm_enabled{true};
    uint8_t alarm_volume{3};
    bool stealth{false};
    Units units{Units::Metric};
    uint8_t page_mask{0x0F};
    char callsign[kCallsignCap]{0};
};

constexpr size_t kPayloadV1 = sizeof(SettingsV1);
constexpr size_t kPayloadV2 = sizeof(SettingsV2);
constexpr size_t kPayloadV3 = sizeof(SettingsV3);
constexpr size_t kPayload = sizeof(Settings);

void migrate_v1(const SettingsV1& old, Settings& out) {
    out = Settings{};
    out.version = Settings::kCurrentVersion;
    out.device_addr = old.device_addr;
    out.addr_table = old.addr_table;
    out.aircraft_type = old.aircraft_type;
    out.alarm_enabled = old.alarm_enabled;
    out.alarm_volume = old.alarm_volume;
    out.stealth = old.stealth;
    out.units = old.units;
    out.page_mask = old.page_mask;
    std::memcpy(out.callsign, old.callsign, kCallsignCap);
    out.callsign[kCallsignCap - 1] = 0;
}

// A unit that stored its settings before this firmware existed was never
// calibrated, so it comes back with no trim - which is the reading it has been
// giving all along, not a change of behaviour on an upgrade.
void migrate_v2(const SettingsV2& old, Settings& out) {
    out = Settings{};
    out.version = Settings::kCurrentVersion;
    out.device_addr = old.device_addr;
    out.addr_table = old.addr_table;
    out.aircraft_type = old.aircraft_type;
    out.alarm_enabled = old.alarm_enabled;
    out.alarm_volume = old.alarm_volume;
    out.stealth = old.stealth;
    out.units = old.units;
    out.page_mask = old.page_mask;
    out.battery_offset_mv = 0;
    std::memcpy(out.callsign, old.callsign, kCallsignCap);
    out.callsign[kCallsignCap - 1] = 0;
}

// A unit that stored its settings before the frequency trim existed was never
// measured on a spectrum analyser, so it comes back on the reference its TCXO
// actually has - the frequency it has been transmitting on all along, not a
// correction invented by an upgrade.
void migrate_v3(const SettingsV3& old, Settings& out) {
    out = Settings{};
    out.version = Settings::kCurrentVersion;
    out.device_addr = old.device_addr;
    out.battery_offset_mv = old.battery_offset_mv;
    out.addr_table = old.addr_table;
    out.aircraft_type = old.aircraft_type;
    out.alarm_enabled = old.alarm_enabled;
    out.alarm_volume = old.alarm_volume;
    out.stealth = old.stealth;
    out.units = old.units;
    out.page_mask = old.page_mask;
    out.freq_trim_e1_ppm = 0;
    std::memcpy(out.callsign, old.callsign, kCallsignCap);
    out.callsign[kCallsignCap - 1] = 0;
}

bool callsign_is_printable(const char* s) {
    for (size_t i = 0; i < kCallsignCap; i++) {
        if (s[i] == 0) return true;
        if (s[i] < 0x20 || s[i] > 0x7E) return false;
    }
    return false;
}

}  // namespace

Settings defaults(uint32_t addr) {
    Settings s;
    s.device_addr = settings::safe_device_address(addr);
    return s;
}

Status validate(const Settings& s) {
    if (s.version != Settings::kCurrentVersion) return Status::Unsupported;
    if (s.device_addr > settings::kAddressMask) return Status::OutOfRange;
    if (s.addr_table > 63) return Status::OutOfRange;
    if (s.aircraft_type > 17) return Status::OutOfRange;
    if (s.alarm_volume > 5) return Status::OutOfRange;
    if (s.battery_offset_mv > power::kCalibrationLimitMv ||
        s.battery_offset_mv < -power::kCalibrationLimitMv)
        return Status::OutOfRange;
    if (s.freq_trim_e1_ppm > kFreqTrimLimitTenthsPpm ||
        s.freq_trim_e1_ppm < -kFreqTrimLimitTenthsPpm)
        return Status::OutOfRange;
    if (!callsign_is_printable(s.callsign)) return Status::Invalid;
    return Status::Ok;
}

size_t blob_size() { return settings::blob_bytes(kPayload); }

void to_blob(const Settings& s, uint8_t* out, size_t cap) {
    settings::seal(kBlobVersion, &s, kPayload, out, cap);
}

Status from_blob(const uint8_t* in, size_t len, Settings& out) {
    if (len < settings::kBlobOverhead) return Status::Crc;

    const uint8_t version = settings::blob_version(in);
    if (version == kBlobVersion) {
        const Status st = settings::open(in, len, kPayload, &out);
        if (st != Status::Ok) return st;
    } else if (version == 3) {
        SettingsV3 old;
        const Status st = settings::open(in, len, kPayloadV3, &old);
        if (st != Status::Ok) return st;
        migrate_v3(old, out);
    } else if (version == 2) {
        SettingsV2 old;
        const Status st = settings::open(in, len, kPayloadV2, &old);
        if (st != Status::Ok) return st;
        migrate_v2(old, out);
    } else if (version == 1) {
        SettingsV1 old;
        const Status st = settings::open(in, len, kPayloadV1, &old);
        if (st != Status::Ok) return st;
        migrate_v1(old, out);
    } else {
        return Status::Unsupported;
    }
    // INFO: fc 18sep26 a unit that was showing every page keeps showing every page when one is
    // added
    if (out.page_mask == kPageMaskEveryPageBeforeSats) out.page_mask = kPageMaskAll;
    if (validate(out) != Status::Ok) return Status::Invalid;
    return Status::Ok;
}

void write_json_fields(json::Writer& w, const Settings& s) {
    w.kv_int("version", s.version);
    w.kv_int("addr", static_cast<long>(s.device_addr));
    w.kv_int("addr_table", s.addr_table);
    w.kv_int("aircraft_type", s.aircraft_type);
    w.kv_bool("alarm", s.alarm_enabled);
    w.kv_int("alarm_volume", s.alarm_volume);
    w.kv_bool("stealth", s.stealth);
    w.kv_int("units", static_cast<long>(s.units));
    w.kv_int("page_mask", s.page_mask);
    w.kv_str("callsign", s.callsign);
}

int to_json(const Settings& s, char* buf, int cap) {
    json::Writer w(buf, cap);
    write_json_fields(w, s);
    return w.finish();
}

Status apply_json(Settings& s, const char* json, int len) {
    json::Reader r(json, len);
    long v;
    bool b;
    Settings n = s;
    if (r.get_int("addr", v)) n.device_addr = static_cast<uint32_t>(v) & settings::kAddressMask;
    if (r.get_int("addr_table", v)) n.addr_table = static_cast<uint8_t>(v);
    if (r.get_int("aircraft_type", v)) n.aircraft_type = static_cast<uint8_t>(v);
    if (r.get_bool("alarm", b)) n.alarm_enabled = b;
    if (r.get_int("alarm_volume", v)) n.alarm_volume = static_cast<uint8_t>(v);
    if (r.get_bool("stealth", b)) n.stealth = b;
    if (r.get_int("units", v)) n.units = v ? Units::Metric : Units::Nautical;
    if (r.get_int("page_mask", v)) n.page_mask = static_cast<uint8_t>(v);
    // Narrowed before it is validated, not after: 65536 truncates to 0 in an
    // int16 and would pass a bound check that never saw the value sent.
    if (r.get_int("battery_offset_mv", v)) {
        if (v < -power::kCalibrationLimitMv || v > power::kCalibrationLimitMv)
            return Status::OutOfRange;
        n.battery_offset_mv = static_cast<int16_t>(v);
    }
    if (r.get_int("freq_trim_e1_ppm", v)) {
        if (v < -kFreqTrimLimitTenthsPpm || v > kFreqTrimLimitTenthsPpm) return Status::OutOfRange;
        n.freq_trim_e1_ppm = static_cast<int16_t>(v);
    }
    r.get_str("callsign", n.callsign, sizeof(n.callsign));
    n.device_addr = settings::safe_air_address(n.device_addr, n.addr_table);
    Status st = validate(n);
    if (st != Status::Ok) return st;
    s = n;
    return Status::Ok;
}

}  // namespace skyblip::go
