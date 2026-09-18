# core/flight

What the aircraft is doing, decided from the fix stream and the barometer. Pure, testable, and free of every framework header: the services in `products/` feed these and publish what they answer.

| File | What it decides |
|---|---|
| `state` | airborne or on the ground, which gates the DFU lockout and the transmit rate, and stopped or rolling while it is on it |
| `ground` | the same answer with a landing held back, which is what a permission gate reads |
| `timer` | how long this flight has been running |
| `atmosphere` | the standard atmosphere as integer math: pressure altitude, subscales, vertical speed |
| `turn` | rate of turn from two reported tracks |
| `rate` | rate of turn from the gyroscope, about the earth's vertical, and the zero it is read from |
| `bank` | how far the wings are over, which a turning accelerometer cannot say on its own |
| `slip` | where the ball hangs, from the acceleration the case measures |
| `gload` | what the airframe is pulling, now and at its worst |
| `extrapolate` | where an aircraft is now, when the fix it came from is older than now |
| `arc` | where an aircraft will be, out to the look-ahead the radar draws |
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

## rate

`turn` differentiates the GNSS track and that is the instrument this device flew with first. It has two faults a gyroscope does not: the track is a cordic9 word, 45/64 of a degree, differenced over a second, so the smallest turn it can express is 0.7 deg/s, and the number is always a second old. It is also a *track* rate, so a changing wind moves it without the aircraft turning.

The gyroscope measures the airframe's own rotation, twenty times a second and with no lag. What it does not measure is the instrument: a turn coordinator reads the rate about the earth's vertical, and the airframe's yaw axis only points that way with the wings level. `vertical_rate_cdps` projects the body rates onto the direction the accelerometer calls down, which is the same arithmetic the ball uses for gravity and is exact wherever the wings are. Below 200 mg of resultant there is no down to project onto and the answer is refused, as in `slip`.

What a MEMS gyroscope cannot do is hold a zero. Its offset walks with temperature, and a degree a second of it is a third of a standard rate turn. `TurnRate` keeps a single trim on the projected rate and learns it from whatever the caller can prove: standing still, where the truth is zero, and in flight, where the truth is the GNSS track rate that `turn` already computes. The time constant is 64 samples, about thirteen seconds at the 5 Hz the hub is drained at, which is slow enough that a real turn passes through untouched and fast enough that a walking zero never reaches the glass.

That trim is also the fallback. A gyroscope reporting nothing while the aircraft turns is dragged onto the track rate within those thirteen seconds, so the instrument degrades to the one this device had before rather than to a lie. A part that is not fitted at all never produces a sample, `valid()` stays false, and `products/skyblip_go/services/ownship` publishes the GNSS rate unchanged.

`own.turn_cdps` is what the gyroscope is worth: hundredths, where `own.turn_dps` rounds to whole degrees a second for the ADS-L extrapolation and the alarm's arcs, which is all those need.

## bank

An accelerometer in a coordinated turn measures nothing useful about bank. The specific force lines up with the aircraft's own vertical - that is what coordinated means, and it is why the ball sits centred at 45 degrees - so levelling on gravity says wings level at every bank there is. Integrating the gyroscope instead drifts away within the minute. Neither sensor alone is an attitude reference, and this is the oldest trap in the subject.

What breaks the tie is the GNSS. The force the turn generates is the ground speed times the rate of turn, `centripetal_mg`, and subtracting it from the measured specific force leaves the gravity vector the aircraft would have felt standing still. The subtraction is done in the frame the filter already believes it is in, so the sines and cosines come from the held bank, which makes the correction exact for a steady turn rather than a small-angle approximation of one.

`BankAngle` is then the usual complementary filter: the roll rate integrated for the fast movement, the corrected gravity blended in over eight samples for the truth. The limit is 90 degrees, past which the arithmetic wraps and the instrument this feeds has clamped at 60 anyway, and the answer expires two seconds after the last sample like every other inertial reading here.

Two honesties are the caller's, not this file's. The bank is the *device's*, so a unit that is not aligned with the airframe reads its own mounting, which is the assumption the ball already makes. And without a fix there is no centripetal correction, so `products/skyblip_go/services/ownship` publishes the bank as valid only while the fix is.

