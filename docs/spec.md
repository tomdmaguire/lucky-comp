# Lucky Compressor: v1 spec

A compressor that is good enough for real mixes and teaches you to hear compression while you use it.

The agreed look and behaviour come from the browser prototype in `prototype/index.html`. Where this spec and the prototype disagree, this spec wins.

## Positioning

- For producers who use compressors but don't fully trust their ears with them yet, and for anyone curious how the classic compressors differ.
- Teaches by ear first: the comparison is level-matched, so what changes is the shape of the sound, not its loudness.
- Shows what the settings do, on the audio passing through and on an example hit.
- Behaviour, not a circuit clone. The classic modes copy how each compressor reacts (timing, ratio, knee, program-dependent release), not its exact tone. It has to sound good; it doesn't have to null against the hardware.
- A sibling of Lucky Reverb and Lucky Reference: same visual family, few controls, done well.

## Signal flow

```
in ─┬─ detector (stereo linked peak) → gain computer → smoothing (mode) → gain g
    │
    ├─ out = in × (dry + wet·g) ──────────── × make-up ──┐
    │                                                     ├─ bypass crossfade (20 ms) → out
    └─ raw = in ──────────────────────────────────────────┘
```

- Zero latency. No lookahead.
- Stereo-linked: one gain for both channels, from the larger of the two channels' absolute values.
- Mono and stereo buses.

## Parameters (host-visible)

| ID | Name | Range | Default | Notes |
|---|---|---|---|---|
| `mode` | Mode | Clean, FET, Opto, VCA | Clean | |
| `threshold` | Threshold | −60 to 0 dB | −24 dB | Shown as Input on FET, Peak Reduction on Opto |
| `ratio` | Ratio | 1:1 to 20:1 | 4:1 | |
| `attack` | Attack | 0.01 to 100 ms | 10 ms | |
| `release` | Release | 5 to 5000 ms | 150 ms | |
| `autoRelease` | Auto release | off/on | off | VCA only |
| `bypass` | Compare with uncompressed | off/on | off | Hear the input without compression (ID kept from when it was Bypass) |
| `match` | Level match | off/on | on | |
| `mix` | Mix | 0 to 100% | 100% | Mix mode: crossfade, % compressed |
| `mixMode` | Mix mode | Mix, Blend | Mix | As Lucky Reverb's output tabs |
| `blend` | Blend | −∞ (−60) to +6 dB | −12 dB | Blend mode: dry stays at 0 dB, compressed added at this level |

All values are stored in real units (dB, ms), so automation means the same thing in every mode. Each mode then constrains them (next section). Mix and Blend are shared by every mode (not remembered per mode): parallel compression is how you use a compressor, not part of the classic, and keeping it fixed means switching modes changes only the compressor. It was left out at first, then added back: parallel compression is something newcomers learn and need to try.

### Mode memory

