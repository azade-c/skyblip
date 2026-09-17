# ui/screens

One file per page, each one a pure function from a snapshot struct to pixels. A page reads nothing, owns nothing and decides nothing: `go::ScreenService` fills the snapshot and the page draws it, which is what lets every page be tested without a device, a bus or a clock.

## The unit every page reads in

`settings::units` is one setting and it reaches every page that prints a distance or a speed: the radar's range ring, `signal`'s slant range, `sixpack`'s speed dial. `Nautical` out of the box, which is nautical miles and knots; `Metric` is kilometres and km/h. The two settings are `Metric` and `Nautical`, not metric and imperial. A knot is a nautical mile an hour and a flight level is a hundred feet of pressure altitude, and neither has anything to do with the imperial system; calling them that invites somebody to add statute miles or Fahrenheit to a page that must never carry them.

Vertical is feet whatever the setting says, on every page that shows it: the altimeter, the vertical speed in feet per minute, the radar's altitude tags in hundreds of feet, `signal`'s relative altitude. A level is cleared in feet and a climb rate is flown to in feet per minute wherever the aeroplane is, so a pilot reading km/h on the speed dial still reads feet on the separation to the aircraft above them.

`status` is the one page the setting does not reach, and that is what the page is for: it has the room for two columns, so it prints the aeronautical figure and the SI one side by side rather than asking which one a pilot wanted. A bench comparing a reading against a phone converts nothing.

## radar

Heading up, not north up. The own ship is drawn nose-up and cannot turn, so the picture turns instead: a target is plotted by how far ahead of the nose and how far right of it it lies, which is the bearing a pilot then looks along. Which way that nose points is not written anywhere on the page: the plot is already nose-up, and `sixpack` is where the track reads in figures.

### The three symbols, and the tag beside them

Traffic is drawn the way a pilot has already learned to read it, in TCAS's shapes: a hollow diamond for an aircraft that is out there, a filled diamond for one that is close, a filled circle for a traffic advisory. The shape comes from the grade `core/traffic/alarm.h` put on the target and from nothing the page computes for itself, so the glass and the annunciator can never disagree about which aircraft matters: `traffic::Level::None` is the hollow diamond, `Info` the filled one - the alarm layer's info ring, 3 km and a 300 m vertical window - and `Important` or above the circle.

There is no resolution advisory here, and there never will be. An RA is an instruction to climb or descend, coordinated with the other aircraft so the two are not told to do the same thing; this device has no link to coordinate over and no authority to give one. The loudest thing it can say is look, so the urgent grade reads as the same circle as the important one, and the extra urgency goes where it is already spent: the bar across the top of the glass, the lamp and the sounder. The circle is a pixel wider than the diamonds because it is the one symbol that has to survive being glanced at.

Beside each symbol is its relative altitude in hundreds of feet, signed, and that tag sits above the symbol when the traffic is above and below it when it is below. The position is the reading: a pilot knows which way to look before the digits are read, and the digits then say how far. Level traffic reads `00` without a sign, the same way a zero rate does on `sixpack`, because `+00` and `-00` are the same separation. Feet whatever `settings::units` says, for the reason the altimeter is in feet: a level is cleared in feet everywhere.

The tag is set at double height, the page's 5x7 font drawn at scale 2, which is a 10x14 digit. It is the figure on this page most likely to be read out of the corner of an eye while the aeroplane is being flown, in sunlight, at arm's length, through a screen protector, and at single height it was a thing you had to stop and look for. The figure stops at 99 hundreds either way, the way a TCAS tag does: a third digit is another character cell of tag on a 200 px glass, and nine thousand nine hundred feet of separation is already not a factor.

A tag that size is no longer free to sit anywhere. At double height it is 18 px tall and 38 to 52 wide, four times the glass the small one held, so the page places it rather than just centring it. Centred over the symbol is still the first choice, and the only one when the target reported no velocity. When it did, the tag is pushed to the side away from where the aeroplane is going, because a tag centred over a target tracking up the glass clears the whole minute of leader line underneath it, and a target flying at you is the one whose line is worth keeping. Pushed, the tag's inner edge stands on the symbol's own column, so the digits and the line grow out of the same point. A tag that would hang off the edge slides inboard to the same 4 px margin the labels keep instead of being dropped: a target out in a corner is exactly the one whose height a pilot wants, and there is nothing out there for the tag to land on.

