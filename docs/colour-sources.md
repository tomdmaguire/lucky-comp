# Colour: sources

Lucky Comp's Colour is voiced from published specifications and published measurements of **hardware** units. No
figure here comes from a software plugin. Levels in dBu are converted with 0 dBFS = +18 dBu (EBU R68), so 0 dBu is
−18 dBFS.

## FET (1176-style)

- **UREI 1176LN manual** (late revision, differential input): response ±1 dB 20 Hz–20 kHz; "less than 0.5% THD,
  50 Hz to 15 kHz with limiting, at 1.1 seconds release setting; distortion of low frequency peaks increases with
  faster release time"; attack < 20 µs to 800 µs; release 50 ms to 1.1 s (63% recovery).
  https://www.thehistoryofrecording.com/Manuals/UREI/Urei%201176LN%2001.pdf
- **A. Moore, PhD thesis, University of Huddersfield, 2017** (measurements of physical units).
  https://eprints.hud.ac.uk/id/eprint/34118/
  - Rev D, no gain reduction, 1 kHz: THD 0.01% at 0 dBu, 0.06% at +9 dBu, 0.25% at +16 dBu; H3 −52 dBc at +16 dBu
    (Table 5-1). The output stage's distortion is odd-order and rises steeply with level.
  - About 7 dB of reduction, fastest attack and release, 20:1, 1 kHz: THD 0.16% (Blackface) to 0.37% (reissue) while
    compressing; harmonics around −58 dBc (Tables 6-2/6-3). Rock-vocal settings (attack 3, release 7): 0.07%.
  - THD and IMD rise steadily with gain reduction and ratio; release matters more than attack; slow release cuts
    H3/H5 by about 20 dB; no transformer distortion seen at 50 Hz; units vary noticeably.
- **Siliconix/Vishay AN105**, "FETs as Voltage-Controlled Resistors": feeding half the drain signal back to the gate
  cancels most of a FET's even-order distortion; what remains grows with the voltage across the FET, i.e. with gain
  reduction. https://www.vishay.com/docs/70598/70598.pdf
- **Our own Behringer 676** (hardware, 2026-11-13; `measurements/676/`, write-up in `docs/676-comparison.md`). It
  matches Moore's published point (0.17% THD at 7 dB of reduction, 20:1, fastest, 1 kHz), so its shape fills the gaps
  between published figures. Slow-release runs (no detector ripple), 1 kHz, Attack 3, Ratio 4: H2 rises as
  compression starts, peaks at about −62 dBc from 2.5 to 5 dB of reduction, then falls (−70 at 9 dB, about −80 at
  11 dB); H3 sits at about −69 dBc throughout compression. The fall fits AN105: the cancelling feedback works harder
  as the FET conducts more.

## Opto (LA-2A-style)

- **Teletronix LA-2A manual (3-66)**: "less than 0.35% total harmonics at +10 dBm, and less than .75% at +16 dBm";
  response ±0.1 dB 30 Hz–15 kHz; release "approximately 0.06 seconds for 50% release, 0.5 to 5 seconds for complete
  release depending upon amount of previous reduction".
  https://www.steampoweredradio.com/pdf/teletronix/Teletronix%20LA-2A%20Leveling%20Amplifier%20Circa%201966.pdf
- **A. Moore, thesis (above), Table 5-1**, one Teletronix unit, no reduction: THD 0.20% at 0 dBu, 0.50% at +9 dBu,
  0.90% at +16 dBu; H2/H3 −54/−85 dBc at 0 dBu rising to −41/−57 dBc at +16 dBu (tube stage, second harmonic first).
- **A. Moore, "Objective Analysis and Perceptual Evaluation of LA-2A Compressors and Vocal Recordings", JAES 74(1/2),
  2026**, six units (three vintage, three reissues), draft:
  https://artefacts-discovery.researcher.life/full_text/DA-2/d8/d83e5c396d1d3fada2b6a691f987893c/full_text/ef0df7bb11eeb03f281eea16a4b20579.pdf
  - At 6 dB of reduction, +4 dBu: THD 0.75–4.22%, **third harmonic dominant** at −28 to −42 dBc, "typically 10–20 dB
    higher than second"; H2 −47 to −76 dBc; roughly flat across 63 Hz–1 kHz.
  - Response with no reduction: no unit met ±0.1 dB; 0.5–1.3 dB variation over 30 Hz–15 kHz, with a fall of 2.2–3.5 dB
    below 30 Hz or about 5 dB above 14 kHz depending on the unit.
  - T4 settling after a step: 33–81 ms (attack), 0.45–1.67 s (release).
- **Yu & Fazekas, AES AIMLA 2025**, fitted to the SignalTrain LA-2A hardware dataset: about 4:1 in Compress.
  https://arxiv.org/abs/2509.10706

## VCA (SSL G-series bus compressor style)

- **SSL SL4000G Console Service Manual (1993)**, drawing T82026-71 (quad bus mix amps, VCA, compressor side chain):
  the audio VCA is a dbx 202X wired unbalanced, with a "DISTORTION NULL" trim; NE5534 op-amps around it.
  https://funkwerkes.com/web/wp-content/techdocs/MixedProAudio/SSL-4000G-Console-Service-Manual-v1_1.pdf
