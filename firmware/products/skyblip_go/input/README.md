# products/skyblip_go/input

Two contacts, four things a pilot can say. What a contact does electrically is the board's and is conditioned in `core/input/`; what it means is here, because the hold thresholds are a contract with a pilot and a board must not be able to read them.

The board pushes one `events::ContactEvent` per settled edge, stamped when the level changed. `Controls` turns those into the three commands this device obeys, and `go::ScreenService` is the only thing that reads them.

| Gesture | Command | On the traffic pages | In the settings mode |
|---|---|---|---|
| pad touched and released under `Controls::kHomeTouchMs` | `Next` | the next page | the next row |
| pad held past `Controls::kHomeTouchMs` (1 s) | `Home` | the radar, and a standing alarm is dismissed | the radar |
| button pressed | `Act` | opens the settings | changes the focused row |
| button held past `power::kLongPressMs` (2 s) | none | off | off |
| pad held through that press | none | off, with a blank panel: the stow | the stow |

One rule in two places: the pad moves, the button acts, and the long touch is the way back to the traffic picture from anywhere. Nothing a pilot learns on the pages has to be unlearned on the rows.

The pad carries the navigation because it is what a gloved thumb finds on the top edge without looking, and the page lands on the release rather than on the contact: that is what leaves room for the long touch in the same finger. A touch the button joins says nothing at all, in either direction, because that pair is already the stow and a device on its way off must not change page on the way.

A touch can also go home without being let go of, which is why `Controls` is ticked as well as read: the pilot's finger is still on the pad when the radar comes back, and a hold that waited for the release would be a second of glass saying nothing.

## The long touch under an alarm

With any graded contact standing, the way home also dismisses it (`core/traffic/README.md`). One gesture and not two, because under an alarm the two mean the same thing: a pilot holding the pad is asking for the traffic picture, and a pilot who is looking at the traffic picture has been told everything the buzzer and the flashing wedge were going to tell them.

It is the pad and not the button. The button's hold is already the way the device switches off, and a pilot silencing an alarm must never be a thumb away from stowing the device that raised it. The press the button does have here opens the settings, which is a page the same alarm has just taken off the glass.

What a dismissal costs if it was an accident is one flight's worth of nothing: the grade stands, the wedge stays on the bearing, and anything worse speaks again. That is what makes a single 1 s hold the right price rather than a gesture a pilot has to be taught.

## The button's third meaning

`ConfirmGesture` is the security boundary, not an input helper. This product ships with BLE pairing off, so nothing proves cryptographically that the phone asking for a firmware upload belongs to the pilot, and physical presence stands in for it. The gesture has to be one a thumb cannot produce by accident and one that cannot be confused with the press that opens the settings or the hold that switches the device off: a double press inside `kDoublePressMs` is the only one of the button's meanings a pilot has to mean to make.

It authorises nothing unless a prompt the pilot can read is on the glass, and a lone press at a prompt refuses the operation rather than leaving it standing. Fail closed, which is what makes "press twice to allow, once to refuse" true on the panel.