Each mode remembers its own threshold, ratio, attack, release, auto release and Colour (each classic has its own tone, so 3× on FET shouldn't carry over to Opto). Switching mode in the editor saves the current values for the old mode and loads the new mode's (its defaults on first visit). The memory is saved with the session. Automating `mode` from the host doesn't swap memory; the new mode just constrains the current values.

## Modes

Each mode constrains the controls and sets the detector behaviour. The engine always applies the constrained values, so automation can't push a mode outside its range.

| | Clean | FET (think 1176) | Opto (think LA-2A) | VCA (think SSL bus) |
|---|---|---|---|---|
| Amount control | Threshold −60 to 0 dB | Input 0 to +40 dB (= threshold 0 to −40) | Peak Reduction 0 to 100 (= threshold 0 to −40) | Threshold −40 to 0 dB |
| Ratio | 1 to 20, continuous | 4, 8, 12, 20 | fixed ≈3:1 | 2, 4, 10 |
| Attack | 0.01 to 100 ms | 20 µs to 0.8 ms, dial 1 to 7 (7 fastest) | fixed 10 ms | 0.1, 0.3, 1, 3, 10, 30 ms |
| Release | 5 to 5000 ms | 50 to 1100 ms, dial 1 to 7 (7 fastest) | follows the music | 0.1, 0.3, 0.6, 1.2 s, Auto |
| Knee | 6 dB | 2 dB | 14 dB | 3 dB |
| Defaults | 0 dB, 4:1, 10 ms, 150 ms | Input +0, 4:1, attack 3, release 5 | Peak Reduction 0 | 0 dB, 4:1, 10 ms, 300 ms |

Every mode starts with the threshold at 0 dB, so a new instance changes nothing until you pull the threshold down and hear it start working, as on the hardware (an 1176 with the input at minimum, an LA-2A with Peak Reduction at 0). Double-clicking the knob returns there too.

- The FET dials are logarithmic: `attack ms = 0.8 × (0.02/0.8)^((dial−1)/6)`, `release ms = 1100 × (50/1100)^((dial−1)/6)`.
- Stepped values snap to the nearest step on a log scale.
- Opto release and VCA Auto use two-stage release (below).
- Trademarks: mode buttons use the technology name only. "Think 1176" etc. appears in tips and docs, never as product branding.

## Detector

- Level: `20·log10(max(|L|, |R|))` per sample.
- Gain computer with a soft knee W:
  - below `T − W/2`: no reduction
  - above `T + W/2`: reduction `(x − T)(1 − 1/R)`
  - in between: `(1 − 1/R)(x − T + W/2)² / (2W)`
- Smoothing in the gain-reduction domain (dB), one-pole, branching: attack time constant while reduction rises, release while it falls. Time constants are the time to reach 63% of a step.
- Two-stage release (Opto, VCA Auto): the reduction is the average of a fast follower and a slow follower. Both attack at the attack time. The fast one releases in 60 ms (Opto) or 100 ms (Auto), so about half the reduction lets go quickly. The slow one releases in a time that grows with recent compression: a memory follows the average reduction over 3 s (Opto) or 1.5 s (Auto), and slow release = `min(5 s, 0.4 + 0.3 × memory dB)` for Opto, `min(1.5 s, 0.2 + 0.1 × memory dB)` for Auto. The current slow release time is published for the UI.

## Level match

On by default. Turns the compressed signal up so it is as loud as the input, so comparing with uncompressed compares shape, not level.

- The audio thread records the last 3 s of input: K-weighted energy and detector level per sample.
- A background thread predicts the make-up by replaying that history through a fresh detector with the current settings and Mix (twice, measuring the second pass, so Opto's memory is warm): `sqrt(input energy / (input energy × (dry + wet·g)²))`, where Mix gives dry = 1 − m, wet = m and Blend gives dry = 1, wet = the Blend level. So comparing stays fair at any setting. It also tracks the loudest sample before and after compression (over every pass, the first starting from a detector at rest, so the first hit after a silence counts) and holds the final trim down so no peak goes above −1 dBFS (or the input's own peak, if higher); when it does, the uncompressed signal heard while comparing is turned down by the same amount, so the comparison stays fair. On a drum loop peaking at −1 dBFS that can be 5 dB at default settings; at −12 dBFS it never happens. The peaks come from the 3 s heard at the last settings change, so a much louder passage later can still go over. Blend can make the output louder than the input, so matching may also turn down. It re-predicts whenever the settings change (so a change, even mid-drag, is matched within a few tens of ms), and every 0.25 s of new sound until the history has 3 s of sound in it. The make-up ramps to the new value over 80 ms.
- With Mix or Blend, the compressed signal is level matched on its own first (predicted with dry 0, wet 1) and that gain is applied before it is combined with the dry signal; the result then gets a final trim. So both sides arrive about equally loud, the way a parallel compressor's own output gain is set, and Mix sounds even across its range (on a drum bus at −44 dB, 50% sits about two thirds of the way to fully compressed in crest factor, where before the first 60% sounded dry). Blend's level is then relative to the dry signal's loudness. With Level match off, nothing is made up and you hear the raw levels. The meters, the live view and the Lab still draw the compressed signal before any make-up, so they keep showing how much it was turned down.
- The matched gain is remembered: saved with the session (with the settings it was matched for) and kept when the host restarts audio, so playback starts at the right level instead of dipping for the first fraction of a second. A remembered gain holds while the history refills and is replaced once it has 3 s of sound, or straight away if a setting changes. A new instance has nothing to remember and starts at 0 dB until 0.1 s of sound arrives.
- Between changes the make-up holds. A make-up that kept following the music would undo the compressor's own levelling.
- Accuracy: within 0.5 dB of a real render (Opto slightly less exact, as its memory reaches further back than the history).
- Silence (below −70 dBFS) doesn't count; with no sound yet there is no make-up.
- Clamped to −30 to +30 dB.
- Off: no make-up gain. The output is quieter by however much is compressed; use your DAW's gain.
- Offline renders predict on the audio thread, so bounces match too.

## Compare with uncompressed

- Hear the input without compression. With Level match off, or when the match isn't held down by peaks, the output is bit-identical to the input once the 20 ms crossfade has finished; otherwise it is turned down to the held match (see Level match). A real bypass is the host's own.
- C toggles it while the editor has focus; it also lives in the ••• menu. Space is never used: it belongs to the host's transport.
- No Delta (hearing only what is removed): tried and dropped, as it didn't help anyone hear compression.

## Editor

900 × 600 at 100%, scalable like Lucky Reference (Window size menu: 100, 125, 150, 175%; remembered).

```
┌──────────────────────────────────────────────────────────────────────────┐
│ ▣ LUCKY COMPRESSOR     │   ● CLEAN    ● FET    ● OPTO    ● VCA        │  •••   │ top bar
├──────────────────────────────────────────────────────────────────────────┤
│ tip: what this mode is and what it's good for (one line)    [C] COMPARE...│ mode info (optional)
├───────────────────────────────────────────────┬──────────────────────────┤
│ YOUR SOUND  ◯ LEVEL MATCH  −3.6 dB [👁 GR]   │ LAB ⓘ  [▶ TEST SNARE S]  │
│  before (faint) / after (solid) waveform,     │  test snare before/after  │
│  threshold lines, gain-reduction curve        │  on a time grid,         │
│                                               │  GR curve                │
├───────────────────────────────────────────────┴──────────────────────────┤
│ THRESHOLD    RATIO     ATTACK     RELEASE     COLOUR     MIX | BLEND     │ strip
│   (knob)    (knob)     (knob)      (knob)     (later)      (knob)        │
└──────────────────────────────────────────────────────────────────────────┘
```

- **Colours:** each mode has a colour from the Lucky palette (Clean gold `#d6b785`, FET pink `#d685be`, Opto green `#85d68f`, VCA blue `#85aad6`). Tabs carry a dot in their colour; the selected mode tints the knobs, waveforms and switches.
- **Knobs:** Lucky Reverb's knob style. Drag vertically, Shift for fine, double-click to reset, wheel. Value in the middle, a caption underneath (e.g. "Dial 3.0" under FET attack, "= threshold −18 dB" under Input). While Attack or Release is held, the caption adds one word on what the value does: attack Flattens (under 0.5 ms), Tight (under 5 ms), Punchy (under 25 ms) or Gentle; release Fast (under 80 ms), Natural (under 400 ms) or Glued (e.g. "Dial 3.0 · Flattens"). Only while held: shown all the time, the words read as labels for something else.
  - Attack (10 µs to 100 ms), Release (5 ms to 5 s) and Ratio (1:1 to 20:1) keep one log scale in every mode, labelled at the ends of the ring. Each mode lights up only the stretch it can reach (FET attack: the first ~40%), so modes can be compared on the same dial. Switched controls (VCA, FET ratio) show their positions as dots and click between them; VCA Auto sits one position past 1.2 s.
  - Fixed controls (Opto ratio and attack) draw dimmed with a dotted ring, value shown, not draggable.
  - Opto release is display-only and moves on its own to show the current slow release time, captioned "Follows the music".
  - VCA Auto: the release knob reads "Auto", captioned with the current release time.
  - Colour: Off, then 1× to 20× in FET, Opto and VCA (the first 8% of the dial is Off, then log from 1× to 20×; below 1× is too faint to hear, so there is nothing between Off and 1×), "Off" to "20×" with a tick and a detent at 1× (drags within 3% of the dial land exactly on it; double-click also returns there), captioned "Like the hardware" at exactly 1× ("Exaggerated" above it); greyed out in Clean, with no caption. Live in VCA too.
  - Clean's Attack and Release tooltips list the classic ranges and suggest switching modes to see them on the dial (coloured marks were tried and dropped: the colour code needed explaining).
- **Live view ("YOUR SOUND"):** carries the Level match switch in its header. the last 4 seconds as smooth curved envelopes, mirrored peaks in dB (−54 to 0), input faint, output before make-up (compressed, blended by Mix) with a gradient fill and crisp edge. Dashed threshold lines. Gain reduction as a glowing white curve from the top (0 to 24 dB over half the height). The gain-reduction number is hidden until the "👁 GR" button in the corner is clicked. Turning it on first asks, in a callout from the button: "Show the gain-reduction number?", recommending to keep it hidden and guess by ear first, with "Show GR" and "Keep it hidden" (the default: Return, Escape or clicking away keep it hidden) and a "Don't ask again" box (remembered for every instance; ••• → "Ask before showing GR" turns the question back on). Turning it off never asks. Once shown, the number it then shows as e.g. "−3.6 dB" to the button's left (so the pointer never covers it) as the deepest reduction of the last 0.3 s. The choice is remembered (shared by every instance). Tooltip: try guessing by ear first. The curve explains; a number always on show invites compressing by eye. Both views label the upper dashed line "THRESHOLD" on a small chip drawn above everything. History is binned by meter index so it doesn't shimmer, slides by sub-pixel amounts and repaints on every display refresh.
- **Lab ("LAB · TEST SNARE"):** a test snare (a real, uncompressed snare with a small room added, 0.95 s, peaking at −6 dBFS, resampled to the host rate; see `Resources/README.md`; chosen over a kick because the crack and room make attack and release audible on any speakers) run through the current settings and Mix, drawn as its real waveform (each cycle, on the dB scale) in the same visual language, on a grid of dotted vertical time lines with a ruler so it reads as a test bench rather than your audio: input faint, output solid, threshold, gain-reduction curve (smoothed to lose ripple from single cycles). No attack marker: the gain-reduction line already shows the attack. Window 600 ms in every mode, so the hit keeps its size; a slow release shows as a line still climbing at the right edge. The hit is preceded by identical hits every second (a backbeat at 120 bpm, longer than the snare), so Opto's memory and release are in a realistic state. Recomputed on the message thread when a setting changes.
- **Tips:** one short line directly under the mode bar, on by default, turned off from the ••• menu (a setting shared by every instance): what the mode is and what it's good for. The mode tabs have no tooltips: the line already says it. Kept short on purpose: people don't read long copy, and anything about a single setting belongs at its dial.
- **Colour in the Lab:** the waveform can't show tone (FET's harmonics at 10× still move the drawn level by under 0.3 dB), so turning the Colour knob shows a panel in the Lab's top right for 2 s (fading over 0.4 s): "HARMONICS ADDED", bars and values for the 2nd ("warm") and 3rd ("edgy") harmonics in dB below the hit (−90 to −10 dB), at the snare's loudest moment and deepest reduction, from `Colour::harmonicsAt` (tested against what the stage adds), with "Overtones at 2× and 3× each note's pitch, in dB below the hit." FET, Opto and VCA.
- **Test snare:** "▶ TEST SNARE [S]" in the Lab's corner, or S, plays the test snare exactly as the Lab shows it: rendered with the current settings at the host rate, level-matched to the raw snare when Level match is on, and capped at −1 dBFS. It has its own path added to the output, so it never touches your audio's compression, meters or level match. C plays the raw snare instead, like everything else. An ⓘ next to LAB explains the Lab in its tooltip. Not MIDI: Live doesn't route MIDI to audio effects without workarounds.
- **••• menu:** version, Window size, Compare with uncompressed (C), Show mode info, Show Lab (hidden, the live view takes the full width; remembered), Ask before showing GR.
- **Compare with uncompressed:** not called Bypass, because with level match it isn't one: it plays your sound without compression at the same loudness. In the ••• menu with C as its shortcut, and a clickable [C] COMPARE WITH UNCOMPRESSED hint (`lucky::KeyHintButton`) at the right of the mode info line. While on, every accent (logo, mode dots and underline, waveforms, knobs, switches) turns grey and a `lucky::ToggleNotice` over the live view says "HEARING IT UNCOMPRESSED", "Your sound without compression, at the same loudness." (without "at the same loudness" when Level match is off), "Press [C] to switch back and forth", with a Back to compressed button for hosts that keep key presses. It never mentions being turned down: there is enough about levels already. C, the hint, the menu, the button, or picking any mode turns it off again. The (i) and the mode info stay short on purpose.
- **Strip:** six knobs: Threshold, Ratio, Attack, Release, Colour, and the output knob, whose label is replaced by MIX | BLEND tabs (`lucky::LabelTabs`, as on Lucky Reverb). MIX: 0 to 100%, "Dry" to "Comp", a linear crossfade (dry and compressed are in phase, so no equal-power law), caption "Parallel" in between. BLEND: the dry signal stays at 0 dB and the knob adds the compressed one from −∞ to +6 dB (Lucky Reverb's range and skew), caption "Dry stays at 0 dB": New York style parallel compression. Drawn in white rather than the mode colour: Mix and Blend are shared by every mode, while the other knobs are remembered per mode.

## Colour

Voiced from published specifications and published measurements of hardware units; see `docs/colour-sources.md` for
every figure and its source. A stage on the compressed (wet) signal, before it is combined with the dry one:

- **FET:** the output stage's third harmonic (−52 dBc at +16 dBu, 2 dB per dB below; Moore). While compressing
  (our Behringer 676): H2 ramps in to −62 dBc by 2.5 dB of reduction, holds to 5 dB, then falls 0.5·(GR − 5)² dB, and
  an H3 of −69 dBc ramps in over 2 dB. No EQ (the published response is ±1 dB).
- **Opto:** tube H2 (−54 dBc at 0 dBu, +0.8 dB per dB) and a little H3 with no reduction; while compressing the
  cell's H3 dominates, in proportion to reduction (−36 dBc at 6 dB). The response falls 1 dB at 30 Hz and 15 kHz.

- **VCA:** the VCA chip's own distortion (THAT 2180B datasheet, as a 4000 G's untrimmed-style dbx 202): H2 −80 dBc
  at 0 dBV in, +0.4 dB per dB of level and +0.15 dB per dB of reduction, H3 24 dB below. No EQ. Inaudible at 1×, as
  on the hardware; the detector already makes the low-frequency distortion SSL measures under fast settings.

