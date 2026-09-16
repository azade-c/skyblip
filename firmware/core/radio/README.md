# core/radio

The station log: every burst this radio sent or heard, in the order it happened, newest first.

`rx_ok` and `tx_ok` are totals, and a total cannot tell an empty sky from a receiver that frames nothing. The traffic table only ever holds what already decoded, so a burst that arrived and did not become a frame leaves no trace in it. That burst is the one worth seeing: it is the difference between "nobody is transmitting" and "everybody is transmitting and I am deaf to them", and the two have the same reading on every other page. Two skyBlips that both transmit and neither hears is the fault this exists for, and it happened: `git log core/protocol/air.cpp`.

`Event` names the seven things that can happen to a burst.

| | |
|---|---|
| `Transmitted` | own-ship's burst left the antenna |
| `Lost` | own-ship's burst was armed and never completed: the dwell ran out, or the radio's transmit timeout did |
| `Held` | the hour's air-time budget refused it (`timing::Transmitter::Attempt::over_budget`) |
| `Unarmed` | `hal::Rf` refused the plan that carried it, so nothing was ever armed |
| `Received` | a burst arrived, framed, and named an aircraft |
| `BadCrc` | an integrity check refused it: the chip's own CRC, an ADS-L CRC no forward correction could rescue, an ALP-TAS CRC, or a Reed-Solomon codeword the uplink could not correct |
| `Undecoded` | the bits survived their check and still named no aircraft: no known system behind the sync window, or an ALP-TAS frame with no fix of our own to decode it against |

`BadCrc` and `Undecoded` were one verdict until the bench had two devices on it, and the pair of them is the reading that separates a marginal link from a protocol disagreement. Bits the air corrupted are a radio problem, and bits nothing here knew what to do with are ours: one is answered by moving the antenna and the other by reading `core/protocol/`.

A `CRC` row with a byte count is a frame that framed and failed its own protocol's check, which is a real emitter the air damaged. One with only a level is the modem's own refusal, and on this band it is mostly the 16-chip shared sync window matching noise. Both carry the level they arrived at now: `GetPacketStatus` survives a CRC failure, and -110 dBm against -15 dBm is the difference between band noise and a neighbour this receiver is failing to decode.

Until 2026-09-16 the modem was programmed with `CRCType = 0x00`, which the SX126x spells one-byte CRC: off is `0x01` (DS table 13-70). The two units on the bench interoperated because they made the same mistake symmetrically, and every foreign ADS-L or FLARM burst was thrown away by a CRC byte its sender never sent. Correcting it does not make those rows go away, it renames them: a false sync now reaches `protocol::receive_mband`, fails to match either 24-bit sync tail, and reads `DEC`.

There is no verdict for a burst the radio declined to send, because nothing declines: the instant is drawn inside the slot and the PA keys there whatever the receiver is hearing (`core/timing/README.md`). A dwell that carried a burst and reported nothing is `Lost`.

