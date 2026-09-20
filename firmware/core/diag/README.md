# core/diag

The diagnostics capture: one fixed-size record per thing the device sensed or decided, buffered in RAM while a capture is armed, so a bench or a flight can be replayed on a laptop and the tuning constants re-derived from what actually happened.

It exists because every threshold in this tree - 12.0 m/s for a takeoff, 1000 ms for a long touch, 60 s of PPS holdover, the dwell map itself - was chosen from a datasheet, a reference project or an argument, and none of them from this device's own numbers. A corpus is what turns that into measurement, and it is only worth having if it is comprehensive and if its holes are visible.

## What it decides

Three things, and nothing else.

`record.h` is the envelope: a 24-byte slot, the type tag that names what is in it, the instant it happened at, and the flags every type shares. `payload.h` is the subjects: one struct per thing worth recording, each laid out inside the 16 bytes the envelope leaves, and the two files behind it are the corpus cut in half - `payload_sensed.cpp` for what reached the device, `payload_decided.cpp` for what it made of it. `recorder.h` is the RAM ring a capture fills and a writer drains.

What it deliberately does not hold:

| Not here | Where |
|---|---|
| the flash writing, the sector ring, the recovery scan | `core/store/`, and the service that owns the partition |
| the CRC over a record | the sector label's, `core/store/sector.h`: this geometry has no room for one, and the torn tail is answered by the `End` marker below |
| the BLE offload, the chunking, the base64 | `core/comms/log_link.h`, shared with the flights store |
| the taps that fill these records | the services in `products/`, one call each, behind `armed()`: the table below says which |
| where the capture is armed from | the product's diagnostics screen, `products/skyblip_go/pages/capture.cpp` |
| draining this ring to flash | `products/skyblip_go/services/capture.cpp`, over the store both logs share |
| whether a capture survives a reboot | nothing: it does not, and that is the design |

The armed flag lives in RAM and in nothing else. It is not a setting, it is never written to flash, and a boot always comes up disarmed: a device that quietly kept recording after a power cycle would fill its partition with a capture nobody asked for, and the pilot who armed it would not be the one who found out.

## The record

24 bytes, the same geometry as `flight::LogRecord`, because the sector ring, the chunking, the base64 and the resumable offload are the flights store's and must not be written twice.

| Offset | Bytes | Field |
|---|---|---|
| 0 | 1 | type |
| 1 | 1 | flags |
| 2 | 2 | into_ms, milliseconds into the second (`core/events/stamp.h`) |
| 4 | 4 | at_s |
| 8 | 16 | payload |

Every field is written and read one at a time, little-endian, exactly as `core/flight/log_record.cpp` does it: a `memcpy` of a struct puts this layout at the mercy of padding nobody can see, and the host decoder that reads these back is on the other side of a Bluetooth link and a schema.

`at_s` is UTC once the receiver has given us a second and time since boot before that, which is the same rule and the same reason as `radio::Entry::at_s`: the capture outlives a first fix and the records either side of one are dated differently. Which of the two it is, is `kFlagUtcDated` on the record rather than a flag on the capture. `kFlagPhaseValid` says `into_ms` was measured against a PPS edge instead of guessed at, and without it the millisecond field means nothing. Those two bits mean the same thing on every type; bits 2 to 7 belong to the type and are named beside it in `payload.h`.

A type is retired, never renumbered. A recorded corpus outlives the firmware that wrote it, and a laptop decoding a flight from last season has nothing but the number to go on - `test/core/diag/test_diag_record.cpp` fails if one moves. Type 0 is not a record, so an all-zero slot decodes as nothing rather than as a boot, and a slot no writer has reached reads erased (`store::erased`) rather than as a record of zeroes.

## What each type answers

Every field below is read off `bus::State` or off an `events::` value as it stands today. Nothing here is a sensor this device does not have.