A line runs out of each symbol to where the aircraft will be in a minute, at the speed and track it last reported, turned into the same nose-up frame the plot is drawn in. A minute because it is the horizon a pilot can act inside and because it makes the length mean something at a glance: a line half the way to the ring is an aeroplane crossing two miles in the time it takes to look up. It stops at the ring rather than running off the glass, so a fast target far out reads as a line that reaches the edge instead of one that reads as a scratch across the footer. A target that reported no velocity, which is every aircraft relayed by a ground station, gets no line, and neither does one that is not moving: there is nothing to project, and a stub pointing nowhere would be an invention.

Own ship gets the minute too, as a dot off the nose at the distance rather than a line out to it, and a second dot at two minutes behind it. The direction is already the whole picture - the plot is nose-up - so a stalk out of the aeroplane drew the one thing the page never needed to say, and read as a mast. What is worth reading is the distance, and a dot at it can be measured against a target's line: where the line reaches the dot's range, the two aircraft are in the same place at the same time. Two pixels square, on the 99|100 pair the way the ship and the ring are, so they move straight up and down the centre line as the speed changes and never appear to drift off it. Two dots and not a scale of five: the pair is what gives the picture a rate, since the gap between them is the same minute again, and a third would be a ruler nobody reads at arm's length. A dot whose minute falls outside the ring is dropped rather than parked on the stroke, because a mark held at the edge would read as a speed the aircraft is not doing.

They are only struck when there is an aircraft inside the ring. A dot is a scale, and a scale is read against something: on an empty ring there is nothing to measure, and two marks standing off the nose with no traffic anywhere would be ink a pilot learns to ignore, on a page whose whole job is that ink means an aeroplane. Traffic out in the corners does not bring them back, for the same reason it is not in the count: the ring is what the page is graduated in. The moment one crosses it, so do the dots.

The dots are the only marks on the page driven by own speed alone, so they are also the ones that can refresh the glass with nothing else happening. The first steps a pixel every `range_nm * 0.65` kt, 2.6 kt on the 4 NM ring, and the second at half that, and the panel is only presented when the frame actually differs and at most once a second, so a pixel of drift costs one partial refresh. There is none without a fix, without a ground speed, or where the minute puts it inside the propeller.

A chevron follows the figure when the target is climbing or descending at 500 fpm or more, the TCAS threshold, pointing the way the aircraft is going. It is drawn as pixels rather than set in the font, which has no caret: 10 across and 6 down, two strokes thick so it carries the weight of the doubled digits beside it, and its apex is a pixel pair on the arrow's middle rather than a single pixel off to one side, the same reason the ship straddles 99|100. It is only ever drawn from a rate that was actually reported - a ground station's relay carries no climb rate, and a flat arrow would be a claim about an aeroplane nobody has measured.

The tags are drawn in one pass and the symbols in a second, over them. Each tag clears the glass under itself so the range ring does not read through the digits, and that is also what makes a tag this size dangerous: it erases whatever was drawn before it. So it keeps off three things. Own ship, because the aeroplane in the middle is the one mark on the page that is true whatever the radio heard. Every target's symbol, because the symbols go down last and a filled circle stamped through a tag's digits reads as neither of them. And every tag already placed, with a character cell of clearance either side, because two tags that touch read as one longer number attached to nothing. The minute dots are struck before the tags for the same reason: a 2x2 dot stamped into a digit is a 3 that reads as an 8, while a dot a tag has cleared is a scale mark with a tag standing on it.

Where no placement clears, the tag is dropped and the symbol still drawn: a tag whose minus sign was cleared away by its neighbour reads as traffic a thousand feet above when it is a thousand feet below, and no tag at all is the honest half of that. Tags go down loudest first, by the grade `core/traffic/alarm.h` put on each target, so a clash costs the quiet aircraft its figure and never the advisory's. The symbols themselves are never suppressed, so the count in the footer and the shapes on the glass still agree.

