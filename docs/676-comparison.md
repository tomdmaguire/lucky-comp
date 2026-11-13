# Behringer 676 against Lucky Comp FET

Measured 2026-11-13. Hardware settings, files and caveats are in `measurements/676/README.md`; Lucky Comp runs are
`measurements/676/lucky-<settings>-c0-<test>.csv` (Mode FET, Level match off, Mix 100%, Colour 0, so the detector
alone) and, after the change below, `measurements/676/after-fet-change/`. Lucky Comp's threshold was set to put the knee where the 676's
was (−16 dBFS in at Ratio 4, −8 dBFS at Ratio 20); results are compared at equal gain reduction (GR), so absolute
levels don't matter.

One Behringer unit is not an 1176: it has transformers and modern parts, and Moore found real 1176s differ by 2× in
distortion. The 676 is used to fill the gaps between published figures (`docs/colour-sources.md`), never to override
them.

## The anchor: Moore's published point

1 kHz, about 7 dB of reduction, fastest attack and release, 20:1.

| | THD | H2 | H3 | H5 |
|---|---|---|---|---|
| Moore, 1176 Blackface / reissue | 0.16% / 0.37% | | "around −58 dBc" | |
| 676 (7.0 dB GR) | **0.17%** | −60 | −58 | −69 |
| Lucky Comp, Colour 0 (6.6 dB GR, detector alone) | 0.13% | | −58 | −66 |

They agree. The 676 matching Moore's Blackface figure is what justifies using its shape elsewhere.

## Finding 1: FET H2 while compressing

H2 rises as compression starts, peaks around −62 dBc at 3–6 dB of reduction, then **falls** with deeper reduction.
Lucky Comp's earlier figure (with no published source) kept rising without limit.

1 kHz, Attack 3, Ratio 4 (H2, dBc):

| GR (dB) | 676, slowest release | 676, Release 7 |
|---|---|---|
| about 2 | −65 | −66 |
| about 4 | −63 | −62 |
| about 6.5 | −64 | −62 |
| about 9 | −70 | −65 |
| about 11 | −82 | −75 |

The fall fits AN105: the feedback that cancels the
FET's even-order distortion works harder as the FET conducts more.

At Ratio 20 (fast), H2 is a little higher (−57 to −60 dBc from 1 to 7 dB GR) and hasn't started to fall by 7 dB.

**Proposed:** replace the earlier figure with a peak and a fall:
H2 = −62 dBc + 20·log10(min(1, GR / 2.5)) − 0.5·max(0, GR − 5)² (dBc, GR in dB). That gives −68 at 1.2 dB, −62
from 2.5 to 5 dB, −70 at 9 dB, −83 at 11.5 dB: within about 2 dB of the slow-release runs, which show the FET's own
distortion without detector ripple (Lucky Comp's detector supplies the ripple).

## Finding 2: FET H3 while compressing (new)

With slow release (no ripple), the 676's H3 sits at about **−69 dBc** throughout compression (−70.6 at 1.9 dB, −68.5
at 4 dB, −70.6 at 11.5 dB). Lucky Comp at slow release has H3 only from its output-stage term and the detector:
−79 to −73 dBc, 5–10 dB too low.

**Proposed:** add a compressing H3 of −69 dBc, ramping in over the first 2 dB of reduction
(+ 20·log10(min(1, GR / 2))), summed in power with the existing output-stage H3.

At Moore's point, with both changes, Lucky Comp comes to about 0.16% THD (H3 is then dominated by the detector's ripple).

## Finding 3: detector ripple at fast release (not Colour; no change proposed yet)

With Colour at 0, Lucky Comp's detector alone already produces H3 and H5 at fast release (the manual's "low frequency
peaks"), as intended. But at the dial setting 7 it produces more than the 676:

| Attack 3, Release 7, Ratio 4, about 11 dB GR | H3 1 kHz | H5 1 kHz | H3 100 Hz | H5 100 Hz |
|---|---|---|---|---|
| 676 | −62.5 | −73 | −44 | −53 |
| Lucky, Colour 0 | −57.5 | −67 | −38 | −47 |

