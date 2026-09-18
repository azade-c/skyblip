# core/settings

Two things that are not a product's to decide: the identity this device may claim on the air, and the framing its settings survive a power cycle in.

`address.h` is ADS-L's, not ours. A chip id is a serial number and some of the space it lands in is already crowded by other trackers that mint their identity the same way, so this is the one place that decides where we may not sit.

`blob.h` is a version byte, a payload and a CRC32 over both. It knows nothing about fields: what a product stores, what it defaults to, what it refuses and how an older layout migrates forward are the product's, in `products/skyblip_go/settings.{h,cpp}`. That is where `units`, `aircraft_type` and `callsign` belong, because they are what a pilot of that device can change and what `schemas/config.v1.schema.json` pins.

The split is what lets two products keep different settings without two copies of the framing, and it is why the CRC is tested here against a payload that is not anybody's `Settings`.

The companion link's side of the same line is `core/comms/config_store.h`: `comms::ConfigService` speaks JSON in and JSON out, and the product hands it the two calls that turn its own struct into that.
