# core/gnss

A receiver produces a solution every second whether or not it can see the sky. A fix is a solution that survived `validity.h`. The two are different words here because they were the same word for a while, and a struct called `GnssFix` carrying `valid == false` is a fix that is not a fix: `nmea.h` parses `GnssSolution`, `FixValidity` judges it, and only what comes out the far side of that judgement is called a fix anywhere downstream.

`GnssSolution::is_fix` is filled twice, and it means the same thing both times. The parser sets it from RMC status `A`, which is the receiver's own claim. `parts::L76k::poll` overwrites it with `FixValidity`'s verdict before publishing, which is ours: both sentences present, neither stale, a date that is not the MTK 1980 lie, and a position no aircraft could have flown to. The receiver's claim never reaches the bus.

## The rate

One solution per second, `L76k::kFixRateHz`. The receiver goes to 5 Hz and it would buy nothing here: the nav rate does not change acquisition, so a cold start is no faster, and the extra solutions are noisier rather than more accurate. What a higher rate does buy is lower latency on velocity during a manoeuvre, which `flight::extrapolate` already covers by carrying the fix forward to the burst's own instant. It also does not fit: GGA, three GSA and RMC is 320 bytes, 333 ms of 9600 baud line, and `L76k` static-asserts that the burst fits inside one solution period.

## PPS, and what it is worth

The 1PPS pin is the receiver's, the slot map is the radio's, and the edge is what joins them: `timing::ClockState::pps_edge_us` is phase 0 of the UTC second the ADS-L dwell map is measured against, latched in a GPIO interrupt so it carries no kernel-tick jitter. A solution is dated at that edge rather than at the arrival of its last sentence, which is what `solution_instant_ms` is for. Without a lock it falls back to subtracting the receiver's stamped burst latency.

Whether the L76K emits the pulse at all without a fix is not documented. Quectel's hardware design and protocol specification describe the pin and its 100 ms pulse and say nothing about gating, and the `$PCAS` sentence set has no command for it. The chip is an AT6558, whose CASIC `CFG-TP` message carries an `enable` field where 2 is "output continuously, maintain the rate when it cannot position" and 3 is "output only while positioning": the behaviour is configurable, and we never send that message, so we run on a default nobody states. It decides how much of the fix-loss path is real, because `timing::kPpsHoldoverMs` gives the slot map 60 s of dead reckoning after the edges stop. Settle it on the bench: cold-start the receiver indoors and watch whether the status page transmits or the edge count in `platform::zephyr::Pps::edges()` moves while the page reads `NO FIX`.
