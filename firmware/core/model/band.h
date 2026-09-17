#ifndef SKYBLIP_CORE_MODEL_BAND_H
#define SKYBLIP_CORE_MODEL_BAND_H

#include <cstdint>

namespace skyblip::model {
// The two halves of the second the single radio is shared between: ADS-L 4
// SRD-860 issue 2 §C.2 puts two 200 kHz channels on the M band and §C.4 one HDR
// channel on the O band. It lives here rather than in core/timing because it
// travels with a received burst: the dwell that heard one knows which band it
// was tuned to, and everything downstream would otherwise have to guess.
enum class Band : uint8_t { M, O };

}  // namespace skyblip::model

#endif