Nothing is written across the top of the glass. The sector a pilot is flying into is the one a target has to be seen in, so the half of the picture above the ship carries the plot and nothing else - which is also why there is no compass rose: four 5x7 letters are not read at arm's length in daylight, they each cost the plot a patch of cleared glass, and a pilot reading a nose-up picture is not steering by them.

The flight time reads in the bottom-left at double height, with the state it is being accrued in over it in the small font: `FLIGHT` over a running clock, `GROUND` over a stopped one, `NO FIX` when there is no position. `H:MM` needs no unit and no `TIME`, because a colon between two figures under a word that says which state it counts is a clock, and it reads `-:--` until there is a flight to time, the shape of the reading it is standing in for rather than a row of dashes. The track had this corner and it went: a heading-up plot says which way the nose points without spending a number on it, while the time since takeoff is the one figure a pilot copies into a logbook and cannot recover from anything else on the glass. It is the clock `sixpack` shows, off `flight::FlightTimer`.

The range sits in the middle of the footer, on the bottom of the ring, with a clear patch of glass around it: `4` at double height with `NM` in the small font beside it, or `7.4` and `KM` for a pilot who asked for metric. The circle is the same circle either way: the ring is a geometry, four miles of sky around the aeroplane, and it is the label that converts rather than the scale that moves under a habit. It is the label of the circle it stands on, so it stands on it - a scale written off to one side is a second thing to find, and the patch is what keeps the stroke from running through the digits. It is also the only reading on the page that is a setting rather than a measurement: it changes when a thumb changes it and at no other time.

The glass is wider than the ring and the plot uses all of it. The circle is 92 px for the range in the footer, and the panel reaches 99 px abeam and 141 px into a corner, so traffic keeps being drawn out to about 4.3 NM beside and 6.1 NM diagonally on the 4 NM setting. Those corners are peripheral vision: something is out there, in that direction, further than the ring. They carry no scale, which is why they carry no count either, and why a target that leaves the ring changes the footer figure without leaving the glass. The bottom band is the footer's, and traffic outside the ring keeps off it rather than being drawn and then wiped by the clock; an aircraft inside the ring is never suppressed for any label.

One ring, two pixels thick. There was a second one at half range, and it went: a lone target next to two concentric circles is read as which ring it is near rather than as where it is, and the ring that carries the scale is the one that has to survive a glance in sunlight through a scratched screen protector. What that ring gained instead is weight. A single-pixel circle on this panel is the first thing to disappear at arm's length, and thickening it costs no glass, where widening it would cost the margin every label on the page keeps.

It is not `fb.circle`, and it is not a Bresenham arc either. A midpoint circle is only 8-connected: along the diagonals it steps a pixel across and a pixel down at once, and two of them nested leave white specks between the steps that a 200-pixel panel shows as a dashed ring. So the ring is solved a row at a time instead - each row gets the chord between radius `r` and radius `r - 2`, drawn as one `hline` per quadrant - which is a solid annulus of the same two pixels everywhere, at any radius, with no case to get wrong at 45 degrees. The chord is worked in half-pixels, `(2a+1)^2 + (2b+1)^2 <= (2r)^2`, because the ring is centred on the 99|100 point and a pixel's centre is therefore half a pixel out from the offset that names it.

The ring is four nautical miles, and the range is carried in whole miles rather than in metres: the label is then the setting itself, and no rounding stands between what a pilot picked and what the glass says. The kilometre label is that same range converted and carries a tenth, because a whole number there would be a ring the footer is half a kilometre wrong about. Miles because that is what a pilot's other instruments and the airspace around them are marked in, and four because at the speeds this device is flown at a head-on conflict entering the ring is around two minutes away. Metres are the plot's business, one multiplication further in.

The count on the right is the ring, not the receiver and not the glass: the aircraft inside the circle the footer names, so a target drawn out in a corner is not in it, and `signal` is where everything heard is listed. It is the biggest figure on the page, a size above the clock, because aircraft in the ring is what a pilot is here for and the clock is read once a leg. `ACT` stays in the small font beside it: the count is what is read, the word only says what it counted.