| Type | Payload | The tuning question it answers |
|---|---|---|
| `Boot` 1 | capabilities, firmware version, reset reason, image state | which build and which parts produced the rest of the corpus, and whether the device came up from a fault |
| `Config` 2 | address, address table, aircraft type, alarm volume, battery and frequency trims, whose the battery trim is, units, alarm enabled | what the firmware was assuming while it decided everything else |
| `Gnss` 3 | nav_ms, residual, HDOP, VDOP, stage and its age, sats used and in view, fix mode, reject reason | where in its own second a solution lands (`kFixLagMaxMs`, 500 ms of §G.1.16 nav age), and what the rejects cost |
| `Pps` 4 | edge interval, signed error against a nominal second, samples, holdover events, ms since the edge, lock | whether `kPpsHoldoverMs` is the right patience, and what the slot map is really anchored to |
| `Burst` 5 | the whole of `radio::Entry`: verdict, band, channel, address, length, RSSI, key offset, tx_keyed_us, tx_span_us, airborne | every one of the eleven verdicts against the second it happened in: two devices on one bench read one link twice |
| `Dwell` 6 | slot state, band, frequency, start and end, the phase it was read at, duty, noise floor, refusal reason, tx_allowed | whether the dwell map spends the second where §C.5 says, and what refuses a burst when one is refused |
| `Flight` 7 | speed, climb, altitude, HDOP, VDOP, declared state, latched state, rolling | how close the machine came to deciding the other way. Written on every evaluation, never only on transitions: a transition-only tape cannot say what the margin was, and the margin is the whole question when choosing 12.0 against 8.0 m/s |
| `Power` 8 | cell millivolts, percent, level, the caution knee, charge condition, die temperature, the trim the charger taught this unit, supply warnings, implausible and charge counts | what a burst does to the rail, and whether `kCautionMv`, `kLowWarnMv` and `kCutoffMv` fire where a real pack needs them to |
| `Baro` 9 | pressure, derived altitude, the rate taken from it, temperature | the vertical speed window (`kBaroVsWindowMs` against `kGnssVsWindowMs`) measured rather than assumed |
| `Motion` 10 | ball position, the three g-meter axes now, the normal axis at both extremes, hub error bytes | what the airframe pulled, and whether the filter lengths suit it. Absent on a unit with no hub, which the fitted flag says |
| `Contact` 11 | contact, edge instant, how long the level had held, the gesture the product decided | `Controls::kLongTouchMs` and `kLongPressMs` against the presses a hand actually makes |
| `Link` 12 | session, negotiated payload, action (up, down, claim taken or released, received, sent, dropped), endpoint, frame bytes, drops | what a phone negotiates in the field, and where an offload stalls |
| `Traffic` 13 | address, distance, bearing, relative altitude, closing speed, alarm level, source, RSSI, targets tracked | `kAdvisoryDistM` and `kAdvisoryAltM` against the encounters that happened, and what the ageing rules threw away |
| `Write` 14 | placement, kind, how long the change waited, the phase it was placed at, requests, writes, forced | whether `kSettleMs` and `kMaxDeferMs` place a durable write inside its window, and how often one is forced |
| `Screen` 15 | page, mode, prompt, alarm level, how long it had been up, backlight, powered, thermal hold | `ScreenService::kPresentFloorMs` and the refresh cadence against what a pilot was shown |
| `Gap` 16 | records dropped, the span they cover, the total since arming, the ring capacity | nothing. It is the hole itself, written where the hole is |
| `End` 17 | records written in the session, records dropped | nothing. It is the one record that says the session stopped rather than was stopped |

### The battery ladder, and what a replay works out for itself

The `Power` record carries the steps of `core/power`'s ladder the device decided rather than the ones a decoder can re-derive. `caution` is a decision: three consecutive samples under `kCautionMv`, cleared by a cable and by a sample back above it, and it is nowhere in `level` because nothing acts on the knee (`../power/README.md`). `trim_offset_mv` and `trim_learned` are the other one: `power::FloatTrim` reads a charger's float plateau off the untrimmed samples, and the corpus only ever sees millivolts that have already been through a trim, so the number it learned cannot be recovered from any record. Whether the device would adopt it is `Config`'s `battery_trim_manual`, because a trim a person measured outranks a charger's for good.

Both fit where the record already had room: `caution` and `trim_learned` are the last two bits of the flag byte, the offset is payload bytes 13 and 14, and byte 15 is still free.

Four more things the ladder decides are left out on purpose, all of them a decoder's arithmetic rather than the device's: the lamp condition, which `indication::condition_for` computes from the alarm level, the power level, this caution flag and the fix, every one of them already in the corpus; the panel's `battery_low`, which is `level` in two comparisons; `may_write` and `may_refresh`, which are `level` and a supply warning count that is already a field; and the trim window's own workings, the session low and the plateau spread, which are the derivation and not the result. The parked low-cell frame is invisible for a different reason: the capture is closed before the glass is drawn.

### The end marker, and the tail a reader must drop

