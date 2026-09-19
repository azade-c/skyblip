# core/protocol

The wire formats, in both directions and with no I/O: `adsl.{h,cpp}` and `adsl_uplink.{h,cpp}` are the ADS-L 4 SRD-860 packet the radio carries, `air.{h,cpp}` the shared air-frame plumbing, `alptas.{h,cpp}` and `nmea_out.{h,cpp}` what a tablet reads over the companion link.

## What ALP-TAS costs us

`$PFLAA` carries an `IDType` with three values, and ADS-L's address mapping table has 64. The mapping is `addr_table_to_idtype`, and it is a lie chosen from a short list of lies.

SkyDemon refuses the sentence for anything but 1 or 2 (`oss/SoftRF-moshe-braner/.../libraries/OGN/ads-l.h:657-658`: "if (AddrType==5) AddrType=1; else AddrType=2; // SkyDemon only accepts 1 or 2"). Leaving an address at IDType 0 draws nothing on that app at all, which is the one failure worth never causing again. Between the two values left, 1 (ICAO) claims a permanent, registry-issued identity; 2 (FLARM) claims a device-class kinship that is at least true of the mechanism, self-assigned and transient. So ICAO is reported as ICAO and everything else becomes FLARM.

What that costs: an OGN-Tracker address (7), which is what this device transmits by default, draws on the tablet as if it were FLARM. That is a lie about provenance too, and a cheaper one than claiming ICAO, because nothing downstream correlates a FLARM ID against an aircraft register the way it might an ICAO one.

## Two things an ALP-TAS frame is given before it is refused

`alptas_correct` runs the erasure correction the ADS-L path has always had. §C.2.1 is Manchester, so a chip pair that decoded to neither symbol is an error whose position the receiver already knows: `fec::manchester_decode` marks it, `protocol::Frame` carries the map, and the frame CRC says which combination of those flips was transmitted. The search is a Gray-code walk over the marked bits with the CRC syndrome of each one precomputed, so a combination costs one XOR rather than a pass over the frame, and a frame no combination repairs is restored to exactly the bytes that arrived. Up to six marked bits, which is 63 combinations; beyond that the burst is refused as damaged. The syndromes are computed rather than tabled, out of the CRC's own linearity: flipping a data bit moves the check by `crc16_ccitt(that bit alone, init 0)`, and flipping a carried CRC bit moves it by the bit itself.

Until 2026-09-19 the ALP-TAS path checked the CRC and threw the frame away, so every burst with one dead chip pair was a `CRC` row while the same damage on an ADS-L frame was corrected and drawn. On a bench that difference is invisible, because nothing at -15 dBm has dead chips. At range it is receptions.
