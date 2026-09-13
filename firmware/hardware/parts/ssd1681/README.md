# The SSD1681 driver, and the two waveforms it runs

The panel is a GDEH0154D67, 200x200, one bit deep, on an SSD1681 controller. Two refreshes reach it and they are not variations of one thing.

## The full refresh

`0x22 0xF7` then `0x20`: enable the clock, enable the analog, load the temperature, load the OTP waveform set for display mode 1, display, disable the analog, disable the OSC. The panel carries 36 of those sets in OTP, indexed by temperature, and one load brings the LUT, the gate and source voltages, VCOM and the end option with it (datasheet section 6.7). That is the waveform that lands a deep black, it takes about 2.6 s, and the driver writes the new frame into both banks before it so the differential update that follows has a truthful previous.

## The partial refresh

The OTP set for display mode 2 is what this driver ran until September 2026, and it greyed the blacks. The first partial after a full lifted every black on the panel, not only the pixels that changed, and each partial after it put some of the black back. Ink that changes state is ink that is being driven, so the OTP mode-2 set drives pixels it was given no reason to drive.

Waveshare do not use it. `epd1in54_V2.py` writes its own partial waveform to `0x32` with the analog levels it was measured against, enables RAM ping-pong so the controller keeps the previous frame itself, powers the panel up as a step of its own, and only then sends frames. That is what `waveform.h` holds and what `enter_partial_mode()` sends:

| command | what it carries |
|---|---|
| `0x32` | 153 bytes: source levels per LUT group, phase lengths, group and phase repeats, frame rates |
| `0x3F` | the end option, `0x02`, which is the power-on default and not an override |
| `0x03` | gate voltage |
| `0x04` | VSH1, VSH2, VSL |
| `0x2C` | VCOM, `0x28`, deeper than the full waveform's |
| `0x37` | ten display-option bytes; byte 5 bit 6 enables RAM ping-pong for display mode 2 |
| `0x3C` | the border, held at VCOM |
| `0x22 0xC0` + `0x20` | clock and analog up, charge pump settled, nothing displayed |

Each frame after that is the new image into `0x24` and `0x22 0xCF` + `0x20`: display mode 2, then disable the analog and the OSC. No load bit, so nothing overwrites the waveform for the life of the mode, and the rails still come down at the end of every frame, which is what keeps the ink from migrating in a cockpit in the sun.

Ping-pong is why only `0x24` is written. The controller moves the frame into the previous bank itself, and a driver that also wrote `0x26` would be guessing at which bank the controller considers current.

The next full refresh loads the OTP mode-1 set over all of it, so partial mode is left on every full and entered again on the first partial after it. A reset does the same, and so does deep sleep.

## Power off

The last thing the glass is shown is always a full-waveform frame, because that is the image it wears for as long as it is off: the wordmark on a long press, blank on a pad touch held with it. Then deep sleep mode 1, which retains RAM. Never a partial, and never rails cut mid-frame.

## Licence

`waveform.h` carries the partial waveform table from Waveshare's e-Paper library, MIT, Copyright (c) Waveshare. The rest of this directory is GPL-3.0-only with the firmware.
