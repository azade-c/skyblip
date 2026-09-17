# core/flight

What the aircraft is doing, decided from the fix stream and the barometer. Pure, testable, and free of every framework header: the services in `products/` feed these and publish what they answer.

| File | What it decides |
|---|---|
| `state` | airborne or on the ground, which gates the DFU lockout and the transmit rate |
| `ground` | the same answer with a landing held back, which is what a permission gate reads |
| `timer` | how long this flight has been running |
| `atmosphere` | the standard atmosphere as integer math: pressure altitude, subscales, vertical speed |
| `turn` | rate of turn from two reported tracks |
| `extrapolate` | where an aircraft will be, for the transmitter and the alarm |
| `log_record`, `log_session` | what a flight leaves behind, and when a session runs |

## atmosphere

Pressure is the only altitude source that is datum-free in a useful way: a rate derived from it needs no agreement with anyone. That is what the module is for. It is deliberately not the altitude skyBlip broadcasts, because the collision alarm compares relative altitude against other aircraft (`core/traffic/alarm.h`, a 300 m window), so every participant must share one datum and which one belongs to the ADS-L spec, not to us.

The curve is the ICAO troposphere formula tabulated at 500 Pa steps and interpolated, in millipascals to millimetres. Those units are not ambition, they are what the part resolves and where the resolution goes:

| Link | Resolution | As altitude |
|---|---|---|
| BME280 at the driver's x16 / x2 oversampling and IIR 4 | 0.5 Pa RMS noise | 4 cm |
| what Zephyr's `sensor_value` carries | 1/256 Pa | 1.5 cm |
| `pressure_to_alt_mm` | 1 mPa | 0.008 mm |
| table interpolation against the closed form | | under 25 cm, a bias, not jitter |
| `climb_e8_from_mm_s`, the ADS-L G.1.9 unit | 0.125 m/s | 24.6 ft/min of rate |

The chain used to truncate the driver's reading to a whole pascal, which is nine centimetres at sea level, twice the sensor's own noise and forty times what the driver handed over: over a one-second window that is 17 ft/min of quantisation invented after the measurement. Millipascals cost nothing here - the table lookup is the same two loads and a multiply - and what they buy is a vertical speed whose error is the barometer's rather than ours.

The rate is therefore measured in mm/s and encoded to eighths of a metre per second only where the radio needs it (`core/protocol/adsl.cpp`). Every screen, and the `$LK8EX1` vario, reads the measurement. `kMinWindowMs` and `kMaxWindowMs` bound the interval a rate may be taken over: too short and the sensor noise is the answer, too long and it is history.

The barometer is sampled once a second on the PPS edge (`boards/lilygo/t_echo_plus/board.h`), so the second a rate is taken over is the second the fix stream is dated in. That needs the part in forced mode: in Zephyr's default normal mode the chip converts on its own standby timer and a read returns a sample of unknown age, which is the one error differentiating over that second cannot survive.

## ground

`state` answers one question about one solution, and the bus carries that answer as the ADS-L G.1.4 code in `own.flight_state`. `state_from` reads it back, and it is the only place that does: G.1.4 is two bits and we own neither the sender nor the future, so a code this build does not name is `Unknown`, which every gate refuses.

`GroundLatch` is the second question, the one a door asks: may this device be written to, erased, updated. `Unknown` is not a ground - a device that has never had a fix has not proven anything - and once `Airborne` has been seen, only a positive `OnGround` clears it. A fix lost in flight is therefore never a landing: without the latch, a receiver dropping out over a ridge would unlock the firmware update mid-flight, which is the same failure the hold in `state` exists to prevent one layer down.

There is one latch, owned by the service that owns the monitor (`products/skyblip_go/services/ownship.cpp`) and published as `state.confirmed_flight_state`. The DFU and settings gate in `core/comms/config.h` and the flight log's offload gate both read that one value rather than deriving a second opinion or asking each other, so "on the ground" means the same thing to all three.

## timer

`FlightTimer` counts from the takeoff `state.h` declares, and from no other event. One takeoff, so the clock on the glass, the log session on flash and the transmit rate cannot disagree about when the flight began. The five seconds `kTakeoffHoldMs` costs are invisible at the minute resolution the six-pack reads it in.

It freezes at the landing rather than clearing, and the next takeoff carries on from the figure it froze at. The count is airborne time since the device was switched on, not time since the last takeoff: a circuit detail or a touch and go is one line in a logbook, and a pilot should not have to add up the legs the state machine happened to split the day into. It also makes a false landing cheap. A glider bouncing on a ridge that loses `Airborne` for twenty seconds costs those twenty seconds, where a resetting clock would cost the whole flight.

Nothing but a takeoff starts it and nothing but switching the device off clears it.

`FlightState::Unknown` is not a landing. It is a solution the receiver could not give, so the timer keeps its takeoff instant and picks the count up again when the fixes come back, exactly as `FlightMonitor` holds its own state through the same outage. Without that, a minute under a wing in the circuit would restart the flight.

`flown()` separates "no flight yet this power cycle" from "a flight zero minutes old", which are the same number and not the same answer: the page draws its no-answer dashes for the first and `0:00` for the second. `running()` is the other half the page needs, and it is not `own.flight_state`: that one reports `Unknown` on every bad solution, so a title driven from it would flicker between states in an outage.

A device switched on in the air is timed from the first solution rather than from the wheels, because `FlightMonitor` declares `Airborne` immediately in that case and nothing on board saw the takeoff. The alternative is withholding the number from exactly the pilot who has been flying longest.

The count is a difference of unsigned milliseconds, so the 49.7-day wrap of `ports::Clock::millis()` is one ordinary second of flight.
