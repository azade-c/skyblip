# core/gnss

A receiver produces a solution every second whether or not it can see the sky. A fix is a solution that survived `validity.h`. The two are different words here because they were the same word for a while, and a struct called `GnssFix` carrying `valid == false` is a fix that is not a fix: `nmea.h` parses `GnssSolution`, `FixValidity` judges it, and only what comes out the far side of that judgement is called a fix anywhere downstream.

`GnssSolution::is_fix` is filled twice, and it means the same thing both times. The parser sets it from RMC status `A`, which is the receiver's own claim. `parts::L76k::poll` overwrites it with `FixValidity`'s verdict before publishing, which is ours: both sentences present, neither stale, a date that is not the MTK 1980 lie, and a position no aircraft could have flown to. The receiver's claim never reaches the bus.

## How far a receiver with no fix has got

`NO FIX` is one word for an antenna under a hangar roof, a receiver nobody is talking to, and one twenty seconds from solving. `acquisition.h` tells them apart out of the sentences already on the wire, so it costs no bytes and no configuration: `SILENT` is nothing parsed for `kSentenceMaxAgeMs`, `SEARCH` is solutions arriving with no usable date, `TIME` is a date decoded, and `FIX` is a solution that passed `FixValidity`.

`TIME` is the rung worth having. The date is in the navigation message, so a receiver cannot report one until it has read a satellite through: it separates an antenna that sees nothing from one that sees sky and is waiting for geometry, which is the difference between moving the device and waiting another minute. The MTK 1980 lie is refused in the parser, so a fake date cannot promote the ladder.

The stage carries the instant it was entered, and the elapsed figure beside it is what makes it a progress report rather than a label. That figure goes on `status`, which already redraws every second for its UTC clock. The radar ring gets the word alone: the elapsed seconds there would be a partial refresh of the panel every second for as long as the device has no fix, which is the one page a pilot leaves on the glass.

## The satellites, and the second they do not fit in

GSV is the only sentence that carries a satellite's signal level, and `sky.h` is where a set of them becomes a picture: one entry per satellite with its id, elevation, azimuth and C/N0, the constellation off the talker, and a mark for the ones GSA named as being in the solution. An empty C/N0 field is a satellite in view and not tracked, which is not a satellite at zero dB-Hz, so it is kept as zero and drawn as a foot rather than a bar.

The talker is the constellation, with one exception the L76K's protocol specification states outright: QZSS answers on the `GP` talker, and satellite ids 193 to 197 are the only thing that tells it from GPS. BeiDou ids start again at 1, so an id without its talker means nothing at all. GSA carries the same answer in its own field 18, the NMEA 4.10 system id, and where the receiver leaves that out the in-use mark falls back to matching the id alone.

One GSA arrives per constellation, and the one whose satellites are in no solution carries no DOP at all, so a GSA with an empty PDOP is skipped rather than allowed to overwrite the fix mode and the VDOP the constellation before it reported.

What this costs is the reason it is not always on. At 9600 baud the fix burst is 320 bytes, a third of the second; the widest GSV set the part can send, four talkers and thirty-odd satellites four to a sentence at about 72 bytes each, is another 648. Together that is `L76k::kSearchingBurstMs`, over a second, which means RMC would arrive after the ADS-L direct slot has opened and `Transmitter::kFixLagMaxMs` would refuse the burst. So GSV is asked for exactly while `timing::own_ship_transmits` is false and given up the moment it is true, one `$PCAS03` each way with a null in every field but nGSV, which the receiver reads as keep what you were doing.

The turn-off has time to land because the settle above it exists: a re-fix waits `kRefixSettleMs`, five seconds, and the receiver obeys within one. Nothing is transmitted in between, so the two switches can never be on at once.

## The settle, and why it is measured rather than waited out

Nothing is transmitted on the first fix of a run. A cold receiver's first solutions walk: position, altitude and above all ground speed move over the seconds that follow, and ground speed is what `core/flight/state.h` calls a takeoff. A takeoff sets the transmit rate, starts the flight clock, opens the log and locks out firmware updates, so a burst sent through that window publishes a track nobody flew, possibly at the airborne rate while parked.

What the wait is standing in for is convergence, and convergence is measurable here: `pred_resid_m` is how far the constant-speed, constant-turn model missed the fix that has just arrived, an L1 sum in whole metres over the second between two solutions. So `FirstFix` settles on three consecutive solutions the model predicted to inside `kSettleResidualM`, and the clock is only the ceiling behind them.

The threshold is not a number of its own. A residual of that many metres is a velocity the model had wrong by the same figure in metres per second, so the threshold is `flight::kFlightSpeedQ`, the ground speed at which this device declares a flight, read over the one second the residual spans: under it, the error alone cannot invent the flight the settle exists to protect. Three solutions rather than one because two make the first residual and one residual can be luck, a receiver stepping forty metres every other second lands inside the threshold half the time. Three puts the floor three seconds past the first fix, which is the neighbourhood of moshe-braner's five second re-fix wait, reached from the other direction.

A 2D solution never counts. It reports no VDOP, its altitude is not a measurement, and a third of the residual would therefore not be one either.

The clock is a ceiling and never a floor. A receiver manoeuvring in rough air, or one with a genuinely poor antenna, can sit over the threshold indefinitely; a device that then never transmits at all is a worse failure than one transmitting a velocity that is a second behind. So it is three clean residuals or `kFirstFixSettleMs`, whichever comes first, and `kRefixSettleMs` after a receiver that only blinked.

What can falsify all of this is `models::L76k::walk_m`: metres of position error at the first solving second, decaying to none over `walk_ms`, alternating in sign so consecutive solutions disagree the way a walking receiver's do. Zero by default, because the ordinary model is a converged receiver and a cold one is something a case asks for.

## The rate

One solution per second, `L76k::kFixRateHz`. The receiver goes to 5 Hz and it would buy nothing here: the nav rate does not change acquisition, so a cold start is no faster, and the extra solutions are noisier rather than more accurate. What a higher rate does buy is lower latency on velocity during a manoeuvre, which `flight::extrapolate` already covers by carrying the fix forward to the burst's own instant. It also does not fit: GGA, three GSA and RMC is 320 bytes, 333 ms of 9600 baud line, and `L76k` static-asserts that the burst fits inside one solution period.

## PPS, and what it is worth

The 1PPS pin is the receiver's, the slot map is the radio's, and the edge is what joins them: `timing::ClockState::pps_edge_us` is phase 0 of the UTC second the ADS-L dwell map is measured against, latched in a GPIO interrupt so it carries no kernel-tick jitter. A solution is dated at that edge rather than at the arrival of its last sentence, which is what `solution_instant_ms` is for. Without a lock it falls back to subtracting the receiver's stamped burst latency.

Whether the L76K emits the pulse at all without a fix is not documented. Quectel's hardware design and protocol specification describe the pin and its 100 ms pulse and say nothing about gating, and the `$PCAS` sentence set has no command for it. The chip is an AT6558, whose CASIC `CFG-TP` message carries an `enable` field where 2 is "output continuously, maintain the rate when it cannot position" and 3 is "output only while positioning": the behaviour is configurable, and we never send that message, so we run on a default nobody states. It decides how much of the fix-loss path is real, because `timing::kPpsHoldoverMs` gives the slot map 60 s of dead reckoning after the edges stop. Settle it on the bench: cold-start the receiver indoors and watch whether the status page transmits or the edge count in `platform::zephyr::Pps::edges()` moves while the page reads `NO FIX`.
