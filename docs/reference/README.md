# Reference specifications

Normative sources the firmware is written against, kept locally so a clause citation in `docs/BEHAVIOR.md` or a comment in `firmware/core/protocol/` can be checked without a network round trip.

Both issues are published by EASA as attachments to [ED Decision 2022/024/R](https://www.easa.europa.eu/en/document-library/agency-decisions/ed-decision-2022024r). The firmware targets Issue 2; Issue 1 is here because deployed hardware and most third-party implementations predate Issue 2, and the two differ in the payload set and in Subpart C.

## ADS-L 4 SRD-860 Issue 2

The one the firmware implements. 52 pages, dated 1 December 2025. Adds the Status, Traffic Uplink, FIS-B and Remote Identification payloads, and the O-band LDR/HDR split, to Issue 1's Traffic payload.

- `ads-l_4_srd860_issue_2.pdf`, downloaded from https://www.easa.europa.eu/en/downloads/142821/en
- SHA-256 `114f45966c1b10ab0cbd22dab365a451136ff26952ce8cdcc91740666b76b2aa`
- `ads-l_4_srd860_issue_2.md` is the text conversion

## ADS-L 4 SRD-860 Issue 1

30 pages, dated 20 December 2022, titled "Technical Specification for ADS-L transmissions using SRD-860 frequency band". Its Subpart G names the position payload "iConspicuity", which Issue 2 renames "Traffic".

- `ads-l_4_srd860_issue_1.pdf`, downloaded from https://www.easa.europa.eu/en/downloads/137503/en
- SHA-256 `a547f3b83990f6f335a2c5a622a3b55b736079a6dfea340373c5cf0c8c48bd8a`
- `ads-l_4_srd860_issue_1.md` is the text conversion

## The conformance suite

Issue 2 is read clause by clause by five host suites, one per group of subparts, and every case is named after the clause it checks: `ADS-L.4.SRD860.G.1.7: the altitude encodes the clause's worked examples`.

| Suite | Clauses |
|---|---|
| [`test_spec_physical.cpp`](../../firmware/test/core/test_spec_physical.cpp) | B.3 to B.6, C.2 to C.5, the performance appendix |
| [`test_spec_datalink.cpp`](../../firmware/test/core/test_spec_datalink.cpp) | D.1 to D.3, E.1 to E.4 |
| [`test_spec_header.cpp`](../../firmware/test/core/test_spec_header.cpp) | F.1, F.2 and its sub-clauses |
| [`test_spec_traffic.cpp`](../../firmware/test/core/test_spec_traffic.cpp) | G.1 to G.1.10, and the transmit rate |
| [`test_spec_quality.cpp`](../../firmware/test/core/test_spec_quality.cpp) | G.1.11 to G.1.17 |
| [`test_spec_payloads.cpp`](../../firmware/test/core/test_spec_payloads.cpp) | B.5, G.2 to G.5 |

Where the firmware does not satisfy a clause, the case is written as the clause requires and marked `doctest::skip()` above a `TODO:` saying why. It is a gap, stated in the suite and greppable from it, rather than a missing test: delete the skip and the run prints what is not conformant. Skipped cases carry no claim, so they do not reach `docs/BEHAVIOR.md`.

Subparts A and H, B.1 and B.2 are prose: qualification, documentation, a block diagram and an OSI table, with nothing to execute.

## The conversions

Each `.md` is generated, for grepping and quoting, never edited by hand:

```
pdftotext -layout docs/reference/ads-l_4_srd860_issue_2.pdf /tmp/spec.txt
python3 scripts/spec_to_md.py /tmp/spec.txt docs/reference/ads-l_4_srd860_issue_2.md
```

`scripts/spec_to_md.py` drops page furniture and the table of contents, promotes `SUBPART` and `ADS-L.4.SRD860.x.y` lines to headings, joins wrapped paragraphs, and fences every column-aligned block verbatim. Tables keep the PDF's spacing inside those fences rather than becoming markdown tables: the field-offset tables lose their alignment otherwise. Figures and block diagrams are not carried over.

The PDF is the normative text. Where the two disagree, the PDF wins.
