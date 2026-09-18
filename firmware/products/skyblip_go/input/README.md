# products/skyblip_go/input

Two contacts, four things a pilot can say. What a contact does electrically is the board's and is conditioned in `core/input/`; what it means is here, because the hold thresholds are a contract with a pilot and a board must not be able to read them.

The board pushes one `events::ContactEvent` per settled edge, stamped when the level changed. `Controls` turns those into the three commands this device obeys, and `go::ScreenService` is the only thing that reads them.

| Gesture | Command | On a page | In a menu |
|---|---|---|---|
| pad touched and released under `Controls::kHomeTouchMs` | `Next` | the next page | the next row, and off the last row the page it belongs to |
| pad held past `Controls::kHomeTouchMs` (1 s) | `Home` | a standing alarm is dismissed, and with none standing, the radar | the radar |
| button pressed | `Act` | opens this page's menu | changes the focused row, or opens the page it names |
| button held past `power::kLongPressMs` (2 s) | none | off | off |
| pad held through that press | none | off, with a blank panel: the stow | the stow |

One rule in two places: the pad moves, the button acts, and the long touch is the way back to the traffic picture from anywhere. Nothing a pilot learns on the pages has to be unlearned on the rows.

The pad only ever moves forward and it never dead-ends. The pages it walks are a closed rotation of three (`../pages/README.md`), the rows of a menu are not: past the last row it hands the glass back to the page the menu belongs to, so the thumb that opened a menu closes it with the same gesture rather than hunting for a way out.

The pad carries the navigation because it is what a gloved thumb finds on the top edge without looking, and the page lands on the release rather than on the contact: that is what leaves room for the long touch in the same finger. A touch the button joins says nothing at all, in either direction, because that pair is already the stow and a device on its way off must not change page on the way.

A touch can also go home without being let go of, which is why `Controls` is ticked as well as read: the pilot's finger is still on the pad when the radar comes back, and a hold that waited for the release would be a second of glass saying nothing.

## The long touch under an alarm

With any graded contact standing, the hold dismisses it (`core/traffic/README.md`) and stops there. Silence is what the pilot is asking for with a buzzer going, and it is what the gesture spends itself on: the page they are on is left where it is, and the way home waits for the next hold, which is a hold made in quiet. An alarm loud enough to be worth turning the head for has already brought the radar with it (`../pages/README.md`), so the hold that silences it is almost always made on the traffic picture anyway.

What that buys is a glass that stands still at the moment it is being read. The radar is asked for by name and the service ignores a request for the page already on it, so neither the dismissal nor a hold made on the radar costs the 360 ms of black every screen change goes through. A pilot holding the pad at a converging glider gets the sector, the tone and the lamp out, and the plot they were reading stays on the glass throughout.

It is the pad and not the button. The button's hold is already the way the device switches off, and a pilot silencing an alarm must never be a thumb away from stowing the device that raised it. The press the button does have here opens the radar's menu, which is a page the same alarm has just taken off the glass.

What a dismissal costs if it was an accident is one flight's worth of nothing: the grade stands, the traffic stays plotted, and anything worse speaks again with its sector and the lamp back. The touch covers the aircraft the device has already spoken about and no others, so the next one heard arrives with its own voice. That is what makes a single 1 s hold the right price rather than a gesture a pilot has to be taught.

## The button's third meaning

`ConfirmGesture` is the security boundary, not an input helper. This product ships with BLE pairing off, so nothing proves cryptographically that the phone asking for a firmware upload belongs to the pilot, and physical presence stands in for it. The gesture has to be one a thumb cannot produce by accident and one that cannot be confused with the press that opens a menu or the hold that switches the device off: a double press inside `kDoublePressMs` is the only one of the button's meanings a pilot has to mean to make.

It authorises nothing unless a prompt the pilot can read is on the glass, and a lone press at a prompt refuses the operation rather than leaving it standing. Fail closed, which is what makes "press twice to allow, once to refuse" true on the panel.
