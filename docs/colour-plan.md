# Colour: plan

The Colour knob adds each classic compressor's tone on top of its behaviour. Agreed so far:

- **Dial:** 0 = clean, 12 o'clock = what the hardware does (1×), full = 10× on a log scale. "2×" doubles the harmonic distortion (+6 dB per harmonic), 10× is +20 dB; the EQ's fall in dB scales up to 2× and stops there (more would be a treble cut, not colour). Real hardware is subtle, so the right half is exaggerated on purpose: turn it up until the tone is obvious, then back off.
- **Modes:** FET and Opto, voiced from published hardware specifications and measurements (`docs/colour-sources.md`). FET is checked against a Behringer 676 measured through an audio interface. VCA is voiced from the VCA chip's datasheet and SSL's published measurements (no hardware check yet). Clean has no colour: the knob is greyed out with a note.
- **Not a clone:** we copy the tone's ingredients (frequency response, harmonic make-up and how it grows with level and gain reduction), not the circuit.
- **Sources:** published manuals, papers and our own hardware only. Third-party plugin output is never used to voice anything.

## 1. Model

Per mode, a colour stage on the compressed (wet) signal, after the gain is applied:

1. **EQ:** a gentle high-pass and low-pass where the published response isn't flat (Opto).
2. **Harmonics:** an envelope-normalised waveshaper `x + e·(h2·u² + h3·u³)`, `u = x / e`, with a DC blocker, so each harmonic's level relative to the signal is set directly from the published figures, depending on signal level and gain reduction.
3. **Amount:** the Colour dial scales the harmonics and the EQ's dB deviation (0, 1×, 10×; the EQ's filters are redesigned for each amount, up to 2×).

Level match keeps working (colour changes level a little; prediction stays within tolerance).

## 2. Check

- Unit tests: H2/H3 match the published figures within 1 dB at the published levels, and the Opto EQ within 0.05 dB, with Colour at 1×; Colour 0 is bit-identical to no colour; Colour 2× is +6 dB per harmonic and 10× +20 dB; the EQ's fall scales in dB.
- Hardware: measure the 676 (section 4) and the same tests on Lucky Comp's FET, and report the differences. Adjust where we're clearly off.
- Listening: A/B against the hardware on real material.

## 3. UI

- The Colour knob comes alive in FET and Opto: 0 to 10×, labelled "Off" … "10×", with a tick and a detent at 1× (drags within 3% of the dial land exactly on it; double-click also returns there), captioned "Like the hardware" at exactly 1× ("Exaggerated" above it).
- In Clean it stays greyed out, captioned "Clean has no colour".

## 4. Hardware session (Behringer 676)

`tools/Measure.cpp` (`LuckyCompMeasure`) plays test signals through an audio interface and analyses what comes back (Goertzel, no FFT library), aligned with a click.

Cabling: Scarlett line out 3 → 676 input, 676 output → Scarlett input 3 (line, gain fully down), interface at 48 kHz.
Each step runs `LuckyCompMeasure --hardware "<Scarlett name>" 3 3 <test> measurements/676/<file>.csv`.

1. Loopback (cable out 3 → in 3, no 676): `response`, `harmonics1k` → the interface alone.
2. 676, no compression (Ratio 4, Input low, Output set for unity on a quiet tone): `response`, `harmonics1k`.
3. 676 compressing, fast (Attack 3, Release 7): `harmonics1k`, `harmonics100`, `curve`.
4. 676 compressing, slow release (Release 1): `harmonics1k`, `harmonics100`.
5. Timing at Attack 1/7 × Release 1/7: `timing`.

Then the same tests on Lucky Comp's FET at matching settings, and a short write-up of where they differ.
