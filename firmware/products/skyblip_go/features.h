// products/skyblip_go/features.h: what this product claims to do, as a bitset a
// service is constructed with.
//
// It lives apart from products/skyblip_go/product.h so a service can be handed
// the claim it implements without including the product that owns it. A feature
// with no reader is a line of marketing inside a header: the list said
// CompanionLink for as long as the tree had a NMEA characteristic nothing ever
// wrote to, and nothing anywhere could tell.
#ifndef SKYBLIP_PRODUCTS_SKYBLIP_GO_FEATURES_H
#define SKYBLIP_PRODUCTS_SKYBLIP_GO_FEATURES_H

#include <cstdint>

#include "ports/capabilities.h"

namespace skyblip::go {

enum class Feature : uint32_t {
    None = 0,
    UplinkRx = 1u << 0,
    CompanionLink = 1u << 1,
};

constexpr Feature kFeatures = static_cast<Feature>(static_cast<uint32_t>(Feature::UplinkRx) |
                                                   static_cast<uint32_t>(Feature::CompanionLink));

// Named apart from ports::has so a service reading both cannot pick the wrong one.
constexpr bool has_feature(Feature declared, Feature one) {
    return (static_cast<uint32_t>(declared) & static_cast<uint32_t>(one)) != 0;
}

struct FeatureSpec {
    Feature feature;
    ports::Capabilities needs;
};

constexpr FeatureSpec kFeatureSpecs[] = {
    {Feature::UplinkRx, ports::Capability::Rf},
    {Feature::CompanionLink, ports::Capability::Link},
};

constexpr Feature supported(Feature declared, ports::Capabilities capabilities) {
    uint32_t kept = static_cast<uint32_t>(declared);
    for (const FeatureSpec& spec : kFeatureSpecs)
        if (!ports::has(capabilities, spec.needs)) kept &= ~static_cast<uint32_t>(spec.feature);
    return static_cast<Feature>(kept);
}

}  // namespace skyblip::go

#endif
