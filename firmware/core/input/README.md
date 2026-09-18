# core/input

A contact, conditioned. `Contact` takes sampled levels plus the time and emits one `Down` and one `Up` per press, with how long the press lasted on the `Up`.

The settle window is the board's, not this file's: a mechanical button bounces for a few milliseconds on both edges, and a fingertip crossing the edge of a capacitive electrode flutters. Neither is anything a hand did, and neither is the same number on two boards, so it arrives at construction (`boards/lilygo/t_echo_plus/pins.h`).

What a press then means is a product's, and it is nowhere near here. The board pushes `events::ContactEvent` and the product reads it: for skyBlip Go that is `products/skyblip_go/input/`, where a tap is a page and a touch held past a second is the way back to the radar. The split is not tidiness. The board instantiates the conditioner, so the conditioner has to sit below `boards/`, while the hold thresholds are a contract with a pilot that a board must not be able to read.

`edge_ms()` is when the level changed, not when the poll saw it, so a product measuring a touch against a hold threshold measures the touch a hand made rather than the moment the loop got round to it.
