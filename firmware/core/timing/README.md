# core/timing

One radio, one second. `slot.h` cuts the second into dwells, `transmit.h` decides whether own-ship speaks in one of them and at which instant, `channel.h` holds what the channel is worth and what we have already spent on it. Nothing here touches hardware: `hal::Rf` flies the plan against absolute deadlines.

## The burst is placed, never cancelled

Listen-before-talk (ADS-L 4 SRD-860 issue 2 §D.3) chooses *when* inside the window. It does not choose *whether*. A dwell that carries a burst puts it on air: at the first instant the carrier reads clear, or at `Transmitter::last_instant_in()`, the last instant a 5 ms burst still completes inside the direct slot. `RfPlan::tx_by_us` is that deadline and the executor keys the PA there whatever the carrier says.

This is a deliberate deviation from a plain reading of §D.3, and the reason is that carrier sense cannot do the job the clause implies here.

A burst is 5 ms and the direct slot is 550 ms wide, so two aircraft collide when their drawn instants land within 5 ms of each other: about 1.8% per neighbour in range. Half of those we drew the earlier instant for, and an assessment only ever detects a burst *already* on air. So the theoretical ceiling of listen-before-talk against our own kind is around 0.9% of bursts saved per neighbour.

Against that, the collision happens at the receiver's antenna and the assessment is made at ours. A third aircraft the other side of us hears both bursts collide while we heard nothing, and we defer to a station the intended receiver cannot hear at all. At these ranges the hidden terminal is the normal case, not the corner one. Detection is also weighted backwards: a burst has to clear the floor by `kClearMarginDb` to register, so the traffic we successfully defer to is the near, loud traffic whose position we already have, while the distant aircraft near the noise floor, the one whose update matters, is never heard and never deferred to.

What protects the band is the randomised instant (`Transmitter::instant_in()`, a fresh draw every second, decorrelated by device address) and the 1% duty cycle this product declares as its channel-access route. An aircraft that goes silent is invisible, which is the failure this device exists to prevent.

The escalating threshold is what keeps devices off the deadline instant: every failed assessment inside a dwell buys 3 dB of tolerance (`NoiseFloor::backed_off`, capped by §4.6.2.3's ceiling), so on a noisy site bursts key at spread-out backoff instants rather than all piling onto `by_ms`.

None of the references cancel a burst either. `pjalocha/nrf52-ogn-tracker` (`src/ogn-radio.cpp:840-851`) backs off and escalates, then falls through and transmits when the slot runs out; its loop has no path that drops the packet. `pjalocha/esp32-ogn-tracker` has the code but both `TimeSlot()` call sites pass `MaxWait=0`, which skips it entirely. Neither SoftRF fork has listen-before-talk at all.

## What the dwell map is for

`kSlot1End` is 1200 ms, 200 ms past the second it opened in, because FLARM-generation traffic is still transmitting there. That tail is receive-only: §C.5 ends the direct slot at 1000 and `last_instant_in()` respects it, so a burst held by a busy channel still cannot key inside the tail.

`SlotPlan::own_tx_dwell` is a property of the dwell, not of the phase the service happens to tick on. Slot 0's dwell opens at 400 and its burst is placed from 450, so the plan that opens the dwell has to carry it: `hal::Rf::arm()` queues a plan armed mid-dwell behind the one already flying, and a burst added by a second arm is read only after the window it asked for has closed.