That count reads `-`, not `0`, until the page can make the claim: an SX1262 on the board, PPS locked so the dwells fall where the protocol says, and a position to plot against. Given those, `0` is a measurement and says the ring is clear - a quiet band is not a fault, and the radio is trusted to be listening rather than made to prove it with a frame. Without them an empty sky and a radio that never started look identical on the plot, and `0` would be the half of that the page cannot know. One dash and not three, at the size of the figure it stands in for, because it stands where a single digit stands and the footer should not shift when the first aircraft arrives.

The three of them share one baseline, which stands the same four pixels off the bottom of the glass as the outer two stand off its sides, because type flush to the edge of a round window reads as something that fell off. The state word is the only thing off it, stacked over its clock rather than beside it: at double height the words `NO FIX` and the widest clock together are wider than the glass has left between the range and the count.

## radio_log

The station log, newest at the top, `radio::Log::kCapacity` rows and no more: what scrolls off the bottom is gone, because the page is a tape of what is happening now rather than a history.

Each row is one burst.

```
12:34:56 RX M A 3FA21C -87     an ADS-L frame from 3FA21C
12:34:56 RX M F 4C11A0 -93     an ALP-TAS frame, same dwell
12:34:55 RX M BAD       -101   a burst that arrived and never framed
12:34:55 TX M SENT             own-ship's burst left the antenna
12:34:53 TX M LOST             armed, and the radio never reported it sent
```

The columns are the stamp, the direction, the band the dwell was armed for, the verdict, the emitter's address and the level it arrived at. `BAD` is the one that matters: a burst reached the dwell and did not become a frame. It is the only reading on the device that separates an empty sky from a receiver that hears everything and frames none of it, and that second case is a real fault that once shipped, see `git log core/protocol/air.cpp`.

The stamp is UTC as `hh:mm:ss` once the receiver has given us a second, and `T+<seconds>` since boot before that. Two shapes rather than one, so a reading is never taken for a wall clock it is not. A bench indoors never gets a fix and would otherwise have a column of dashes.

The GNSS line is on this page for the same reason the log is: a radio that hears nothing and a radio that is not being told where it is read identically on every other page. The solution count beside it is the one that separates a receiver saying nothing at all from one saying it cannot see the sky.

## sixpack

Six dials over the GNSS-derived own-ship state, each with its own number: ground speed, flight time, altimeter, turn coordinator, track, vertical speed. The snapshot arrives in knots, feet and feet per minute, because the bank and flight-path geometry is worked in them.

The top middle is the one dial whose face and whose number are different quantities. The face is the artificial horizon, pitching with the flight-path angle and rolling with the bank inferred from the track rate: neither is sensed, there is no gyro on the board, and it is kept because a 1-bit panel with a checkerboard ground on it reads as an instrument rather than as six empty circles. The number above it is the flight time, which is the one thing on this page a pilot writes down.

The reference symbol in front of it is two wing bars and a dot between them. On the instrument it copies the bars run from 0.22 to 0.49 of the face radius and the dot is about twice their weight (measured off `Attitude indicator level flight.svg` on Wikimedia Commons); here they reach 0.65 and the dot is five pixels across, because a bar of nine pixels and a dot of three read as two dashes and a speck rather than as an aeroplane. The dot is not decoration: on a real instrument the bars and the dot are each about two degrees of pitch, so it is what the horizon is read against once the bars are the thing being levelled.

Its title is the state, not the quantity: `FLIGHT` over a running clock, `GROUND` over a stopped one, `NO FIX` when the receiver has no position at all. `NO FIX` rather than `NO GPS FIX` because it is the word the radar page's clock is labelled with in the same situation, and because GPS is one of the constellations this receiver uses rather than the name of what it is doing. A duration under a word that says which state it was accrued in needs no unit and no `TIME`, and the pair answers a question the rest of the page cannot: whether this device thinks it is flying, which is also what gates the transmit rate and the update lockout. The figure is `H:MM` since the takeoff `core/flight/state.h` declared, frozen at the landing, off `flight::FlightTimer` (see `core/flight/README.md` for what the clock does across an outage and a second takeoff). Minutes and not seconds, because it is copied into a logbook and because a seconds field on glass that refreshes every few seconds is a clock that is always slightly wrong.

