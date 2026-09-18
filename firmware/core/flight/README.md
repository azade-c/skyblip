# core/flight

What the aircraft is doing, decided from the fix stream and the barometer. Pure, testable, and free of every framework header: the services in `products/` feed these and publish what they answer.

| File | What it decides |
|---|---|
| `state` | airborne or on the ground, which gates the DFU lockout and the transmit rate |
| `ground` | the same answer with a landing held back, which is what a permission gate reads |
| `timer` | how long this flight has been running |
| `atmosphere` | the standard atmosphere as integer math: pressure altitude, subscales, vertical speed |
| `turn` | rate of turn from two reported tracks |
| `slip` | where the ball hangs, from the acceleration the case measures |
| `extrapolate` | where an aircraft is now, when the fix it came from is older than now |
| `arc` | where an aircraft will be, out to the look-ahead the radar draws and the alarm grades |
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

## slip

The turn coordinator's ball, from the only sensor on this device that measures a force: the BHI260AP (`hardware/parts/bhi260/`). Two things happen here, and both are the instrument's rather than the sensor's.

The first is the geometry. A ball in a curved tube hangs along the resultant of gravity and the aircraft's acceleration, so what it shows is the lateral component of the specific force **as a fraction of that resultant**, not the lateral axis on its own: the same rudder mistake in a 2 g turn moves the ball half as far, because the resultant it hangs from is twice as heavy. `slip_from_specific_force` is that fraction in thousandths of g, which is the unit `sixpack` draws with and 200 of which is the full travel its cage allows.

The sign is the reading, and it is the opposite of the axis. A case accelerating left - a left turn with too little rudder - measures a leftward specific force, and the ball, free to slide, goes right: the pilot steps on the right rudder. So the ball's deflection is minus the lateral force, and the page's `lateral_mg` is a ball position, never an accelerometer reading.

Below 200 mg of resultant there is nothing to hang from, and the reading is refused rather than scaled: in free fall a real ball floats, and a fraction of nothing is noise at full amplitude.

The second is the damping. A real ball is a mass in a damped tube and it does not chatter; this one is sampled at 12.5 Hz and drawn on e-paper, where a jittering figure costs a partial refresh a second for nothing. `SlipBall` is a first-order filter over eight samples, about two thirds of a second, which settles to within a pixel of a step and turns turbulence into a ball that leans rather than one that rattles. `valid()` expires two seconds after the last sample, so a hub that stops answering takes the ball off the glass instead of freezing it somewhere plausible.

## arc

`extrapolate` and `arc` answer the same question over two horizons, and they are deliberately not one function. `extrapolate` closes the gap between the instant a fix was solved and the instant a position is used, so its ceiling is `kMaxExtrapolationMs`, a little over a second: past that the transmitter would be putting an invented position on the air, and §G.1.16 already refuses a solution older than 500 ms. `arc` is asked about a future nobody has to stand behind. It is what the radar's leader line draws and what `core/traffic/conflict` measures a breach against, so it runs to a minute and carries no obligation to the radio at all. Widening the first to serve the second was the tempting mistake.

The model is the same one, OGN's: constant ground speed on a constant turn rate, the turn applied half before the step and half after, which keeps a circling aircraft on its arc instead of on the tangent. A path is walked, never indexed: `Arc::advance()` rotates the velocity vector by half a step's worth of turn, moves, and rotates again, so a whole minute costs two sine lookups per aircraft rather than two per sample. Position is carried in millimetres and rotations are rounded rather than truncated, because a Q14 rotation applied thirty times in a row loses a metre a step otherwise.

A target's turn rate is not on the wire. ADS-L carries position, speed, track and climb, so `motion_of(obs, ...)` takes the rate `core/traffic` estimated from that target's own track history, and flies it straight when there is no estimate yet. Both the screen and the alarm read the same estimate, which is the whole point: a leader line that curves one way while the alarm grades the other is two models and one of them is wrong.

`kMaxTurnDps` is 30. Above it the number is describing the receiver rather than the aircraft: a differentiated 1 Hz track produces tens of degrees per second out of a bad fix, and no aeroplane this device rides in holds that rate for the length of the projection. The clamp is applied where a motion is built, so nothing downstream has to remember it.

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
