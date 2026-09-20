#include "core/store/sector.h"

#include "core/fec/crc.h"

namespace skyblip::store {

namespace {

constexpr uint32_t kChecksummedBytes = kSectorHeaderBytes - 2;

void put_u16(uint8_t* out, uint16_t v) {
    out[0] = static_cast<uint8_t>(v);
    out[1] = static_cast<uint8_t>(v >> 8);
}

void put_u32(uint8_t* out, uint32_t v) {
    out[0] = static_cast<uint8_t>(v);
    out[1] = static_cast<uint8_t>(v >> 8);
    out[2] = static_cast<uint8_t>(v >> 16);
    out[3] = static_cast<uint8_t>(v >> 24);
}

uint16_t get_u16(const uint8_t* raw) {
    return static_cast<uint16_t>(raw[0] | (static_cast<uint16_t>(raw[1]) << 8));
}

uint32_t get_u32(const uint8_t* raw) {
    return static_cast<uint32_t>(raw[0]) | (static_cast<uint32_t>(raw[1]) << 8) |
           (static_cast<uint32_t>(raw[2]) << 16) | (static_cast<uint32_t>(raw[3]) << 24);
}

bool owner_known(uint8_t code) {
    return code == static_cast<uint8_t>(SectorOwner::Flights) ||
           code == static_cast<uint8_t>(SectorOwner::Diagnostics);
}

}  // namespace

void encode_sector_header(const SectorHeader& header, uint8_t* out) {
    put_u16(out + 0, kSectorMagic);
    out[2] = header.version;
    out[3] = static_cast<uint8_t>(header.owner);
    put_u32(out + 4, header.sequence);
    put_u32(out + 8, header.session_id);
    // INFO: fc 20sep26 written down, not assumed: a later codec's sectors stay walkable
    out[12] = header.record_bytes;
    out[13] = header.session_start ? kSectorFlagSessionStart : 0;
    put_u16(out + kChecksummedBytes, fec::crc16_ccitt(out, kChecksummedBytes));
}

Status decode_sector_header(const uint8_t* raw, SectorHeader& out) {
    if (erased(raw, kSectorHeaderBytes)) return Status::Empty;
    if (get_u16(raw + 0) != kSectorMagic) return Status::Invalid;
    if (fec::crc16_ccitt(raw, kChecksummedBytes) != get_u16(raw + kChecksummedBytes))
        return Status::Crc;

    out = SectorHeader{};
    out.version = raw[2];
    out.sequence = get_u32(raw + 4);
    out.session_id = get_u32(raw + 8);
    out.record_bytes = raw[12];
    out.session_start = (raw[13] & kSectorFlagSessionStart) != 0;
    if (out.version != kSectorVersion || !owner_known(raw[3])) return Status::Unsupported;
    out.owner = static_cast<SectorOwner>(raw[3]);
    return Status::Ok;
}

bool erased(const uint8_t* raw, uint32_t len) {
    for (uint32_t i = 0; i < len; i++)
        if (raw[i] != 0xFF) return false;
    return true;
}

}  // namespace skyblip::store