## gload

`GMeter` is the sensor with nothing done to it but a sign: the largest and smallest of each axis held, in thousandths of g, with no fusion, no assumption and no fix required. It is the one instrument here that cannot be wrong about anything but its own calibration.

The sign is the instrument's, not the accelerometer's, and it is the ball's. Normal load is the load factor every g-meter shows, positive up, 1 g in level flight. The two horizontal axes are the direction the load throws a loose object, which is minus the measured force: a force to the right throws the ball, the pilot and the marker to the left. Reading them as raw axes instead would put the page and the turn coordinator in opposite languages on the same device, which is exactly how a bench came to read the whole page as inverted.

The peaks reset when the flight timer starts, so they are this flight's, and a unit left on the bench holds what the bench did to it.

## arc

`extrapolate` and `arc` answer the same question over two horizons, and they are deliberately not one function. `extrapolate` closes the gap between the instant a fix was solved and the instant a position is used, so its ceiling is `kMaxExtrapolationMs`, a little over a second: past that the transmitter would be putting an invented position on the air, and §G.1.16 already refuses a solution older than 500 ms. `arc` is asked about a future nobody has to stand behind. It is what the radar's leader line draws, so it runs to a minute and carries no obligation to the radio at all. Widening the first to serve the second was the tempting mistake.

The model is the same one, OGN's: constant ground speed on a constant turn rate, the turn applied half before the step and half after, which keeps a circling aircraft on its arc instead of on the tangent. A path is walked, never indexed: `Arc::advance()` rotates the velocity vector by half a step's worth of turn, moves, and rotates again, so a whole minute costs two sine lookups per aircraft rather than two per sample. Position is carried in millimetres and rotations are rounded rather than truncated, because a Q14 rotation applied thirty times in a row loses a metre a step otherwise.

A target's turn rate is not on the wire. ADS-L carries position, speed, track and climb, so `motion_of(obs, ...)` takes the rate `core/traffic` estimated from that target's own track history, and flies it straight when there is no estimate yet. The screen draws that estimate, and the tag beside a turning target is the same number the table holds.

`kMaxTurnDps` is 30. Above it the number is describing the receiver rather than the aircraft: a differentiated 1 Hz track produces tens of degrees per second out of a bad fix, and no aeroplane this device rides in holds that rate for the length of the projection. The clamp is applied where a motion is built, so nothing downstream has to remember it.

## state

Airborne or on the ground, from the fix stream alone. It is not a display value: it gates the DFU lockout and the transmit rate, so the two ways of being wrong are not symmetric. Declaring a takeoff that did not happen locks the update out on the ground and drains the cell at the airborne burst rate; missing one leaves an aircraft transmitting at the rate a parked device uses, unlocked, in the air.

Two bands, and nothing between them moves:

| | Condition | Meaning |
|---|---|---|
| flight | ground speed >= 12.0 m/s | a speed no aircraft taxis at |
| ground | ground speed < 1.0 m/s | stopped |
| between | keep the state you had | |

Ground speed is the whole rule. There is no altitude in it, no vertical rate, no barometer and no timer, so what decides the two states a radio and a firmware lock depend on is one number the receiver solves directly, on every fix, whether or not any other sensor on the board is alive.

There is no timer in any of it. What used to be five seconds of takeoff evidence and ten of landing is now the width of the gap between the two bands: ten times, where the thresholds it replaced were two apart. The gap is the hysteresis, and it is not decoration: a glider thermalling in a 15 m/s wind swings its ground speed from 5 m/s upwind to 40 downwind every circle, and a single threshold in the middle of that would land it and launch it once a turn, a log session and a firmware lock cycle each time.

1.0 m/s is the ground speed, and what it separates is an aircraft stopped from an aircraft moving at all. A receiver at a standstill reports 0.05 to 0.3 m/s of Doppler speed, spiking to a metre a second on multipath, so the threshold is three times the noise it normally sits above and the spikes cost a solution or two before a landing latches. It is also where the word on the glass goes back to GROUND. Lower and a finished flight keeps running; higher and an aircraft creeping on an apron reads parked, and the ground band widens under a wing hovering over one spot.