A 24-byte slot leaves no room for a CRC (the geometry is the flight log's and is not being re-cut), and recovery counts every slot that is not erased. A power cut in the middle of a 24-byte program therefore leaves a slot that decodes: a type byte in range, a plausible instant, sixteen bytes of whatever was programmed before the rail went. Nothing in the record can say it is torn.

So the session says it instead. A capture that stops cleanly - the pilot disarms it, the device powers off, the partition refuses another sector - writes an `End` record as its last record, and the store reports `closed: true` only for a session whose last record is one. A session recovered without an `End` is reported `closed: false`, and **the last record of an unclosed session is the one that may be torn: a reader drops it.** The host decoder does exactly that (`scripts/blip_records.py`), and the loss is one record rather than a corpus a reader cannot trust.

The writer keeps the last two slots of its frontier sector in hand for this: one for the `Gap` that says why a capture stopped, one for the `End` that closes it.

Two payloads carry a code this layer does not own: `Contact::gesture` is the product's `go::Gesture` and `Screen::page` its `go::Page`. The decoder resolves both through the schema, which is why they are listed there by name.

## Where each type is produced

One tap per fact, in the service that owns the field on `bus::State` (`core/bus/README.md` names it), guarded by `armed()` before a payload is built. Paths below are `products/skyblip_go/`.

| Type | Tap | When |
|---|---|---|
| `Boot` | `services/capture.cpp` `record_boot` | as a capture session opens, so a corpus names the build that produced it |
| `Config` | `services/capture.cpp` `record_config` | the same instant, and for the same reason |
| `Gnss` | `services/ownship.cpp` `record_gnss` | every solution the receiver publishes, fix or not |
| `Pps` | `services/ownship.cpp` `record_pps` | every latched edge, and every `kPpsRecordPeriodMs` that brings none |
| `Burst` | `services/traffic.cpp` `log`, `services/radio.cpp` `log_refusal` | every tape entry: what the air delivered, and what policy refused to key |
| `Dwell` | `services/radio.cpp` `record_dwell` | every dwell armed, which is three a second: the two band edges and the M-band hop |
| `Flight` | `services/ownship.cpp` `record_flight` | every evaluation of the flight machine, never only its transitions |
| `Power` | `services/power.cpp` `record_power` | `PowerService::kRecordPeriodMs`, the cadence the cell is sampled at |
| `Baro` | `services/ownship.cpp` `record_baro` | every sample the board polls (`runtime::kBaroPeriodMs`) |
| `Motion` | `services/ownship.cpp` `record_motion` | `OwnshipService::kMotionRecordPeriodMs`, and only where a hub is fitted |
| `Contact` | `services/screen.cpp` `record_contact` | every debounced edge, carrying the gesture the product made of it |
| `Link` | `services/config.cpp` `record_link`, `services/flight_log.cpp` `record_link` | a connection up or down, the claim changing hands, a frame in, a frame dropped |
| `Traffic` | `services/alarm.cpp` `record_traffic` | every reception that reached a target, once the alarm layer has graded it |
| `Write` | `services/config.cpp` `record_write` | every verdict of the durable-write window that is not `Idle` |
| `Screen` | `services/screen.cpp` `record_screen` | `ScreenService::kRecordPeriodMs`, the render cadence |
| `Gap` | `recorder.cpp` `flush_gap`, `services/capture.cpp` `write_gap`, `announce_rotation` | where the ring refused a record, where the partition refused a sector, and where the sector ring recycled one of this session's own |
| `End` | `services/capture.cpp` `write_end` | the last record of a session that stopped rather than died |

`Gnss::reject` is the receiver's own verdict and reaches the tap the way every other receiver fact does: `gnss::FixValidity` lives on the L76K driver, which no `runtime::Context` reaches, so the board publishes its reason and its count onto `bus::State` beside the sky view as it pushes the solution, and own-ship reads them there. The board is a writer of that group by the table in `core/bus/README.md`.

Two fields have no producer on this device and are left at their default rather than filled with a number nobody measured. `LinkAction::Sent` is never written: an outbound frame exists in `RecordPool::send` and in `core/comms`, and neither can see the claim the record carries, so a `Sent` emitted from the one a recorder reaches would cover the log endpoint's chunks and miss every reply the config endpoint sends - a corpus that reads as a device which answered nothing. The action keeps its number rather than being retired, because a decoder has nothing but the number. `Gap::span_ms` is zero on the marker the writer leaves when the partition is full, because the records still queued are the ring's and it does not hand out their instants.

`Write::kind` is always `Settings`: the flight log's own records are not placed through `timing::DurableWriteWindow`, they stand off the direct slot instead (`services/flight_log.cpp`).

## The recorder

```
void arm();                                 a capture starts, and the ring starts empty
void disarm();                              nothing more is accepted, what is buffered still drains
bool armed() const;
bool record(const Record&);                 false when disarmed or dropped
bool record(const radio::Entry&);           a tape entry carries its own instant
template <class T> bool record(const T&, const Instant&);
bool peek(Record& out);                     the head, still in the ring
void commit();                              once flash has it, and not before
int queued() const;
uint32_t written() const;                   records accepted since arming, gap markers included
uint32_t dropped() const;
```

`record()` returns on the armed flag before it encodes anything, so a disarmed device pays one branch per tap. A tap that has to gather its fields first checks `armed()` itself.

The writer peeks and commits rather than taking, because the two failures it can meet are different. A flash that refused the write still owes that record: it stays at the head of the ring and the next pass tries again, where a `take()` in front of the write would have dropped it somewhere no counter can see. Only a record the flash has taken leaves the ring. `flight::LogSession` drains the same way, for the same reason.

A full ring refuses the new record and keeps the history. A corpus is read forward from a known start, and overwriting the oldest record would move the hole to the one place a reader cannot see it. The refusals are counted, the first and last of them are remembered, and as soon as a slot frees the ring writes a `Gap` record in their place: how many were lost and over what span, stamped at the instant the hole opened. That marker costs a slot of its own, so a ring that stays full keeps dropping - which is the honest reading of a writer that has stopped keeping up.

### Why 64

The ring has to cover the worst window a writer can be blocked for, at the rate a busy second produces records.

The blocking window is about a second: the store erases a 4 KB sector when the frontier moves to it, and a writer stands off the direct slot (450 to 1000 ms, §C.5) rather than putting flash work where own-ship keys the PA. The rate, in the worst second: the eleven the next paragraph counts off, plus the air, which is what makes the second busy - a site where the radio tape (16 rows) turns over in a few seconds is a handful of `Burst` records and their `Traffic` updates. Thirty a second is a generous reading of that, and 64 slots is two of those windows, 1536 bytes of RAM.

It is a bound, not a promise: a sky that produces more is a sky whose gaps are recorded as gaps.

`kPeriodicRecordsPerSecond` is the other half of that arithmetic and a much smaller number, eleven, counted off the taps above: seven subjects that write once a second whatever the sky is doing (`Gnss`, `Pps`, `Flight`, `Baro`, `Power`, `Motion`, `Screen`), the three dwells the radio arms as the second crosses its two band edges and its M-band hop, and own-ship's own `Burst`. The ring is sized on the worst second, but how long a partition lasts is a question about the quiet ones, so that is the rate the capture page quotes before a capture is armed. Once one is running the page quotes what it is actually producing instead. `test/products/test_diag_taps.cpp` measures an armed device against that number, so the page cannot go on quoting a rate the taps stopped producing.

## Where the recorder lives

On `runtime::Context`, beside the roles, the bus and the blackboard. It is many-producer and single-consumer, which is neither of the other two: `core/bus`'s queues are one producer each, and the blackboard takes one writer per field. Every service can therefore reach it with one call and no wiring, which is what makes a tap a line rather than a plumbing change, and `Context::instant()` is where a service gets the stamp to hang on it - UTC once the receiver has given a second, time since boot before that, phase only where a PPS edge measured it.

A capture that runs out of partition ends with a `Gap` too, and so does one that outlives its own beginning. The pool rotates: a long capture keeps its most recent hours and the sector ring recycles what is oldest, which can be a sector of the session being written (`../store/README.md`). The writer notices that the moment the allocator decides it, writes a `Gap` naming how many records went with the sector, and re-emits `Boot` and `Config` behind it, so a corpus a reader picks up in the middle still says which build and which settings produced it. The `list` reply calls that session truncated.

## The schema

[`schemas/diagnostics_log.v1.schema.json`](../../../schemas/diagnostics_log.v1.schema.json) is what one decoded record looks like, per type, so the host decoder and this directory cannot drift. It is not `schemas/diagnostics.v1.schema.json`, which is the live status dump the companion link answers with (`core/comms/diagnostics.h`) and a different thing entirely.
