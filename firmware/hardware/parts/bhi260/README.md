# parts/bhi260

The Bosch BHI260AP on the T-Echo Plus sensor bus, at 0x28 (0x29 with the address pin high). It is what puts the ball in the six-pack's turn coordinator, and it is the only part on this board that arrives without a program.

## Why this is not a register map

A BMA423 or an ICM-20948 is a chip with an accelerometer behind some registers: probe it, set a range, read six bytes. The BHI260AP is a sensor hub - a Fuser2 core, a boot ROM, and 100 KB of RAM with nothing in it. Until a host uploads Bosch's firmware image and boots the core from RAM, the part answers its address, names itself `0x89` on `PRODUCT_ID`, and has no accelerometer to read at all. There is no flash on this board for it to boot from itself (`BOOT_STATUS` reports `NO_FLASH`), and the rail it hangs off is switched, so the upload is paid again after every power cycle and after every `SYSTEM OFF`.

That is the whole reason this driver is a state machine rather than four register writes. `firmware/` holds the image and where it came from.

## The sequence

| Stage | What it does | How it can end |
|---|---|---|
| `Absent` | nothing answered 0x28 or 0x29, or what answered is not a BHI260 | the board grants no capability |
| `Idle` | probed and named, waiting for an image | `load()` |
| `Resetting` | `RESET_REQ`, then 100 ms | the settle expires |
| `HostInterface` | polls `BOOT_STATUS` for `HOST_INTERFACE_READY` every 50 ms | ready, or `Timeout` after 2 s |
| `Uploading` | one 240-byte chunk of the image per `service()` | the last chunk sends `BOOT_PROGRAM_RAM` |
| `Booting` | polls `BOOT_STATUS` for `FW_VERIFY_DONE` | `Crc` on the verify-error bit, `Timeout` after 5 s |
| `Configuring` | reads `KERNEL_VERSION`, sets the range, configures the virtual sensor | `Down` if the kernel version reads zero |
| `Running` | drains the FIFO every 200 ms | `Down` the moment the bus stops answering |

Nothing here blocks or sleeps. Every wait is a deadline against the `now_ms` the board already passes down, which is what lets a host test walk the whole bring-up under a clock it advances, and what keeps the upload out of the service loop's way.

### Why the upload is paced, and why the bus runs at 400 kHz

The image is 101 KB. A 240-byte chunk is 5.4 ms of bus at 400 kHz and 22 ms at 100 kHz, and the service loop's pass is 10 ms (`runtime::kServiceStepMs`), so the bus was moved to fast mode in the devicetree rather than the chunk made smaller: at 100 kHz one chunk is longer than the pass it is sent from. One chunk per pass puts the ball on the glass about four seconds after boot, with the radio thread - which is cooperative and higher priority than the loop - untouched throughout.

Chunk framing follows the reference (`BHY2_SensorAPI`, `bhy2_hif.c`): the first packet carries the four-byte command header, `UPLOAD_TO_PROGRAM_RAM` with the length in 32-bit words, and every packet after it is raw image bytes written to the command channel at register 0x00, padded to a word.

### What is configured, and what is read

`ACC` (virtual sensor 4, the bias-corrected accelerometer) at 12.5 Hz with no report latency, range 4 g. The rate is written as an IEEE-754 word because that is what the command takes; this firmware has no floats, so the bit pattern is a named constant and the part model decodes it back in the test. 4 g rather than 2 g because a turn in turbulence pulls past 2 g, and a saturated axis would bend the ratio the ball is drawn from; the resolution it costs is 0.12 mg against the 12.5 mg the ball's pixel is worth.

The FIFO is the non-wakeup one, read exactly the way the reference does: two bytes for the count when nothing is outstanding, then up to 64 bytes a pass until the count is drained. Events are parsed by the size table the reference carries for system ids (padding, the three timestamp forms, meta events, filler) plus the seven bytes an accelerometer frame takes. An id with no size is an id we cannot step over, so the rest of that stream is dropped and the count is on `unparsed_events()`: the next transaction starts on an event boundary again.

## What this part does not do

No gyroscope, no fusion output, no step counter, no wrist gestures, no interrupt line - the Plus leaves HIRQ unconnected, so everything here is polled. The hub is capable of all of it and none of it has a reader, so none of it is configured: the six-pack needs one lateral acceleration, and `core/flight/slip.h` is where that becomes a ball.

## The frame

The driver reports the chip's own axes. Which way the chip sits in the case is the board's fact and lives in `boards/lilygo/t_echo_plus/imu_mount.h`, unmeasured until somebody puts a unit on a bench: the note there is the one place a rotation gets corrected, and nothing downstream has an axis in it.
