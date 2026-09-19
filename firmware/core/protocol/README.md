# core/protocol

The wire formats, in both directions and with no I/O: `adsl.{h,cpp}` and `adsl_uplink.{h,cpp}` are the ADS-L 4 SRD-860 packet the radio carries, `air.{h,cpp}` the shared air-frame plumbing, `alptas.{h,cpp}` and `nmea_out.{h,cpp}` what a tablet reads over the companion link.

## What ALP-TAS costs us

`$PFLAA` carries an `IDType` with three values, and ADS-L's address mapping table has 64. The mapping is `addr_table_to_idtype`, and it is a lie chosen from a short list of lies.

SkyDemon refuses the sentence for anything but 1 or 2 (`oss/SoftRF-moshe-braner/.../libraries/OGN/ads-l.h:657-658`: "if (AddrType==5) AddrType=1; else AddrType=2; // SkyDemon only accepts 1 or 2"). Leaving an address at IDType 0 draws nothing on that app at all, which is the one failure worth never causing again. Between the two values left, 1 (ICAO) claims a permanent, registry-issued identity; 2 (FLARM) claims a device-class kinship that is at least true of the mechanism, self-assigned and transient. So ICAO is reported as ICAO and everything else becomes FLARM.

What that costs: an OGN-Tracker address (7), which is what this device transmits by default, draws on the tablet as if it were FLARM. That is a lie about provenance too, and a cheaper one than claiming ICAO, because nothing downstream correlates a FLARM ID against an aircraft register the way it might an ICAO one.