`Lost` and `Unframed` are both `rx_bad` on the counters, which is wrong of the counters: `messages::RfEventType::Missed` is only ever emitted for a dwell or a transmission of ours that did not complete, never for a reception. The log is split because a transmit failure reported as a bad reception sends a reader hunting the wrong fault, which is the exact thing this page exists to stop. The counter keeps its old meaning until it is given one of its own: [#61](https://github.com/fcatuhe/skyblip/issues/61).

`Entry::at_s` is UTC once the receiver has given us a second, and time since boot before that. Which of the two is a flag per entry rather than a flag on the log, because the log outlives a first fix and the entries either side of one are dated differently. The second itself comes from the PPS edge the clock last latched (`timing::ClockState::utc_s`), not from the sentence that names it: see `core/timing/README.md`.

## The phase, and what two devices read across it

`stamp_of` dates a burst from the instant the executor stamped it with (`messages::RfEvent::at_us`), against the PPS edge `timing::ClockState` latched, rather than from the pass that drained the queue. Three things follow, and the first is the only one the page exists for.

The phase is UTC-referenced on every device that holds a lock, so a burst sent on one skyBlip and heard on another is one instant read twice: `TX .462` here and `RX .462` there is the link, measured, with nothing between the two units. Both stamps are taken where the radio raised the burst - the end of it, either way - so what is left between them is one poll period, and a missing row at that phase is a dwell or a channel that did not line up rather than a weak signal.

Slot 1 opens at 800 ms and closes 200 ms into the next second, so a burst in that tail is drained under a second the air had not reached when it was sent. Dating from the event's own instant puts it back in the second its dwell opened in; dating from the drain would have moved a burst a second forward and read as a clock fault on the device that heard it.

A phase is refused rather than guessed. Without a PPS lock there is no edge to measure from, and past `kStampReachUs` the event and the edge are not two views of one second. The row then prints the second alone, which is what the receiver actually knows.

`Entry::channel` is the dwell's own, carried on the event from the executor that armed it (`RfEvent::freq_hz`): §C.2.5 makes traffic alternate between 868.2 and 868.4 MHz, and a receiver that never visits the second channel hears half its neighbours. Deriving it from the phase instead would have agreed with the slot map exactly when the radio did not, which is the one case worth printing.

For that label to be true, a dwell drains its own tail before it closes. A burst framed as the window ends leaves its flag in the chip, and the next dwell's first poll used to read it and stamp it with the new band, the new channel and the new phase. Clearing the register on the way in would have been worse: between two dwells the receiver is still tuned where it was, so a frame arriving there is a real reception on the dwell that just closed, and that is the one it is reported as.

`Entry::tx_span_us` is the burst's own completion: from the instant `timing::Transmitter` aimed it at to the instant the executor reported it done. §C.2's M-band burst is 4800 us of that at 100 kchip/s, and the rest is the pass that collected the report, so the figure reads a little over 4800 on silicon and a good deal more wherever the radio thread is being starved. It is not a jitter against the deadline: the report lands at the end of a burst that the deadline keys the start of, and subtracting a nominal length to make it read zero would be this file forming an opinion about air time it did not measure. `state.tx_deadline_us` carries the deadline out of `go::RadioService`, which owns it, for the same reason `last_tx_done_at_us` travels the other way: neither service forms a second opinion about the other's half.

`Entry::tx_keyed_us` is the first half of that, from the same deadline to the instant the executor issued SetTx (`RfEvent::keyed_at_us`). One number could not say whether a slow burst was slow before the air or after it, and both halves have different causes: what happens before is this CPU, what happens after is the chip and the read-out. The pair saturates at the field's own ceiling rather than at a column width - a burst 65 ms past its deadline is a dwell that has already ended, and the `Lost` verdict is what reports that.

A reception is dated where the radio raised it: the executor stamps the instant it saw DIO1 go up, or the instant it began the status read, never the instant the read-out ended. Reading 58 chip-bytes out of the buffer costs milliseconds on a bench, and it used to sit between the burst and its own timestamp, which is what made one link's two stamps read 2 to 3 ms apart. DIO1 stays a level, not an edge: a dwell is driven by a deadline (`boards/lilygo/t_echo_plus/t_echo_plus.dts`).

The capacity is what one screen holds. Nothing is kept that could not be shown: this is a tape of what is happening now, not a history to scroll back through. `ui/screens/radio_log.cpp` draws exactly `Log::kCapacity` rows for that reason.

## Who writes what

`go::TrafficService` writes everything the air reported. It already drains `bus.rf`, and it is the one place that knows whether a burst that arrived also decoded, which is the distinction the whole page turns on.

`go::RadioService` writes the two the air never sees. A burst its own policy refused reaches no executor and raises no event, so before `Held` and `Unarmed` existed the counters moved and the page stayed silent - which reads exactly like a dead transmitter to the one person looking at it. `Held` is the hour's allowance holding every burst until it frees up, and `Unarmed` is the executor refusing a plan that cannot complete inside its own dwell.

`Held` is recorded once per spell, not once per refusal. The allowance holds for minutes at a time and a row a second would push the sky itself off a sixteen-row tape, so the row says when the spell began and `timing_stats.refused()` counts every one. The next completed burst ends the spell, and a refusal after that is a new one. `Unarmed` is written only for a plan that carried a burst: a receive dwell the executor refused is a deafness, not a transmission, and it keeps its counter in `timing_stats.missed()` until the page has somewhere honest to put it.

## On a bench

Two units on a desk read -6 to -17 dBm, which is a hundred dB over this modem's floor and into compression. Nothing measured there says anything about link margin: put distance or attenuation between them before believing a level. What a bench does answer is the column of verdicts, and the honest reading of a quiet band is a few `DEC` rows a minute from the shared sync window matching noise.
