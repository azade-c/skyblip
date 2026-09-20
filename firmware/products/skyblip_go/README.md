# products/skyblip_go

One board, one service list, one set of pages. `product.h` wires it; the shell around it (`main.cpp` on silicon, the simulator on the host) only decides how often `step()` is called and where the pixels go.

`pages/` is the glass half of the product and carries its own README, `input/` is what a press means here. `glass.h` is where the panel this device draws on is named: the size comes from the board (`boards/lilygo/t_echo_plus/glass.h`), and `go::Glass` is the `ui::Panel` every page and the screen service is written against.

`settings.h` is the list of values a pilot of this device can change, its defaults, what it refuses and how an older flash layout migrates forward. `kBlobVersion` moves whenever the struct's bytes move, and `from_blob` still reads every layout before it: version 1 carried `region`, `rotation` and `power_save` between `aircraft_type` and `callsign`; version 2 dropped those three and had no `battery_offset_mv`; version 3 is the same length as version 2 and a different layout, which is exactly the case a length check cannot catch and the version byte must; version 4 added `freq_trim_e1_ppm`; version 5 dropped `page_mask`, which lost its last reader when the pad's walk became three pages and the two that are doors stopped being hideable (`pages/README.md`); version 6 added `gyro_enabled`; version 7 dropped `stealth` and `gyro_enabled` with the features themselves, and is the length version 3 was, which is the collision the version byte exists for; version 8 dropped `device_addr` and `addr_table`, because the identity is the silicon's and a stored copy of it is a copy that can disagree; version 9 added `battery_offset_manual`, so a trim a person measured is told apart from one the device learned off a charger (`core/power/README.md`), and the migration reads any stored version-8 trim as hand-set, because until then there was no other way to have one.

That is the rule the struct states and the reason it is versioned: a setting with no reader leaves the struct, the JSON and the schema together, rather than being stored and ignored. The framing under it is `core/settings/blob.h` and the identity rules are `core/settings/address.h`, because neither is this product's to decide. `settings_store.h` is the two calls `comms::ConfigService` needs to answer a phone, and it is the only thing that couples the companion link to this struct. It is also where the address reaches that reply: the store is handed `roles.device_addr` because the struct no longer carries one, so `addr` and `addr_table` are reported and nothing can set them. The settings do not live on `bus::State`: the blackboard is core's and a product's field list cannot sit on it, so the product owns them and hands a reference to the services that read them.

## What a service is

A service is one unit of work the loop steps, under the watchdog, in dataflow order. It owns one thing: a queue it drains, a slice of `bus::State` it writes, or an output port it drives. Never two of the same kind, and never a queue somebody else also drains.

That is why `nmea` is a service and not a helper, though it drains nothing and writes nothing: it drives `ports::Link` on a one-second cadence that has to defer around an armed burst, which needs its own place in the order and its own watchdog row. Something that consumes the finished product rather than being stepped by it is a reader, and `DiagnosticsDump` is the one the tree has.

Everything a service needs arrives at construction: the `runtime::Context` with the roles, the bus and the state, plus whatever else it is handed as a reference. There is no `attach_` after the fact and no member that starts null, because a service that can be half-built is a service every method has to check.

The order in `services_` is the dataflow, not a preference. `ownship` publishes the fix before `traffic` measures against it; `traffic` drains `bus.rf` and stamps the transmit before `radio` closes its deadline against it; `screen` goes last because it is the only service allowed to spend a whole pass on pixels.

## Features and capabilities

A capability is what the board has, probed at bring-up and granted from the devicetree (`ports/capabilities.h`). A feature is what this build claims to do (`features.h`), and every feature names the capabilities it needs in `kFeatureSpecs`. A service asks `supported(declared, capabilities)` in its `setup()`, so a claim the hardware cannot serve is off rather than broken.

A feature bit with no reader does not exist. The list is exactly the claims a service branches on, and a bit is added on the commit that reads it.

## The parts of a boot

`setup()` probes the board, classifies the reset, and answers one question before anything else: is this boot a device at all. A charger plugged into a unit in a flight bag goes straight back to SYSTEM OFF with nothing painted and no service started (`core/power/wake.h`).

What survives a failed self test is a device that still shows why: the page stays on the glass, the button still works, and the loop refuses to fly. That is `flyable_`, and it is false when a required capability is missing.

## What the glass wears while the device is off

E-paper holds its last image with the rails down, so the frame pushed immediately before the rails drop is what the device wears in a flight bag for as long as it sits there. There are four, and `product.h` picks between them by why the device is going down.

The wordmark is the ordinary one: a long press, or the companion link asking. A stow, which is the long press with the pad held, leaves the glass blank instead, because months of one image is ghosting an e-paper never fully loses. An install says so, because the bootloader is about to take the device and a pilot watching a blank panel would think it had died.

The fourth is the cell, and it is the only one that says something happened rather than naming a state. `SWITCHED OFF`, `FLAT BATTERY`, `PLUG IN THEN PRESS`, stacked halfway between the mark and the bottom of the glass. All three lines earn their place: the device turned itself off, which a pilot who did not press anything has to be told; the reason, because the alternative guesses are a crash and a dead device; and the way back, which is two acts and not one.

The last line is worded that way because a cable alone gives nobody a device. VBUS does wake this SoC out of SYSTEM OFF, and the reset cause carries the bit, but `core/power/wake.h` refuses that boot on purpose: a charger found in a flight bag must not switch a device on. What the cable does is charge the cell and, through the refusal, leave the button armed, so the press that follows is the pilot asking for a device and gets one even while the cell is still filling (`core/power/wake.h`, `button_wake_after_refusal`). A low-battery shutdown withholds the wake pin until then, so the press before the cable does nothing at all.

What this frame does not carry is the percentage. It would be the reading the device died at, frozen at zero, standing there unchanged through the whole charge that follows, and a figure that cannot update is a figure that lies the moment it matters.