At 1–2 dB of reduction the gap at 100 Hz is about 16 dB (676 −62, Lucky −46). The likely cause is the release time:
the 676's fastest release measured **223 ms** to 63% (and 1.14 s to 90%), against the manual's 50 ms, which Lucky
Comp uses. The 50 ms is a published figure and one unit doesn't overrule it; the 676's timing test was also run at
the wrong attack setting and too little reduction. **Next session:** measure release at 7 with Attack 3 and more
reduction before deciding anything.

## Finding 4: the 676's output stage and transformers (design differences, no change)

- **Output stage, no compression:** H3 about −80 dBc at about +11 dBu out (absolute level unconfirmed), where Moore's
  1176 would be about −62. The 676 is much cleaner. Lucky Comp keeps Moore's figure.
- **Low frequencies, no compression:** at 100 Hz the 676 shows H3 of −58 dBc at −40 dBFS falling as level rises
  (transformer-like; the interface shows −89 dBc there), and H2 rising with level (−70 at −22 dBFS to −62 at −16).
  Moore saw no transformer distortion on the 1176 at 50 Hz. Not modelled.
- **Pre-knee at Ratio 20:** H2/H3 reach about −68 dBc at −10 dBFS in with only 0.04 dB of reduction. Lucky Comp's
  output-stage H3 happens to give −68 there; H2 is absent. Small; not modelled.

## Other differences noticed

- **Ratio:** the 676 at Ratio 4 measured about 5.7:1 above the knee (from a level-setting `curve` run whose csv
  wasn't kept; rerun next session). Lucky Comp uses 4:1.
- **Attack:** 676 Attack 7 took 0.83 ms to 63%, Attack 3 about 6 ms; the manual gives 20–800 µs. The `timing` test's
  resolution (one 6 kHz cycle, 0.17 ms) and level choice may limit this. Not acted on.

## Summary of proposed changes (`Source/dsp/Colour.*`; for the user to approve)

| Constant | From | To | Why |
|---|---|---|---|
| FET H2 while compressing | earlier figure, no published source | −62 dBc peak at 2.5–5 dB GR, ramp in below, falling 0.5·(GR − 5)² above | 676 slow-release runs; the earlier figure had no published source |
| FET H3 while compressing | none (output stage only) | −69 dBc, ramping in over 2 dB of GR | 676 slow-release runs |
| FET below-threshold H3 | −52 dBc at +16 dBu, 2 dB per dB | unchanged | published (Moore); the 676 differs by design |
| FET release at dial 7 | 50 ms | unchanged for now | published (manual); 676 measured 223 ms, to recheck |

`docs/colour-sources.md` cites `measurements/676/` for the compressing figures.

## Applied (2026-11-13)

Findings 1 and 2 are in `Source/dsp/Colour.*` as proposed. Lucky Comp re-measured on the new build, same settings
(Mode FET, Level match off, Mix 100%, Colour 1× = `@0.5`, Threshold −16 dB / Ratio 4 or −8 dB / Ratio 20, Attack
234 µs or 20 µs, Release 1.10 s or 50 ms; every printed `Name -> value` line checked), in
`measurements/676/after-fet-change/`:

- 1 kHz, slow release, Ratio 4 against the 676's slowest release: H2 within 1.4 dB from 2 to 11 dB of reduction
  (6 dB low at 1 dB, where the ramp-in is gentler than the 676's); H3 within 2 dB up to 10 dB, 2.9 dB high at 11 dB.
- 100 Hz, same: Lucky Comp's H2 is 4–20 dB below the 676 and H3 2–4 dB below (13 dB at 1 dB of reduction): the
  676's low-frequency distortion (Finding 4), not modelled.
- Moore's point (1 kHz, 6.6 dB of reduction, 20:1, fastest): THD 0.161% (Colour 0: 0.134%;
  676: 0.166%; Moore's Blackface: 0.16%).
