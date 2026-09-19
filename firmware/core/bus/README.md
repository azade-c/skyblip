# core/bus

Two things travel between services, and they are not the same kind of thing.

`bus.h` is what **happened**: typed queues, one producer and one consumer each, drained to exhaustion by whoever owns them. An event that nobody popped is lost and counted (`Queue::dropped`), which is the honest outcome for a fact with an instant attached to it.

`state.h` is what is **true now**: a blackboard every service can read and exactly one may write. No queue, no history, no ordering. A reader gets the current answer or the default the field was born with.

The rule that makes the blackboard safe is one writer per field. It is not enforced by the compiler, so it is written down here, and a second writer appearing in this table is a bug report.

The groups are subjects, not owners. A reader wants every barometric fact in one place whoever wrote it, so `baro` holds the pressure the sensor gave and the subscale the pilot set, and the table below is what says those two came from different services.

## Who writes what

| Field | Writer |
|---|---|
| `settings` | `config` at boot (defaults, then the stored blob), `screen` when a pilot changes one in a menu. The flash write itself is `config`'s alone. |
| `own` | `ownship` |
| `clock` | `ownship` for UTC and the PPS edge, the board for `pps_locked` and `ms_since_pps` |
| `traffic` | `traffic` |
| `radio_log` | `traffic` for what was received, `radio` for what was sent |
| `rf` | `radio`, except `rf.timing_stats`, whose PPS half is the board's |
| `air` | `traffic` |
| `power` | `power` |
| `flight` | `ownship` |
| `baro` | `ownship` |
| `slip` | `ownship` |
| `imu` | the board, which owns the sensor hub the ball comes from and is the only code that can see where its bring-up stopped |
| `alarm_level` | `alarm` |
| `panel_presented` | `screen` |
| `started` | the product |

Three entries have two writers and all three are deliberate. `clock` and `rf.timing_stats` are split between the board, which knows when the PPS edge arrived, and a service, which knows what was done with the second that followed. `radio_log` is one ring with two ends of the same conversation in it.

`air.last_tx_done_at_us` looks misfiled and is not: `traffic` is the single reader of `bus.rf`, so it is the only code that sees the executor's `TxDone`, and `radio` reads the instant from here rather than opening a second drain of the same queue.
