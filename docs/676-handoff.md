# Handoff: measuring the Behringer 676

> **Status (2026-11-13):** first session done on branch `measure-676`. Results: `measurements/676/` and
> `docs/676-comparison.md` (proposed FET colour changes, not yet applied). Still to measure: `timing` at Release 7 /
> Attack 3 with more reduction, a saved `curve` at Ratio 4, attack times, absolute dBu levels. Cabling used: line
> out 3 → 676 compressor input (Split) → Scarlett input 4; loopback was line out 4 → input 4. The interface's
> loopback gain is +8 dB, so the retailer level specs below are wrong somewhere.

A separate session measures a Behringer 676 (a hardware FET compressor in the 1176 style) and compares it with
Lucky Comp's FET mode. Read `.claude/project_overview.md`, `docs/colour-plan.md` (section 4 is the session plan),
`docs/colour-sources.md` and `../lucky-labs/docs/lessons.md` first.

## Why

Lucky Comp's Colour (FET and Opto) is voiced only from published hardware figures (`docs/colour-sources.md`).
The 676 is a real unit we own, so it is the first check of FET against hardware: harmonics against level and gain
reduction, the static curve, attack and release, frequency response. The goal is a short write-up of where Lucky
Comp's FET differs, and proposed changes; nothing in `Source/` changes without the user agreeing.

## Rules

- **Sources:** hardware we own and published figures only; never measurements of software plugins (see the lesson
  on reading licences in `../lucky-labs/docs/lessons.md`). Before committing new hardware data, check the unit's
  manual and its maker's terms for anything about measurement or publishing results.
- **Another session is working in this repo** (Colour, the Lab, the editor). Work in a separate worktree and
  branch so the two don't collide, and don't edit `Source/` there:
  `git -C ~/src/lucky-compressor worktree add ~/src/lucky-compressor-676 -b measure-676`
  Fixes to `tools/Measure.cpp` are fine on that branch.
- **Never run commands against remote services.** Commit locally, don't push. Conventional commits, ending with
  the Co-Authored-By line the session is given.
- **Levels:** start every hardware run with the 676's Output low and the interface input gain fully down, and
  raise slowly. Don't overload the interface (watch for flat tops: H3/H5 jumping at the top steps means clipping).

## Building the tool

In the worktree (`../lucky-labs` must sit beside it, which it does at `~/src/lucky-labs`):

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DFETCHCONTENT_SOURCE_DIR_JUCE=../lucky-seven/build/_deps/juce-src
nice -n 10 cmake --build build --target LuckyCompMeasure -j 4
```

The binary is under `build/LuckyCompMeasure_artefacts/`. Build at low priority if a DAW is running.

## The tool (`tools/Measure.cpp`)

```
LuckyCompMeasure --devices
LuckyCompMeasure --hardware "<device name>" <output channel> <input channel> <test> <out.csv>
LuckyCompMeasure "<plugin.vst3>" params
LuckyCompMeasure "<plugin.vst3>" [Name=value ...] <test> <out.csv>
```

- Tests: `response` (third octaves at −40 dBFS), `harmonics100` / `harmonics1k` (−40 to 0 dBFS in 3 dB steps:
  gain and H2–H5 in dBc), `curve` (1 kHz, −50 to 0 dBFS in 2 dB steps), `timing` (6 kHz carrier stepping
  −30 → −6 → −30 dBFS; 50/63/90% attack and release times, plus an envelope csv beside it).
- Hardware mode runs the interface at 48 kHz (it refuses another rate), plays out of one channel, records one
  channel, and aligns the round trip with a click before each run (it prints the delay and the click level).
- **The hardware mode has never been run on real hardware.** The loopback step exists to prove it works; expect to
  fix small things (channel mapping, alignment, timing of the click).

## Setup

- Interface: Focusrite Scarlett 18i20 (2nd gen). At the start of this work it did not show up in `--devices`:
  it must be connected, powered and set to 48 kHz (Focusrite Control / Audio MIDI Setup) before anything works.
- Cabling: Scarlett line out 3 → 676 line input; 676 output → Scarlett input 3 (line level, gain fully down, no
  pad or Air). Check the channel numbers in `--devices`.
- dBu: Lucky Comp maps 0 dBFS = +18 dBu (EBU R68). The Scarlett's real level at 0 dBFS (output and input) is
  different; look up the 18i20 2nd gen's maximum line output and input levels, state the assumption in the
  write-up, and convert the 676's results to dBu at its own input and output so they compare with the published
  figures.

## Steps (from `docs/colour-plan.md` section 4)

Save results in `measurements/676/` with names that say the settings, e.g. `676-a3r7-r4-harmonics1k.csv`, and
keep a `measurements/676/README.md` with every knob position, button, level and date.

1. **Loopback** (cable out 3 → in 3, no 676): `response`, `harmonics1k`. This is the interface alone; subtract
   it from everything below.
2. **676 without compression:** if the 676 can run with no ratio button in (as on a 1176), use that; otherwise
   Ratio 4 with Input low enough that the meter shows no reduction. Output set so a quiet tone comes back at the
   level it went in. `response`, `harmonics1k`.
3. **Compressing, fast** (Attack 3, Release 7, Ratio 4, about 7 dB of reduction on a 1 kHz tone at −10 dBFS):
   `harmonics1k`, `harmonics100`, `curve`.
4. **Compressing, slow release** (Release 1): `harmonics1k`, `harmonics100`.
5. **Timing:** Attack 1 and 7 × Release 1 and 7: `timing`.
6. **Lucky Comp at matching settings:** the installed `~/Library/Audio/Plug-Ins/VST3/Lucky Comp.vst3`, with
   `Mode=FET`, Level match off, Mix 100%, Colour at 1× and at 0, the same ratio, and Attack/Release set to the
   same dial numbers (FET's dials map 1–7 to 0.8 ms–20 µs attack and 1.1 s–50 ms release; `params` lists the
   names and value text). Same tests.

## Deliverables

- `measurements/676/` (csv files and the README), committed on `measure-676`.
- `docs/676-comparison.md`: per test, the 676 against Lucky Comp FET (Colour 0 and 1×) against the published
  figures in `docs/colour-sources.md`; where they differ, by how much, and a proposed change (which constant,
  from what to what, and why). Remember one Behringer unit is not an 1176: say where it may differ by design.
- A short summary for the user. Merging into `main` and any change to `Source/` are the user's call.
