# firmware

The C++ tree: `core/` is the portable logic, `hal/` the roles a product needs from a board, `hardware/` the parts and platforms that fill them, `ui/` the pages, `products/` the services that wire a device together, `boards/` the Zephyr board, `test/` the host suite, `simulator/` the world it flies in. Each directory carries its own README for what it decides.

`make test` runs the host suite, `make simulator` builds the terminal one, `scripts/build_local.sh` from the repo root builds the device image.

## Naming a boolean

A boolean field says which kind of fact it is in its suffix, never in a prefix. `has_` and `have_` both read as "somebody has something" and the two were two lines apart in one struct before this rule existed.

| Suffix | Fact | Example |
|---|---|---|
| `_valid` | the value beside it is a real reading, taken and trustworthy now | `fix_valid`, `lateral_valid`, `flight_time_valid` |
| `_fitted` | the hardware is on this unit, whatever it is currently reporting | `inclinometer_fitted` |

A state is neither, and takes the adjective the domain already uses: `airborne`, `charging`, `transmitting`, `flyable`, `battery_low`. A device that measures something it is not measuring right now is two facts and two fields, because a page draws them differently: a plain T-Echo has no inclinometer and prints none, a Plus whose sensor has not answered yet prints the empty cage.

`has_` survives as a verb on a container that is asked a question, where it is a call and not a field: `hal::has(capabilities, Capability::Rf)`, `Inventory::has_i2c_address(0x28)`.