Levels are read with 0 dBFS = +18 dBu. The amount scales the harmonics (2× ≈ +6 dB, 10× = +20 dB, 20× ≈ +26 dB, capped at −14 dBc) and the EQ's fall in dB (up to 2×; the filters
are redesigned per amount). 1× is captioned
"Like the hardware". To be compared with a Behringer 676 (FET) measured through an audio interface.

## Performance

`LuckyCompressorBench` (48 kHz, stereo, Colour 1×, Mix 60%, level match on, Apple Silicon): 0.1% (Clean) to 0.6% (FET) of
one core per instance at any buffer size; the slowest block under 13% of its deadline at 32 samples. A level-match
prediction (one replay of the 3 s history for both make-up gains) takes about 4.7 ms on the background thread, which
re-predicts every 15 ms while a setting changes and otherwise idles. The Lab's simulation takes about 7 ms on the
message thread per settings change; rendering the test snare about 3 ms. pluginval passes at strictness 10 and auval
passes with no warnings.

## Later (not v1)

- Feedback detection for Opto and FET, as in the LA-2A and 1176 circuits (the detector hears the output, so the
  first milliseconds of a hit slip through and the body is held more evenly). Every mode is feedforward now; on a
  drum bus Opto can sound a little beefier with less click than the original. Base it on the published circuits,
  not on matching a plugin's output.
- Serial compression (two stages, e.g. FET → Opto); a browser mock-up is in `prototype/chain.html`. Parked: two
  instances do the job for now.
- Check VCA against Thompson & Heilemann's SSL bus compressor dataset (real hardware, CC BY 4.0) once it is released.
- More Lab samples to choose from besides the snare (e.g. a bass note, a vocal phrase, a kick), CC0 or our own.
- Tip copy pass.
- 676 follow-ups: release at dial 7 (measured 223 ms against the manual's 50 ms), a Ratio 4 curve, attack times.
