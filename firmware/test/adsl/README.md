# The ADS-L conformance suite

ADS-L 4 SRD-860 issue 2, read clause by clause against this firmware. The normative text is [`docs/reference/ads-l_4_srd860_issue_2.md`](../../../docs/reference/ads-l_4_srd860_issue_2.md), and the PDF beside it is what wins where the two disagree.

Everything ADS-L conformance lives here and nowhere else. The suites under `test/core/` test the code (`test_adsl.cpp` takes the packet over a bit-error channel, `test_timing.cpp` the dwell map); these test the document.

## One case per clause

A case is named `ADS-L.4.SRD860.<clause>: what holds`, so the subject is the clause and `docs/BEHAVIOR.md` groups by it. `scripts/check_adsl_clauses.py` is the gate: every `ADS-L.4.SRD860.x.y` heading in the specification has at least one case, and every case names a real clause.

| File | Clauses |
|---|---|
| `test_general.cpp` | A.1 to A.5, and the performance appendix |
| `test_overview.cpp` | B.1 to B.6 |
| `test_physical.cpp` | C.1 to C.5 |
| `test_datalink.cpp` | D.1 to D.3 |
| `test_network.cpp` | E.1 to E.4.2 |
| `test_presentation.cpp` | F.1 to F.2.5 |
| `test_traffic.cpp` | G.1 to G.1.10 |
| `test_quality.cpp` | G.1.11 to G.1.17 |
| `test_payloads.cpp` | G.2 to G.5 |

## A gap is a skipped case, never a missing one

Where the firmware does not satisfy a clause, the case is still written as the clause requires, marked `doctest::skip()` over a one-line `TODO:` naming why. `build/skyblip_tests --no-skip --test-case="ADS-L*"` runs them all and prints what is not conformant, which is the list of what this device would have to do to claim the clause. Three kinds of skip live here, told apart by the message:

- `not implemented` / a failing assertion: a real gap, the firmware could satisfy the clause and does not.
- `prose:` the clause has nothing executable, such as the block diagram or the OSI table.
- `paperwork:` a duty of the manufacturer rather than of the device, such as the declaration of conformity.

Skipped cases carry no claim, so they never reach `docs/BEHAVIOR.md`: the generated index stays a list of what the suite checks.

## Where the oracles come from

Where the specification prints code or a worked example, the test carries it rather than restating our answer: E.2's XXTEA and E.3.1's CRC-24 are transcribed from the clause and compared against `core/fec/`, G.1.6's exponential encoder is implemented from the algorithm as written, and G.1.7 to G.1.9 assert the exact codes of the clauses' example tables. The G.1 case reads all seventeen fields at the bit offsets of the payload table, so a field that moves by one bit fails there first.
