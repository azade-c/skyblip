# core/flight

What the aircraft is doing, decided from the fix stream and the barometer. Pure, testable, and free of every framework header: the services in `products/` feed these and publish what they answer.

| File | What it decides |
|---|---|
| `state` | airborne or on the ground, which gates the DFU lockout and the transmit rate |
| `timer` | how long this flight has been running |
| `atmosphere` | the standard atmosphere as integer math: pressure altitude, subscales, vertical speed |
| `turn` | rate of turn from two reported tracks |
| `extrapolate` | where an aircraft will be, for the transmitter and the alarm |
| `log_record`, `log_session` | what a flight leaves behind, and when a session runs |

## timer

`FlightTimer` counts from the takeoff `state.h` declares, and from no other event. One takeoff, so the clock on the glass, the log session on flash and the transmit rate cannot disagree about when the flight began. The five seconds `kTakeoffHoldMs` costs are invisible at the minute resolution the six-pack reads it in.

It freezes at the landing rather than clearing, and the next takeoff carries on from the figure it froze at. The count is airborne time since the device was switched on, not time since the last takeoff: a circuit detail or a touch and go is one line in a logbook, and a pilot should not have to add up the legs the state machine happened to split the day into. It also makes a false landing cheap. A glider bouncing on a ridge that loses `Airborne` for twenty seconds costs those twenty seconds, where a resetting clock would cost the whole flight.

Nothing but a takeoff starts it and nothing but switching the device off clears it.

`FlightState::Unknown` is not a landing. It is a solution the receiver could not give, so the timer keeps its takeoff instant and picks the count up again when the fixes come back, exactly as `FlightMonitor` holds its own state through the same outage. Without that, a minute under a wing in the circuit would restart the flight.

`flown()` separates "no flight yet this power cycle" from "a flight zero minutes old", which are the same number and not the same answer: the page draws its no-answer dashes for the first and `0:00` for the second. `running()` is the other half the page needs, and it is not `own.flight_state`: that one reports `Unknown` on every bad solution, so a title driven from it would flicker between states in an outage.

A device switched on in the air is timed from the first solution rather than from the wheels, because `FlightMonitor` declares `Airborne` immediately in that case and nothing on board saw the takeoff. The alternative is withholding the number from exactly the pilot who has been flying longest.

The count is a difference of unsigned milliseconds, so the 49.7-day wrap of `hal::Clock::millis()` is one ordinary second of flight.
