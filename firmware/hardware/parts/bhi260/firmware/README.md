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

## Replacing it

Take the same path from a later release, check the first two bytes are `2B 66` (the `0x662B` magic the driver refuses an image without), keep the length a multiple of four, and update the table above. The device says whether it worked without a debugger: the six-pack's turn coordinator draws its cage from the part being fitted and the ball from the part reporting, so an image the hub refuses to verify reads as an empty cage a few seconds after boot.
