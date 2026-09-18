# core/traffic

What the sky around this aircraft contains, how dangerous it is, and who in it is flying with us. Pure, integer, no framework headers: the services in `products/` feed these and publish what they answer.

| File | What it decides |
|---|---|
| `sanity` | whether a decoded position is close enough to have been heard at all |
| `table` | which aircraft the finite table holds, and each one's turn rate |
| `conflict` | whether two projected paths enter the volume neither may enter |
| `alarm` | the level a contact is graded at, and what the annunciator is allowed to say |
| `formation` | which contacts are flying with us, on geometry alone |
| `link` | the rows the signal page reads |

## The model

The alarm grades geometry, never a flying style. There is one projector, `core/flight/arc`, and both the alarm and the radar page read it, because a leader line that curves one way while the alarm grades the other is two models and one of them is wrong.

`conflict::first_breach` walks own-ship's arc and a target's arc in lockstep, `kStepMs` apart out to `kHorizonS`, and reports the first sample where the two are inside the protection volume. It also reports the closest approach it saw, which is what a screen draws when nothing breaches.

| Constant | Value | Where the number comes from |
|---|---|---|
| `kHorizonS` | 60 | the minute the radar already drew as a leader line, so the glass and the alarm sample the same future |
| `kStepMs` | 2000 | 30 steps per target, 48 targets, one pass a second: a few milliseconds on the nRF52, and fine enough that a 60 m/s closure cannot cross the core between samples |
| `kProtectionRadiusM` | 75 | the lateral miss that is a near miss rather than traffic. Gliders share a thermal 150 m apart all day, and a core wide enough to catch that pair is a device that gets switched off |
| `kProtectionVertM` | 50 | co-altitude in the sense a pilot means it, well inside the 300 m window `alarm.h` draws traffic in |
| `kSpreadMmPerS`, `kVertSpreadMmPerS` | 0.5 m/s, 0.25 m/s | a projected position is not a fact: track and speed noise, and a turn nobody has to hold. The volume grows with lead time to say so |
| `kMaxSpreadM`, `kMaxVertSpreadM` | 30, 15 | the growth stops. Uncapped, the volume reaches 240 m at a minute and every neighbour holding station becomes a conflict, which is the nuisance alarm this work removed |

Levels come off the time to that breach, at the thresholds the cockpit already knew: `kUrgentTtiS` 15 s, `kImportantTtiS` 25 s. The proximity ring survives only as the info floor, so a contact inside `kInfoDistM` and inside the vertical window is a dot on the plot whatever it is doing. A target that reports no velocity is charged at `kUnknownTargetSpeedMps` on a course straight at us, because zero would make a relayed position the safest thing in the sky.

What is deliberately absent: there is no co-circling test, no gaggle range gate, no steady-range timer, and no constant anywhere in this directory that assumes a glider. Two aircraft on one thermal circle are quiet because their arcs never meet, and the pair on offset circles that pass at 15 m is alarmed on before it happens. That pair was decision 5.3's committed limitation, and `test/core/test_traffic.cpp` now pins it as an alarm.

## Turn rate

ADS-L carries position, speed, track and climb, and no turn rate (G.1.8, G.1.10), so a neighbour's is differentiated from the tracks it has reported. It lives on the table entry rather than inside the alarm, because the screen needs the same number: `kTurnWindowMs` is the shortest window a 1 Hz track says anything over, and `kTurnGapMs` is the gap past which two reports are two facts rather than a rate, so the estimate re-arms instead of averaging across what it did not see. `flight::kMaxTurnDps` clamps what is believed; above it the figure describes the receiver.

## Formation

Aircraft fly together on purpose: a patrol, a tug and its glider, two friends on a task, a gaggle in one thermal. The device cannot see intent, so `formation` names the observable: a contact within `kRangeM` and `kVertM` whose position in own-ship's own heading-up frame has not moved more than `kDriftM` for `kSteadyMs`. That covers all four cases without naming any of them, and a circling pair matches it for the same reason a patrol does.

Three rules keep it honest:

The detector only ever proposes. `State::Candidate` raises an offer on the glass, and nothing is silenced until the pilot answers it with the same double press that authorises a firmware upload on this device. FLARM-style automatic suppression is what this work deleted, and it is not coming back in through the detector.

A member is silenced on the annunciator and never on the plot. It stops being drawn as a separate symbol because the square around own-ship and the count in its quadrant are its depiction, and drawing it twice would be two aircraft.

**A mute cannot survive a breach.** If the arcs say a member enters the protection volume inside `kUrgentTtiS`, the membership is dropped and the contact alarms, on that fix. The pilot can silence a neighbour flying with them; nobody can silence a collision.

The lease ends by itself: `kBreakFixes` consecutive fixes of drift, or leaving the range and altitude band, report `State::Broken` once, which is the `FORMATION SPLIT` notice, and a contact nobody has heard for `kForgetMs` is forgotten with its membership. Addresses rotate only between flights, so a slot reallocated to another aircraft starts at `State::None` and costs the pilot one re-tap.
