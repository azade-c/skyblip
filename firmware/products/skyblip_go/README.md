# products/skyblip_go

One board, one service list. `product.h` wires it; the shell around it (`main.cpp` on silicon, the simulator on the host) only decides how often `step()` is called and where the pixels go.

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