A landing keeps the figure and changes only the title, so it is still readable while taxiing in, which is when it is wanted, and the next takeoff carries on from it rather than starting again. `-:--` means nothing has flown since the device was switched on, which is the one case where the dial has a title and no number: the clock withholds in its own shape, where the five dials around it withhold with `---`.

It replaced a derived QNH, the one place on the device that number was shown. What that was worth is in `git log`: the derivation assumed the ISA lapse rate all the way down to the sea, so it was the setting that would make this altimeter agree with GNSS rather than the one a controller would give, drifting about 1.5 hPa per 1000 ft in cold air. The subscale a pilot flies on is still theirs to set, on the settings page.

### The two scales that are not linear

The speed dial rests at the bottom: zero hangs the needle straight down, and it sweeps clockwise from there, up the left of the glass and over the top, the way a car's speedometer runs. 1.8 degrees per knot, eight marks 45 degrees apart, full scale at 175 kt. That puts 100 kt straight up, which is the landmark worth having, because this dial is read by needle angle long before it is read as a number.

Every number on it falls out of the geometry rather than being chosen: eight marks over seven intervals is 25 kt each, the metric scale that covers the same arc is 315 km/h, and seven intervals of that is 45 km/h each. Both are round without being made round. The eight marks sit on the eight 45-degree rays, the only angles a rasteriser draws as an exact line rather than a staircase, and full scale lands on one of them.

Zero belongs at the bottom because the arc past full scale has to go somewhere, and the top of a 31-pixel dial is the part a glance actually lands on. With zero at the top the shading sat across the most legible arc on the face to say nothing at all; hanging the needle at rest instead puts the dead arc at the bottom right, where the only thing it costs is a corner. A pilot who asks for `Metric` gets 330 km/h full scale rather than a round tick value, because 178 kt is the same arc for the same aeroplane and the needle position they learned has to survive the switch.

The vertical speed dial stands +1000 fpm straight up and hangs -1000 straight down: a climb-out and a circuit descent are both flown at about that, and a vertical needle is read without being interpreted. Getting the knee exactly on the vertical is what makes the scale non-linear at all, 90 degrees for the first thousand against 80 for the second. Marks every 500 fpm, long at the thousands.

Both dials shade the arc their scale cannot reach: past 180 kt on the one, the 20 degrees about the horizontal between +2000 and -2000 fpm on the other. Same checkerboard on both, so the ink means one thing on this page, the scale does not go here.

It is a seven-pixel band on the rim, not a pie slice from the hub: a narrow wedge tapers into the hub and reads as a second needle pointing the wrong way. The band runs out to the ring itself, and the ring is its outer edge - an arc drawn a pixel inside the ring is a second stroke beside the first, and at this radius two strokes a pixel apart read as one thick smear rather than as an edge.

The checkerboard is the whole marking: no outline round it, no arc under it. It had both until the fill was made to agree with them. The fill takes each pixel's angle from `iatan2` while every stroke on the face is placed by cordic, and the approximation was eight degrees out between the cardinals, so the shading spilled past its own ends and the vario's ends sat inside the shading rather than closing it. With that fixed the fill lands where the scale says, and an outline over an edge that is already in the right place only doubles it.

The speed dial's ends are the reason full scale is 175 kt and not 180: at 175 the band runs from the vertical at the bottom to the 135-degree diagonal, so both its edges are exact rays, and each is a scale mark as well. The vario cannot have that. Its band is the 20 degrees about the horizontal, and no clean ray sits anywhere near 10 degrees off one, so the ends are taken where the scale puts them; over seven pixels those two are all but horizontal, which is why the vario's patch reads as a block and the speed dial's as a wedge.

The scale marks run from the rim inwards and stop there, so every one of them touches the ring and none crosses it. Where a mark and the end of a shaded arc fall on the same angle, and on both dials they do, the mark is the solid line that closes the checkerboard at that end.

The needle can land in the shading, since a stopped aircraft parks it on that very edge, so it clears a white channel through the checkerboard on both dials. Neither sector is typed in as an angle: one is the scale's own span, the other is `vsi_deg()` called on its own full scale, so moving a full scale moves its shading with it.

