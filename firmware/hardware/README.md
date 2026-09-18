# hardware

What fills the roles `ports/` declares. `parts/` is a chip and its datasheet, `platform/` is everything that comes from the silicon the board is soldered to, `io/` is the bus vocabulary the two meet over.

A part is written against `io::Spi`, `io::I2c` and `io::Uart`, never against Zephyr or against a platform. That is what lets `models/` stand in for the chip on the host and what makes `make test` exercise the SX1262 driver's real register writes.

## The platform contract

`boards/` is a template over a platform rather than a consumer of a base class, so a platform proves itself by compiling, not by overriding. The cost is that the contract is nowhere in the type system, which is what this section is for. A platform is what `platform/host/platform.h` and `platform/zephyr/platform.h` both are, and there are exactly two of them.

The buses a part is constructed over:

| Member | Answers |
|---|---|
| `spi(io::BusId)`, `i2c(io::BusId)`, `uart(io::BusId)` | the bus a part talks on |
| `uart_rate(io::BusId)` | the receiver's baud, so the GNSS driver can walk the candidates |
| `gpio()`, `wire(const PinMap&)` | the lines a part drives, and the board's map of them |

The ports a platform fills directly, handed to `ports::Roles` by the board: `clock()`, `link()`, `kv()`, `log_flash()`, `annunciator()`, `indicator()`, `dfu()`, `die_temperature()`.

The producers the board polls, which are not ports because nothing calls them on the core's behalf. What reaches a service is the event the board pushes:

| Member | Becomes |
|---|---|
| `read_pressure_mpa(uint32_t&)` | `events::BaroSample` |
| `read_battery_mv(uint16_t&)`, `external_power()` | `events::BatterySample` |
| `button_down()`, `pad_down()` | `events::ButtonEvent` |
| `pps()` | the PPS edge on `bus::State::clock` |

And what the board asks about the unit it is running on: `begin()`, `capabilities()`, `device_addr()`, `glass_rotation()`, `read_panel_signature()`, `buzzer_pin_held_low()`, `watchdog()`, `system_power()`.

A platform that is missing one of these fails at `platform/contract.h`, which both platforms assert themselves against at the bottom of their own header: the error names the member and points at the platform rather than at whichever call in `boards/` happened to need it first. No vtable is involved, the assertions are compile time and the table above is what the file spells out.
