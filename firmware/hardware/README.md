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

## The two questions a pin and a register answer

`buzzer_pin_held_low()` and `read_panel_signature()` are the two facts only the board port can take, before any driver owns the pins, and both are read rather than declared because LilyGO fits more than one thing behind the same footprint.

The buzzer pin is read high-Z and then against the internal pull-up. Held down both times, something on the board is pulling it to ground, and on this hardware that something is the piezo's drive stage: our T-Echo Plus reads it low and sounds, our plain T-Echo has an empty pad, follows the pull-up up and has no buzzer to sound. The board concludes `Capability::Buzzer` from that reading alone (`boards/lilygo/t_echo_plus/board.h`), the same way it concludes `Capability::Vibro` from a DRV2605 answering at 0x5A. Until September 2026 the rule ran the other way round - a fitted buzzer was read as an empty pad - which is how a Plus flew silent while a plain T-Echo reported a buzzer it does not have.

The panel signature has two ways of missing, and `parts/ssd1681/panel.h` keeps them apart because different people fix them. `Panel::Unknown`, printed `NO ID`, is a fingerprint that could not be clocked out at all: the board port did not run, or the pins were not ready, and nothing was read. `Panel::Unlisted`, printed `UNLISTED`, is a fingerprint that was read and matches no row of SoftRF's table - which is the honest answer for a T-Echo Plus, whose row in that table carries a date string and no bytes. A bench that sees `UNLISTED` has a panel worth writing down; a bench that sees `NO ID` has a board that answered nothing.

A platform that is missing one of these fails at `platform/contract.h`, which both platforms assert themselves against at the bottom of their own header: the error names the member and points at the platform rather than at whichever call in `boards/` happened to need it first. No vtable is involved, the assertions are compile time and the table above is what the file spells out.