1.5 m/s is the third band, and unlike the other two it is not a state: ADS-L G.1.2 has two codes, and a glider being pushed to the grid is on the ground in both of them. `FlightMonitor::rolling()` answers the question the glass asks instead, stopped or moving, which is the difference between the words GROUND and TAXI (`products/skyblip_go/pages/README.md`). It picks up at 1.5 m/s and lets go below the 1.0 a landing needs, so the gap is the hysteresis again, half a metre a second of it: a parked receiver spiking to a metre a second on multipath never reaches the word, and a taxi slowing for a turn keeps the one it has. At 1.0 alone the word changed every few seconds on a device nobody had touched, and every change was a partial refresh of the glass spent on a state the aircraft was not in. The pick-up is a brisk walk, so what a pilot would call a taxi still reads as one. It holds through an outage the way the state does, because an antenna that drops out is not an aircraft that stopped.

It is decided here rather than on the page for the reason the takeoff is: a band is a fact about the aircraft, and a page that owns one is a second opinion waiting to drift from this file. The monitor is already fed every solution, so all three speeds are read in one place, off one sample, and `products/skyblip_go/services/ownship.cpp` publishes the answer as `state.flight.rolling` for whoever draws it.

12.0 m/s is 23 knots. Under it are a glider on tow to the grid at 4, a taxi at 5 to 8, and a tug hurrying back to the grid at 10; over it are a glider unsticking on aerotow at 20 and rotating off the winch at 23, a light aircraft at 28, a flexwing microlight whose stall is 16.9. The tug is the case that sets it, because it is the fastest thing on an airfield that is not flying and it carries one of these: a false takeoff there is a phantom log session, a firmware lock its pilot cannot clear between flights, an afternoon of the airborne transmit rate, and an ADS-L G.1.2 code on the air that tells every other aircraft not to suppress a target that is on the ground.

Going lower buys less than it looks. A light aircraft gains about two metres a second of speed every second, so 12 m/s is reached six seconds before a 28 m/s rotation and 10 m/s only two and a half seconds before that: both declare the takeoff while the aircraft is still on the runway, and the difference is runway, not air.

What keeps it from going higher is the wind. Rotation is an airspeed and this threshold is a ground speed, so a headwind moves every takeoff down the scale. In still air nothing here unsticks slowly: no aeroplane flown in this airspace may stall below 55 km/h, the German 120 kg class limit, and the FAI and UK 450 kg classes are capped at 65 km/h, which puts their liftoff at 17 to 20 m/s, a glider's on aerotow at 21 and a light aircraft's at 28. Into an 8 m/s breeze the same 120 kg class crosses the fence at 9 m/s over the ground, and a hang glider on tow leaves it at 13 to 15 before any wind at all. A threshold set at the still-air figures is a threshold that declares those launches late or never, and late here means an aircraft transmitting at the parked rate in the air, which is the only failure in this file that costs more than a log line.

What ground speed alone cannot see is a wing that does not need any. A paraglider trims at 10 m/s of airspeed, so into a 5 m/s breeze its ground speed is 5 for the whole flight; a hang glider working a ridge and a balloon in any wind are the same shape of problem. Those read on the ground from launch to landing: the parked transmit rate, no log session, no clock, and the firmware lock open, in the air. A barometric climb used to catch them, and it is gone deliberately, because the vertical rate on this board is a gust, a canopy, a pressure step and a 3 m/s spike whenever the barometer is out, and paying for it was a takeoff declared in the hangar queue. This is the trade the device makes: nothing is declared airborne that a pilot would not call airborne, and three classes of aircraft that never roll are not declared at all.

A landing is a stop, and only a stop, so an aircraft that lands and rolls straight on is still flying as far as this is concerned. A tug that touches down and taxis back to the grid without ever coming to rest is airborne until it does, which is the right answer for the radio: it is moving on an airfield, and what other aircraft need is a target that is not suppressed.

