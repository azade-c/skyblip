# core/indication

The one lamp on this device, and what it is saying. `lamp.h` holds the whole vocabulary as one table, in priority order, and `condition_for()` is that same order written as code.

E-paper holds its last image with the rails down, which is why the wordmark is painted before power off: an off device and a running device look identical, and the panel says nothing at all about whether there is a device there. This is what does.

## Why this is not core/annunciation

Both answer "what is the device telling the pilot", so the case for merging them is real. It loses on four counts.

Annunciation announces EVENTS and every pattern it plays ends by itself, which is the bug that file exists to not have. This announces a LEVEL, it never ends, and the whole design question is what it costs to hold.

The inputs do not overlap. Annunciation's situation is a traffic level, whether it just got worse, and the first fix. This one's is the cell as the cutoff monitor reads it, the fix, and the alarm.

`settings.alarm_enabled` silences the buzzer. It must not darken the lamp: "is this thing on" is not a preference, and a pilot who turned the noise off did not ask to be unable to tell a live device from a dead one.

Arbitration is the opposite shape. Annunciation hands one voice to another and has to decide who may interrupt whom. Here every condition is true or not true at the same instant and exactly one wins, which is a priority order over a closed set: `kTable`, top row first.

So: two files, and this one depends on the other's output (the alarm level) and on nothing else of it.

## What a row may spend

Every row declares whose budget it spends, and a static_assert refuses a row that exceeds it. `Steady` is held for hours on an 850 mAh pack and has to be very nearly free (2%). `Transient` lasts minutes at most, an alarm that stands or a cell about to go (30%). Nothing is held solid: a lamp left on is a real power term, and `every_lit_row_blinks()` is what keeps it that way.

SoftRF is the reference vocabulary and we keep its distinction and not its duty: solid above the low threshold, a 300 ms toggle below it (`src/driver/LED.cpp:204-219`). Healthy is a wink at 1%, and low keeps SoftRF's rate, a 600 ms period, at a tenth of its duty.

## Why charge is not a row

The board already answers it in hardware. The charger IC drives its own LED, documented in LilyGO's T-Echo pin table: lit while charging, blinking with the cell missing or faulty, off when full. It reports while the SoC is asleep, which nothing here can do, and it reads the charger rather than guessing from a terminal voltage.

Until 2026-09-18 this table had `Charging` and `Charged` rows, held solid for as long as a cable was in. They cost the thing the lamp exists for: on a powered install the only indicator of a running device and a valid fix was replaced by a second opinion about the charger. `Charged` was worse than redundant, since it came from a float-voltage threshold (`core/power/battery.h`) and could contradict the LED beside it.

Their priority rule was already dead code. `power::CutoffMonitor::apply` forces `PowerLevel::Normal` whenever external power is present, because the charger holds the terminal above the cell, so "charging outranks low" arbitrated a conflict that cannot occur. A cable in is never `Low`, and that is decided in `core/power`, once.
