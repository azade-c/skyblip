# core/settings

Two things that are not a product's to decide: the identity this device may claim on the air, and the framing its settings survive a power cycle in.

`address.h` is ADS-L's, not ours. It holds the 24-bit identity this device claims on the air and the table that says which space that identity was drawn from.

Issue 2 F.2.2 gives the table 64 entries: 0 is random/privacy, 1 to 4 are reserved, 5 ICAO, 6 FLARM, 7 OGN-Tracker, 8 FANET, and 9 upwards are manufacturer pages, each one an 8-bit prefix plus a 16-bit base address that `registry@ads-l.aero` assigns and the manufacturer is required to fill densely. That is where a shipped product belongs, and a 16-bit base address has to be numbered at production: derive it from silicon and two devices in three hundred share it.

Until that page exists we transmit table 7 and take the address the way an OGN tracker takes it, the low 24 bits of `DEVICEID[0]` (`oss/nrf52-ogn-tracker/src/main.cpp:56-58`). Same silicon, same number, so a board that ran that firmware and now runs this one is one aircraft rather than two, and the rest of the OGN population is drawn from the same uniform 24 bits, which is the only claim about collisions anyone can honestly make here.

So there is no prefix arithmetic left. SoftRF moves a chip id off 0xD0, 0xDD, 0xDE, 0xDF and 0x11 (`src/system/SoC.cpp`, `DevID_Mapper`) because those blocks are FLARM's own device range and FANET's manufacturer registry, where a vendor numbering sequentially fills 65536 slots and a random arrival collides with probability far above the flat 1-in-16.7-million. That reasoning is sound and it stops applying the moment the table says which space we drew from: its remap sends 0xD0 to 0xE0, which the same registry assigns to OGN Tracker.

None of this is a pilot's to change. `stamp_identity` writes the address and the table over whatever a stored blob holds, on every boot, so a settings file written by an older firmware cannot name the aircraft and neither can the companion link: `apply_json` refuses a patch that moves either one, and accepts one that echoes what `get` reported. The cost is F.2.3, the ICAO override, which this device does not offer and `test/adsl/test_presentation.cpp` states as a skipped case rather than a silent gap.

What survives is the pair of values that mean something else to every decoder that reads one: 0x000000 is "no address" and 0xFFFFFF is what a dead chip-id read produces. `air_address` answers either with `kFallbackAddress`, and it is called last, where the address goes on the air, because a stored blob can hold what a chip id never would.

`blob.h` is a version byte, a payload and a CRC32 over both. It knows nothing about fields: what a product stores, what it defaults to, what it refuses and how an older layout migrates forward are the product's, in `products/skyblip_go/settings.{h,cpp}`. That is where `units`, `aircraft_type` and `callsign` belong, because they are what a pilot of that device can change and what `schemas/config.v1.schema.json` pins.

The split is what lets two products keep different settings without two copies of the framing, and it is why the CRC is tested here against a payload that is not anybody's `Settings`.

The companion link's side of the same line is `core/comms/config_store.h`: `comms::ConfigService` speaks JSON in and JSON out, and the product hands it the two calls that turn its own struct into that.
