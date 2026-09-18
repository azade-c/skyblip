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

## state

Airborne or on the ground, from the fix stream alone. It is not a display value: it gates the DFU lockout and the transmit rate, so the two ways of being wrong are not symmetric. Declaring a takeoff that did not happen locks the update out on the ground and drains the cell at the airborne burst rate; missing one leaves an aircraft transmitting at the rate a parked device uses, unlocked, in the air.

Two bands, and nothing between them moves:

| | Condition | Meaning |
|---|---|---|
| flight | ground speed >= 12.0 m/s, or a vertical rate >= 2.0 m/s from something already moving at 2.5 m/s | a speed no aircraft taxis at, or a climb no ground vehicle has |
| ground | ground speed < 1.0 m/s **and** the vertical rate under 0.75 m/s | slower than a person walks, and steadier than a wing can hang |
| between | keep the state you had | |

There is no timer in any of it. What used to be five seconds of takeoff evidence and ten of landing is now the width of the gap between the two bands: ten times, where the thresholds it replaced were two apart. The gap is the hysteresis, and it is not decoration: a glider thermalling in a 15 m/s wind swings its ground speed from 5 m/s upwind to 40 downwind every circle, and a single threshold in the middle of that would land it and launch it once a turn, a log session and a firmware lock cycle each time.

1.0 m/s is the ground speed, and what it separates is an aircraft stopped from an aircraft moving at all. A receiver at a standstill reports 0.05 to 0.3 m/s of Doppler speed, spiking to a metre a second on multipath, so the threshold is three times the noise it normally sits above and the spikes cost a solution or two before a landing latches. It is also the word on the glass: above it the radar says TAXI, so a glider being pushed to the grid at a walking pace reads as moving, which is what it is. Lower and a parked receiver flickers between the two words and a finished flight keeps running; higher and an aircraft creeping on an apron reads parked, and the ground band widens under a wing hovering over one spot.

12.0 m/s is 23 knots. Under it are a glider on tow to the grid at 4, a taxi at 5 to 8, and a tug hurrying back to the grid at 10; over it are a glider unsticking on aerotow at 20 and rotating off the winch at 23, a light aircraft at 28, a flexwing microlight whose stall is 16.9. The tug is the case that sets it, because it is the fastest thing on an airfield that is not flying and it carries one of these: a false takeoff there is a phantom log session, a firmware lock its pilot cannot clear between flights, an afternoon of the airborne transmit rate, and an ADS-L G.1.4 code on the air that tells every other aircraft not to suppress a target that is on the ground.

Going lower buys less than it looks. A light aircraft gains about two metres a second of speed every second, so 12 m/s is reached six seconds before a 28 m/s rotation and 10 m/s only two and a half seconds before that: both declare the takeoff while the aircraft is still on the runway, and the difference is runway, not air. What no speed threshold can be set under is the slowest class of all: a Part 103 ultralight stalls at 12.3 m/s by regulation and a paraglider flies at 10, and neither can be told from a fast taxi by ground speed. Those launch on the climb trigger, and in any wind on the nose so does everything else, because rotation is an airspeed and the ground speed under it is smaller.

2.0 m/s is the climb, and it is thin at both ends because the two ends nearly touch. Under it is everything a vehicle on the ground can show: 0.24 m/s taxiing at 8 up the 3% ICAO allows a taxiway, 0.8 up a 10% farm strip, 1.5 up Courchevel's 18.6% runway, about a metre a second of apparent rate when a gust crosses a vented case, and 1.5 on a spike when the barometer is out and the rate is a GNSS altitude differenced over two seconds. Over it is the weakest launch: a ridge start at 2 to 3, an aerotow's initial climb at 2.5, a light aircraft at 4, a winch at 10 and up. Raising it loses the aerotow in a headwind, which is the one launch where the speed threshold is also out; lowering it buys the altiport taxi and the gust.

0.75 m/s is the one number here that can put an aircraft on the ground while it is still flying, and all it has to clear is a wing hanging over one spot. Minimum sink is 0.9 m/s for a hang glider, 1.0 to 1.2 for a paraglider, and both fly at no ground speed at all when the wind matches their airspeed, which is what ridge soaring is. At 1.0 the threshold was the paraglider's own sink rate and had no margin left; 0.75 sits under the hang glider's and still clears the receiver's own vertical noise at a standstill, 0.1 to 0.6 m/s differentiated, by enough that a landing lands on the first quiet solution rather than waiting for a run of them. A glider's 0.55 m/s is under it and stays uncovered, but a glider holding station over the ground needs 25 m/s of wind to do it. Set this too high and a soaring wing is declared landed, which drops the transmit rate, closes the log session and opens the firmware lock, in the air; too low and a device parked in a gusty wind keeps a finished flight running for a few more solutions.

The climb trigger carries a ground speed of its own, `kClimbArmSpeedQ`, because a barometer at a standstill is not evidence of anything: a gust, a canopy closing, a pressure step, and a parked aircraft would otherwise open a flight log session and lock its own update out. 2.5 m/s is above the walk-run transition at 2.0, so nothing a person can push, pull or carry arms it, and a device being walked to the grid in a gusty wind stays on the ground. It is a separate constant from the ground band because the two answer different questions: one is how still an aircraft must be to have landed, the other is how much motion a barometer needs behind it before anyone believes it. Something rolling at 2.5 m/s and climbing at 2.0 is a ridge start or a winch launch, and those are the two launches a speed threshold alone cannot see.

The climb trigger is the only thing on this device that sees a slow wing leave the ground at all. A paraglider trims at 10 m/s of airspeed, so into a 5 m/s breeze its ground speed is 5 for the whole flight; a hang glider on a ridge and a balloon in any wind are the same shape of problem. On speed alone all three read on the ground from launch to landing: parked transmit rate, no flight log, no clock, and the firmware unlocked, in the air.

Evidence for flight is divided by the fix's own dilution of precision, the way OGN does it, so a solution nobody should trust cannot declare a takeoff. Evidence for the ground is not derated, and that asymmetry is deliberate: a derated figure is a smaller figure, and a smaller figure must never be the thing that puts an aircraft on the ground. The jerk gate is the other defence, and it guards the speed alone: a speed that jumps more than fourfold between two consecutive solutions is a receiver at a standstill, not an aircraft accelerating, so it costs one sample at the start of every roll.

The first solution after power-on decides on its own evidence rather than waiting: a device rebooted in flight that answers `Unknown` hands both the transmit rate and the update lockout their wrong default. In the band between the two, it decides for the ground, because switching on while being towed to the grid is the common case and a glider rebooted in a thermal is climbing.

What the missing holds cost is one case, and it is written down as a test: a bounce during a fast rollout is a second takeoff and a second landing, which is a second session in the flight log. The ten-second landing hold used to absorb exactly that. A wing lifting once the aircraft has stopped does not, because the climb trigger needs the ground speed.

There is no longer an altitude that means flight on its own. The 2000 m rule it replaced was MSL, so any airfield above it - Samedan at 1707 m, Courchevel at 2008 m, Leadville at 3026 m - was a device that read airborne while parked, transmitting at 1 Hz with its update locked out for good.

## ground

`state` answers one question about one solution, and the bus carries that answer as the ADS-L G.1.4 code in `own.flight_state`. `state_from` reads it back, and it is the only place that does: G.1.4 is two bits and we own neither the sender nor the future, so a code this build does not name is `Unknown`, which every gate refuses.

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