### With no fix

Every needle parks at zero rather than being left off: speed and altitude standing up, the vario level, the wings level, the horizon flat, the card showing north. An instrument with no needle at all reads as a broken instrument, and the six of them together read as a device that has crashed rather than one that is waiting for satellites.

The numbers do not follow the needles. They stay `---`, because a needle at rest is a position and a number is a claim: parking the digits at `0` would say the aircraft is stationary at sea level on a track of north, which is a reading and not a rest state. The needles say what the instrument is doing, the numbers say what is known.

The flight time is the exception, and it is why the title carries the state. A fix lost in the air parks five dials and freezes the clock, so `NO FIX` over `1:35` says both that the aircraft has flown for an hour and a half and that nothing on this page is being told anything right now. Blanking it would throw away the one figure the outage cannot make wrong.

A track due north reads `360`, not `000`. It is what a pilot says on the radio and what every other instrument in the cockpit shows, and `000` is nobody's heading.

The page reads outward from the middle. The six faces are a block: 66 px between centres in both axes, so the same 3 px of glass between two dials side by side and between the two rows, so the panel reads as one instrument rather than as two shelves. Each row's numbers and labels are stacked off its outer edge: number first at double size, label above or below it in the small font. Nothing is written between the rows, which is what pays for both the larger faces and that spacing, and the number a pilot glances at is then the biggest thing on the panel rather than a line of 5x7 text wedged under a needle.

`settings::units` decides the speed dial and nothing else here: knots, the unit the scale below is graduated in and the one this page is designed around. Altitude stays in feet and vertical speed in feet per minute, so the speed is the one place on this page a habit is asked for, which is the rule the section at the top of this file states for the device.

A rate of zero prints as `0`, without the sign the other rates carry. `+0` and `-0` are the same number, and a sign a pilot's eye has to discard is a sign that should not have been drawn.

The turn coordinator's symbol is an aeroplane seen from behind, the way the instrument draws it: wings out to three quarters of the face radius, a fuselage on the hub, a tailplane sitting on the fuselage and a fin standing just above it. It was a bare line through a hub, which on a page of needles read as one more needle.

Four marks on the face and nothing else. Two at the horizontal, where the wings sit in level flight, and two twenty degrees below them, where the wings sit in a standard rate turn - three degrees a second, two minutes for the full circle, which is what the L and R doghouses on the real instrument mean and what the 20 degrees is measured off (`Turn coordinator - coordinated.svg`, Wikimedia Commons). The twelve-tick ring this dial used to wear was the default for a scale, and this instrument has no scale: it has one graduation a pilot flies to.

The symbol leans with the turn rate, not with bank: 20 degrees of lean per 3 deg/s, hard over at 45. That is what makes the marks mean anything. It used to lean by the bank inferred from the rate and the speed, which put the same turn at a different place on the dial at a different airspeed, and left the marks standing for a bank angle nobody had asked about. Bank is still inferred, and the horizon is where it is shown.

The title carries its unit, `TURN D/S`, because a number in degrees per second beside a picture of an aeroplane needs saying which it is. The tenth of a degree per second the digit would suggest is not there to show: `core/flight/turn.h` differentiates a cordic9 track, 45/64 of a degree per unit, over a one-second window, so the smallest step the sensor can express is 0.7 deg/s.

Under the symbol is the inclinometer: two cage lines a ball's width apart, and a nine-pixel ball that slides 16 pixels either way, at its stop at 0.2 g across the wings. Sixteen and not more because the measurement was taken: at 18 the ball's pixels reach the ring, at 16 there is a clear 2.8 px of glass, and a ball touching the rim reads as a ball stuck to it. The geometry is the instrument's own - the ball sits 0.54 of the face radius below the centre and is 0.13 of it across (`Turn coordinator - coordinated.svg`, Wikimedia Commons).

