#ifndef SKYBLIP_CORE_SETTINGS_BLOB_H
#define SKYBLIP_CORE_SETTINGS_BLOB_H

#include <cstddef>
#include <cstdint>

#include "core/util/result.h"

namespace skyblip::settings {

constexpr size_t kBlobOverhead = 1 + 4;

constexpr size_t blob_bytes(size_t payload) { return payload + kBlobOverhead; }

constexpr uint8_t blob_version(const uint8_t* in) { return in[0]; }

void seal(uint8_t version, const void* payload, size_t payload_len, uint8_t* out, size_t cap);

Status open(const uint8_t* in, size_t len, size_t payload_len, void* payload_out);

}  // namespace skyblip::settings

#endif
