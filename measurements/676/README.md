# Behringer 676 measurements

Our own Behringer 676 (FET compressor section, 1176-style), measured with `LuckyCompressorMeasure --hardware` on
2026-11-13. Used to set Lucky Compressor's FET colour while compressing (see `docs/colour-sources.md`).

## Setup

- Interface: Focusrite Scarlett 18i20 (2nd gen), 48 kHz. Line out 3 → 676 compressor input (Split on, preamp out of
  the path); 676 output → Scarlett input 4 (TRS line, gain fully down, no pad, no Air). Focusrite Control: Line
  Outputs 3-4 fed by Playback 3-4, fader at 0 dB.
- Levels (retailer specs, not yet confirmed by Focusrite's sheet): line out 0 dBFS = +16 dBu, line in 0 dBFS =
  +22 dBu at minimum gain. The loopback run should show the interface losing 6 dB on its own.
- Round trip 1160-1167 samples. Meter on GR. No EQ in the path.
- Harmonic levels below about −110 dBFS absolute (the low steps of each sweep) are at the noise floor.
- Step onsets at the top of a sweep can reach 0 dBFS for a fraction of a millisecond before the 676's attack acts
  (`loudest sample recorded` in the tool's output); the analysis window is the last 0.5 s of each 1.5 s step, so
  the results aren't affected.

## Runs

Dial positions: Input, Output, Attack and Release on the 676's 1-10 / 1-7 dials. Ratio is the button.

| File | Input | Output | Attack | Release | Ratio | Notes |
|---|---|---|---|---|---|---|
| `676-in2-out10-a3-r7-r4-harmonics1k.csv` | 2 | 10 | 3 | 7 | 4 | Knee at about −10 dBFS in; up to 6 dB of reduction; uncompressed gain +2.0 dB |
| `676-in3-out8-a3-r7-r4-harmonics1k.csv` | 3 | 8 | 3 | 7 | 4 | Knee at about −16 dBFS in; up to 11 dB of reduction; uncompressed gain +3.35 dB; about 5.7:1 above the knee |
| `676-in3-out8-a3-r7-r4-harmonics100.csv` | 3 | 8 | 3 | 7 | 4 | As above, 100 Hz |
| `676-in3-out8-a1-r7-r4-harmonics1k.csv` | 3 | 8 | 1.3-1.4 (just above Auto) | 7 | 4 | Slow attack; harmonics within 0.1 dB of Attack 3 |
| `676-in3-out8-a1-r7-r4-harmonics100.csv` | 3 | 8 | 1.3-1.4 (just above Auto) | 7 | 4 | Slow attack, 100 Hz; within 0.1 dB of Attack 3 |
| `676-in3-out8-a1-r7-r4-timing.csv` | 3 | 8 | 1.3-1.4 (just above Auto) | 7 | 4 | Attack 50% 3.5 ms; release 63% 223 ms, 90% 1.14 s |
| `676-in3-out8-a3-r0-r4-harmonics1k.csv` | 3 | 8 | 3 | slowest (below 1) | 4 | Slow release; residual GR from earlier runs can show at the first steps |
| `676-in3-out8-a3-r0-r4-harmonics100.csv` | 3 | 8 | 3 | slowest (below 1) | 4 | Slow release, 100 Hz |
| `676-in3-out8-a3-r0-r4-timing.csv` | 3 | 8 | 3 | slowest (below 1) | 4 | Attack 50% 3.7 ms; release 63% 773 ms, 90% 2.12 s |
| `676-in3-out8-a7-r7-r20-harmonics1k.csv` | 3 | 8 | 7 | 7 | 20 | Uncompressed gain +3.22 dB; 7.0 dB of reduction at −1 dBFS in: THD 0.17% (Moore's 1176 Blackface: 0.16%) |
| `676-in3-out8-a7-r7-r20-harmonics100.csv` | 3 | 8 | 7 | 7 | 20 | 100 Hz |
| `676-in3-out8-a7-r7-r20-timing.csv` | 3 | 8 | 7 | 7 | 20 | Attack 50% 0.67 ms, 63% 0.83 ms; release not measurable (only 2 dB of reduction on the loud step) |

Gain reduction at each step is the uncompressed gain minus `gain_db`.

## Loopback (interface alone)

Cable from line out 4 → input 4 (input 4 is the one every 676 run used; gain fully down). Line out 4 is the same
circuit as line out 3.

| File | Notes |
|---|---|
| `loopback-out4-in4-response.csv` | Gain +8.0 dB; flat within −0.4 dB (20 Hz) to −0.06 dB (20 kHz) |
| `loopback-out4-in4-harmonics1k.csv` | H2/H3 −97 to −100 dBc up to −10 dBFS out; steps from −7 dBFS clip the input (+8 dB gain) |
| `loopback-out4-in4-harmonics100.csv` | H2/H3 −89 to −111 dBc up to −10 dBFS out; same clipping |

- The interface's distortion is 25 dB or more below every 676 harmonic used, so it is not subtracted.
- The interface's input full scale is 8 dB below its output full scale, so the retailer figures (+16 dBu out,
  +22 dBu in) are wrong somewhere. **The absolute dBu levels are unconfirmed**; the 676's own gain is the chain gain
  minus 8 dB (e.g. −4.65 dB at Input 3, Output 8). Compressing results are referenced to gain reduction and don't
  depend on it; the below-threshold comparison with Moore's dBu figures does.
- At 100 Hz with no compression, the 676 shows H3 about −58 dBc at −40 dBFS in, falling as the level rises; the
  interface shows −89 dBc there, so it is the 676 (most likely its transformers).