The cage is drawn only on a device that has an inclinometer to fill it, and the ball only when that sensor has a reading. A plain T-Echo has no IMU at all, so the lower half of its dial is empty glass rather than a cage with nothing in it: an instrument that cannot measure a thing does not print its scale. The Plus carries a BHI260AP, which is a sensor hub rather than an accelerometer - no acceleration registers to read, its firmware uploaded over I2C at every boot and its FIFO parsed - and nothing here does that yet, so a Plus shows the cage empty. An empty cage is the state a real one is in until its sensor answers, where a ball parked in the middle would say every turn is coordinated, which is a claim about the rudder no GNSS receiver can make.

The card reads `TRK`, not `HDG`. It is GNSS course over ground, referenced to true north: there is no magnetometer on the board, and no magnetic variation model to turn true into magnetic, so labelling it a heading would claim a sensor and a datum the device does not have. In a crosswind it differs from the heading the compass shows, which is the pilot's to reconcile.

## status

Every reading on this page is a measurement except two, and those two are states a pilot has to be able to read without knowing what is inside the box.

The first row is the receiver. `GNSS 3D 9 SAT` when there is a fix, `2D` under four satellites because four is the fewest that can solve for altitude whatever the receiver calls its solution, and `GNSS NO FIX` when there is not: the label names the sensor and the value names the state, where `FIX` as a label read like a claim the page was not always able to make. The satellite count goes with the fix rather than reading `--` beside it, because the count a receiver reports is satellites used in the solution, and there is no solution to have used any.

The barometer row reads the sensor, not a rounding of it: `1013.252 Q1013 hPa` is the BME280's own pressure to the tenth of a pascal beside the subscale the altitudes under it are read against. A pascal is nine centimetres of altitude at sea level, so a page that printed whole hectopascals threw away three digits the part measures and left a bench with no way to see the sensor breathing. The subscale contracts to the METAR's own `Q1013` to make room, and the one `hPa` at the end serves both numbers.

The vertical speed is the measurement too, in millimetres per second, not the 0.125 m/s the radio transmits: that unit is 24.6 ft/min wide, so a page driven off it would step the needle in 25 fpm jumps that no barometer put there. `core/flight/README.md` has what the chain resolves.

The last field of the traffic row is `TX ON` or `TX OFF`, and it answers the question the page exists for: is anyone being told where this aircraft is. It used to read `PPS OK`, which named a pin on a part, was not a thing a pilot could act on, and was not on its own enough to put a burst on air. What it reports now is `timing::own_ship_transmits`, the same predicate `RadioService` refuses an attempt with, so the glass cannot disagree with the radio. When it reads `OFF` the reason is the row above: no fix, or no UTC. The one case it does not distinguish is the 20 s settle after acquisition (`gnss::kFirstFixSettleMs`), which passes on its own. PPS lock itself is support's business and lives in the timing report.

## settings

The panel half of "a pilot with no phone can change the things that matter". It is a list of rows a thumb walks and a small editor that decides what a press means, both pure: the page takes a snapshot, the editor takes the values in and hands new values back, so the service owns the state and the file owns the meaning.

The two contacts mean here what they mean everywhere else: a tap of the pad moves the focus down a row, a press of the button acts on the row the focus is on, and every further press steps the same field again, which is what makes a subscale settable with a thumb. No timing to get right, so nothing here can be produced by accident out of the hold that switches the device off, and a standing prompt takes the button away from this page entirely before the gesture that answers it can be armed.

A pilot cannot get stuck here: the rows only ever advance and the tap past the last one leaves, the button on the `Leave` row leaves, a long touch of the pad goes back to the radar, and a page nobody has touched for `kIdleReturnMs` shows the traffic again on its own.

## The others

| Page | What it answers |
|---|---|
| `sixpack` | what own-ship is doing: speed, altitude, vertical speed, track, turn |
| `status` | what the sensors say: fix, position, pressure, battery, UTC, and whether we transmit |
| `signal` | every emitter heard, nearest first, with the e.r.p. its level implies |
| `settings` | the values a pilot can change without a phone |
| `boot`, `confirm`, `installing` | the three moments that are not pages: coming up, being asked, being written |

`go::Page` lists them in the order the pad walks, and a long touch of the pad goes back to `Radar` from any of them. Settings is not on that walk at all: it is a mode the button opens, and alone among the pages it has no bit in `settings.page_mask`, because that is where the mask is changed and a mask that hid it would be one nobody could undo without a phone.