- **THAT 2180-Series datasheet** (Blackmer pre-trimmed VCAs, the same family as the dbx 202): THD "1 kHz, No
  External Trim, VIN = 0 dBV, 0 dB gain" typ. 0.005 / 0.010 / 0.030 % (A / B / C grades). Figure 13 (FFT, 0 dB gain,
  1 kHz, 0 dBV, typical 2180B): H2 −80.1 dBc, H3 −104 dBc, H4 −104 dBc. Figures 10 and 12 (THD+N against input
  level at 0 and −15 dB gain), read off the graphs: 2180B ≈ 0.01 % at 1 V rms rising to ≈ 0.03 % at 10 V rms, and
  only a little higher at −15 dB gain. https://www.thatcorp.com/datashts/THAT_2180-Series_Datasheet.pdf
- **G. K. Hebert, "An Improved Monolithic Voltage-Controlled Amplifier", AES preprint 4055 (1995)**: parasitic
  resistance errors cancel at unity gain and cause distortion at other gains; mismatches "primarily cause second
  harmonic distortion". https://thatcorp.com/datashts/AES4055_An_Improved_Monolithic_VCA.pdf
- **SSL The Bus+ user guide**: two-stage Auto release "100 ms short, 12 seconds long"; 4K mode switches the VCA
  "from balanced to unbalanced (matching how the Bus Compressor in a 4000-series console was implemented)". An
  Audio Precision plot (+4 dBu, threshold 0 dB, 10:1, attack 0.1 ms, release 0.1 s, LOW THD off) shows THD+N of
  ≈ 3.5 % at 20 Hz, ≈ 1.2 % at 100 Hz, ≈ 0.5 % at 200 Hz, ≈ 0.15 % at 1 kHz: "Most compressors by their nature start
  distorting low frequencies before high ones, particularly when set for fast release times."
  https://solidstatelogic.com/assets/uploads/downloads/The_Bus_User_Guide_1.1.pdf
- **SSL XLogic G Comp manual**: frequency response ±0.2 dB (−3 dB at 100 kHz); THD+N typically 0.005 % with the
  compressor out. https://www.solidstatelogic.com/assets/uploads/downloads/g-series-stereo-compressor/XLogic_G-Comp.pdf
- **Checked against Lucky Comp's own detector** (VCA mode, +4 dBu sine, 10:1, 0.1 ms, 0.1 s, about 10 dB of
  reduction): 3.4 % at 20 Hz, 0.8 % at 100 Hz, 0.4 % at 200 Hz, 0.08 % at 1 kHz, all third harmonic. The detector
  already makes SSL's low-frequency distortion, so the colour stage adds only the chip's static part.
- **Later:** B. R. Thompson and M. C. Heilemann, "SSL Bus Compressor Control Voltage Dataset", University of
  Rochester, 2026, CC BY 4.0, https://doi.org/10.60593/ur.d.31892026 (real G384 hardware; paper accepted to DAFx
  2026). Not released yet; its calibration signals could check the VCA against a real unit.

## What Lucky Comp uses (Colour at 1×)

| | Below threshold | While compressing | EQ |
|---|---|---|---|
| FET | Output-stage H3: −52 dBc at +16 dBu, falling 2 dB per dB | H2 −62 dBc + 20·log10(min(1, GR/2.5)) − 0.5·max(0, GR − 5)²; H3 −69 dBc + 20·log10(min(1, GR/2)), power-summed with the output stage's (676) | none (±1 dB spec) |
| Opto | Tube H2 −54 dBc at 0 dBu + 0.8 dB per dB; H3 −85 dBc at 0 dBu + 1.75 dB per dB | H3 up to −36 dBc at 6 dB of reduction, in proportion to reduction | −1 dB at 30 Hz and 15 kHz |
| VCA | Chip H2 −80 dBc at 0 dBV (+2.2 dBu) in, +0.4 dB per dB of level; H3 24 dB below H2 | H2 +0.15 dB per dB of reduction (level read at the VCA's input); the detector adds the fast-release distortion | none (flat) |

The detector itself adds the low-frequency distortion of fast release (the manual's "low frequency peaks"), so the
colour stage doesn't. Units vary a lot; these are typical values, and the Colour knob scales them.

## Provisional figures and assumptions

- **FET while compressing comes from our 676** (`measurements/676/`), anchored to Moore's published point. Checked
  after the change (`measurements/676/after-fet-change/`): at 1 kHz, slow release, Ratio 4, H2 within 1.4 dB of the
  676 from 2 to 11 dB of reduction and H3 within 2 dB to 10 dB (6 dB low on H2 at 1 dB, where the ramp-in is gentler);
  Moore's point gives 0.161% THD (Moore 0.16%, 676 0.166%). At 100 Hz the 676 has more H2 and H3 (its transformers;
  Moore saw none on a real 1176), so Lucky Comp sits 4–20 dB below it there.
- **Opto, between published points (assumptions):** while compressing, H3 grows in proportion to reduction from 0
  to 6 dB and holds at −36 dBc above 6 dB. No published figure covers either; we have no LA-2A-style hardware to
  check them.
- **What the 676 can check:** the choices between published points (how harmonics grow with reduction, which
  harmonic leads, how they change with frequency and release), not the published points themselves. One Behringer
  unit doesn't outweigh measured 1176s.