Evidence for flight is divided by the fix's own dilution of precision, the way OGN does it, so a solution nobody should trust cannot declare a takeoff. Evidence for the ground is not derated, and that asymmetry is deliberate: a derated figure is a smaller figure, and a smaller figure must never be the thing that puts an aircraft on the ground. The jerk gate is the other defence, and it guards the speed alone: a speed that jumps more than fourfold between two consecutive solutions is a receiver at a standstill, not an aircraft accelerating, so it costs one sample at the start of every roll.

The first solution after power-on decides on its own evidence rather than waiting: a device rebooted in flight that answers `Unknown` hands both the transmit rate and the update lockout their wrong default. In the band between the two, it decides for the ground, because switching on while being towed to the grid is the common case, and an aircraft rebooted in flight is doing more than 12 m/s over the ground within the minute.

What the missing holds cost is one case: an aircraft that stops on the runway and rolls again, a backtrack after a landing, is a second takeoff and a second session in the flight log. The ten-second landing hold used to absorb exactly that, at the price of a landing declared ten seconds after it happened.

There is no longer an altitude that means flight on its own either. The 2000 m rule this replaced was MSL, so any airfield above it - Samedan at 1707 m, Courchevel at 2008 m, Leadville at 3026 m - was a device that read airborne while parked, transmitting at 1 Hz with its update locked out for good.

## ground

`state` answers one question about one solution, and the bus carries that answer as the ADS-L G.1.2 code in `own.flight_state`. `state_from` reads it back, and it is the only place that does: G.1.2 is two bits and we own neither the sender nor the future, so a code this build does not name is `Unknown`, which every gate refuses.

`GroundLatch` is the second question, the one a door asks: may this device be written to, erased, updated. `Unknown` is not a ground - a device that has never had a fix has not proven anything - and once `Airborne` has been seen, only a positive `OnGround` clears it. A fix lost in flight is therefore never a landing: without the latch, a receiver dropping out over a ridge would unlock the firmware update mid-flight, which is the one failure `state` itself has no hold left to prevent.

There is one latch, owned by the service that owns the monitor (`products/skyblip_go/services/ownship.cpp`) and published as `state.confirmed_flight_state`. The DFU and settings gate in `core/comms/config.h` and the flight log's offload gate both read that one value rather than deriving a second opinion or asking each other, so "on the ground" means the same thing to all three.

## timer

`FlightTimer` counts from the takeoff `state.h` declares, and from no other event. One takeoff, so the clock on the glass, the log session on flash and the transmit rate cannot disagree about when the flight began. The takeoff is declared on the solution that shows it, so the clock starts within a second of the wheels.

It freezes at the landing rather than clearing, and the next takeoff carries on from the figure it froze at. The count is airborne time since the device was switched on, not time since the last takeoff: a circuit detail or a touch and go is one line in a logbook, and a pilot should not have to add up the legs the state machine happened to split the day into. It also makes a false landing cheap. A glider bouncing on a ridge that loses `Airborne` for twenty seconds costs those twenty seconds, where a resetting clock would cost the whole flight.

Nothing but a takeoff starts it and nothing but switching the device off clears it.

`FlightState::Unknown` is not a landing, and it is not a pause either. It is a solution the receiver could not give, so a flight already running keeps its takeoff instant and keeps counting across the outage, exactly as `FlightMonitor` holds its own state through it. A minute under a wing in the circuit is a minute flown: the aeroplane did not stop being in the air because the antenna did, and the figure a pilot copies into a logbook is wall time since takeoff, not time the receiver was well. Only a takeoff starts the clock, so `Unknown` before anything has flown still starts nothing.

`flown()` separates "no flight yet this power cycle" from "a flight zero minutes old", which are the same number and not the same answer: the page draws its no-answer dashes for the first and `0:00` for the second. `running()` is the other half the page needs, and it is not `own.flight_state`: that one reports `Unknown` on every bad solution, so a title driven from it would flicker between states in an outage.

A device switched on in the air is timed from the first solution rather than from the wheels, because `FlightMonitor` declares `Airborne` immediately in that case and nothing on board saw the takeoff. The alternative is withholding the number from exactly the pilot who has been flying longest.

The count is a difference of unsigned milliseconds, so the 49.7-day wrap of `ports::Clock::millis()` is one ordinary second of flight.
