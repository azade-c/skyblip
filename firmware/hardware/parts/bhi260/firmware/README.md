# The BHI260AP firmware image

`BHI260AP.fw` is Bosch Sensortec's RAM image for the BHI260AP, vendored unmodified.

| | |
|---|---|
| Source | [boschsensortec/BHI2xy_SensorAPI](https://github.com/boschsensortec/BHI2xy_SensorAPI), `firmware/bhi260ap/BHI260AP.fw`, release v1.6.0 |
| Size | 103,676 bytes, a whole number of 32-bit words |
| SHA-256 | `318def511fd8eb762bf1e61cf02cfd6ecac70f8627d3bacfa7d24a1641f9e915` |
| Licence | BSD-3-Clause, Copyright (c) 2022 Bosch Sensortec GmbH |

It is the RAM variant, not `BHI260AP-flash.fw`: the Plus gives the hub no flash of its own, so the image is uploaded by the host at every boot (`../README.md`).

## How it reaches the part

`generate_inc_file_for_target` in `products/skyblip_go/CMakeLists.txt` turns the binary into a byte list at build time; `hardware/platform/zephyr/imu_firmware.cpp` is the only translation unit that includes it, and the platform hands it to the board as `imu_firmware()`. So the image is linked into the device image only, the host build and the WASM simulator never carry it, and nothing in the repo is a 650 KB generated header anybody could be tempted to edit.

It costs 101 KB of a 724 KB slot. `scripts/size_check.py` is the gate.

## Why it is in the image, and what taking it out would take

The blob is 103,676 B of a device image that measures 447,536 B today, so a quarter of every update pushed over SMP-over-BLE is a file that has not changed since Bosch released v1.6.0 and will not change until we take a later one. The split that fixes that is a blob partition on the external part, a fat image that writes it once and a slim variant whose `imu_firmware()` reads it back:

| | |
|---|---|
| Where it would live | 26 sectors, 0x1A000, carved off the front of `log_partition` (0x14A000 on `&ext_flash`), outside `slot1_partition` so an MCUboot swap never touches it |
| What guards it | the `0x662B` magic the driver already refuses an image without, plus the length and the SHA-256 in the table above, written as a header and checked before the first chunk goes out |
| Who writes it | the fat image, once, from the bytes it is already carrying |
| Who reads it | the slim image, through `hardware/platform/zephyr/flash_region.h`, one 240-byte chunk a pass exactly as the rodata path does now |
| What it buys | 101 KB off every OTA payload |

What it does not buy is room in the slot. The UF2 bootloader's write window is internal flash only, 0x1000..0xEA000 (`boards/lilygo/t_echo_plus/t_echo_plus.dts`), so a drag-and-drop is the only way to provision a board that has never run our firmware, and it cannot reach the external part. Every unit must therefore still be able to boot an image that carries the blob, `slot0` stays sized by the fat one, and `scripts/size_check.py` gates the same number it gates today. Room that exists only in the slim variant is room no feature can spend.

The state to design for before any of this is written is a slim image on a blank blob partition: a new board, a replaced flash part, a log erase that takes one sector too many. The part answers 0x28 and we have nothing to give it, which is a case the driver has no word for. `Absent` is nothing on the bus, `NotFound` is an image without an accelerometer in it, and neither is this, so the panel would draw an empty cage and never say why. That is a fault word, a self-test line, and a way back: a USB re-flash of the fat image, or the config link pushing the blob the way it pushes settings.

What flips it is update time, not flash. At 60.4% of the slot there is nothing to reclaim the space for.

The dead space on the internal part is not worth the same conversation. `softdevice_partition` is 152 KB of S140 we never link against and the UF2 window does reach, but the factory bootloader decides where to start the application from what it finds there, so the reward is the space and the penalty is a unit that only comes back over SWD.

## Replacing it

Take the same path from a later release, check the first two bytes are `2B 66` (the `0x662B` magic the driver refuses an image without), keep the length a multiple of four, and update the table above. The device says whether it worked without a debugger: the six-pack's turn coordinator draws its cage from the part being fitted and the ball from the part reporting, so an image the hub refuses to verify reads as an empty cage a few seconds after boot.
