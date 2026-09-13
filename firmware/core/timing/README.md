# core/timing

One radio, one second. `slot.h` cuts the second into dwells, `transmit.h` decides whether own-ship speaks in one of them and at which instant, `channel.h` holds what the channel sounds like and what we have already spent on it. Nothing here touches hardware: `hal::Rf` flies the plan against absolute deadlines.

## The burst is placed, and nothing on air moves it

`Transmitter::instant_in()` draws the instant from `mix(address ^ mix(utc))`, uniform over the slot's usable width, re-keyed every second. The executor keys the PA there. It does not listen first, it does not back off, it does not defer to anything it receives: `RfPlan::tx_at_us` is the whole contract, and the only rule that can still refuse a burst is the hour's air time (`AirTime`, below).

This is a deliberate deviation from a plain reading of ADS-L 4 SRD-860 issue 2 §D.3, which describes CSMA with listen-before-talk. Carrier sense cannot do the job the clause implies here, for four reasons.

It cannot see the burst it would collide with. A burst is 5 ms and the direct slot 550 ms wide, so two aircraft collide when their drawn instants land within 5 ms of each other: about 1.8% per neighbour in range. Half of those we drew the earlier instant for, and an assessment only ever detects a burst *already* on air. The theoretical ceiling against our own kind is around 0.9% of bursts saved per neighbour.

It listens in the wrong place. The collision happens at the receiver's antenna and the assessment is made at ours. A third aircraft the other side of us hears both bursts collide while we heard nothing, and we defer to a station the intended receiver cannot hear at all. At these ranges the hidden terminal is the normal case, not the corner one.

It is weighted backwards. A burst has to clear the floor by a good margin to register at all, so the traffic we would successfully defer to is the near, loud traffic whose position we already have, while the distant aircraft near the noise floor, the one whose update matters, is never heard and never deferred to.

And it costs the one thing this device exists for. An aircraft that goes quiet is invisible. What protects the band is the randomised instant, decorrelated by device address and redrawn every second so a collision is not repeated, and the 1% duty cycle this product declares as its channel-access route.

None of the references cancel a burst either. `pjalocha/nrf52-ogn-tracker` (`src/ogn-radio.cpp:840-851`) backs off and escalates, then falls through and transmits when the slot runs out; its loop has no path that drops the packet. `pjalocha/esp32-ogn-tracker` has the code but both `TimeSlot()` call sites pass `MaxWait=0`, which skips it entirely. Neither SoftRF fork has listen-before-talk at all. This device is the same on air as those, with one fewer moving part.

## What the channel measurement is still for

`NoiseFloor` and `ChannelLevel` survive as instruments, not as gates. Each executor reads the tuned channel once per dwell, as a window of `ChannelLevel::kSamples` instantaneous reads averaged in the linear domain, because the SX1262 has no averaging block and GetRssiInst is an instant by definition (DS 13.5.2). One read lands between two neighbours' bursts and calls a loud site quiet; a window does not. `NoiseFloor` walks that figure into a running average, seeded at OGN's -105 dBm so a cold start reads as quiet rather than as broken, and it leaves the device as `noise_dbm` in the status dump.

That number answers a question the counters cannot: a device hearing nothing at a site reading -85 dBm is deaf because the band is full, and one hearing nothing at -110 dBm is deaf for its own reasons. Nothing waits on it.

## The duty cycle is the channel-access route

EN 300 220-2 V3.3.1 Table 4 band M is 1% of any hour, and that limit is what this product declares. `AirTime` is the evidence: sixty one-minute buckets on a ring that turns by elapsed time, so the hour that straddles `millis()`'s 49.7-day wrap is an hour like any other.

At the design rate of one 5 ms burst per second we sit at half the allowance, so an empty budget can only be a fault. It blocks: a faulted transmitter that will not stop is worse for everyone on the band than a quiet one. `Attempt::over_budget` says so, and `SlotTimingStats::refused()` counts it.

## What the dwell map is for

`kSlot1End` is 1200 ms, 200 ms past the second it opened in, because FLARM-generation traffic is still transmitting there. That tail is receive-only: §C.5 ends the direct slot at 1000 and `Transmitter::last_instant_in()` bounds the draw so a burst always completes inside the slot and inside the dwell that carries it.

`SlotPlan::own_tx_dwell` is a property of the dwell, not of the phase the service happens to tick on. Slot 0's dwell opens at 400 and its burst is placed from 450, so the plan that opens the dwell has to carry it.

## One arm per dwell, and never a window that has closed

`hal::Rf::arm()` queues a plan behind the dwell already flying, on silicon because the executor is a thread that reads its plan once, and on the host because it models the same rule. Two things follow, and both were bugs on the bench before 2026-09-15.

A burst is armed with the dwell that carries it or not at all. A plan armed mid-dwell is read when that dwell ends, by which time its own window has closed, so the burst never keys and is reported as one that was lost. `RadioService` therefore arms on the dwell's edge and does not re-arm inside it: an attempt that turns true mid-dwell, which is what a device that has never transmitted does, waits for the next dwell rather than firing a plan nobody can fly.

A window already behind the phase is not armed at all. The guard phases between dwells (`SlotState::Hop`, `SwitchOtoM`) report the dwell that has just closed, and arming that plan produced a stub of a millisecond or two that the executor could only drop. A dropped plan carrying no burst is silent now: it is a receive dwell that did not happen, not a transmission that failed, and the station log said `LOST` for it.
