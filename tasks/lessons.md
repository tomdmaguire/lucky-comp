# Lessons

Lessons for every Lucky plugin live in `../lucky-labs/docs/lessons.md`; read them first. Project-specific ones go here.

## Test level matching with the same loudness measure it uses
- **Mistake:** checked level match with plain energy; the matcher uses K-weighting, so a bass-heavy drum loop read 1.5 dB off even though matching worked.
- **Rule:** verify a loudness-matched result with the matcher's own weighting (or BS.1770), never with unweighted RMS.

## Check what the measuring host actually set
- **Mistake:** `Colour=1` was parsed as 0.01× and `Release=1.1 s` as 5 ms, so a whole set of Lucky Compressor runs measured the wrong settings and looked like real findings.
- **Rule:** always read the `Name -> value` lines `LuckyCompressorMeasure` prints before trusting a run; use `@<normalised>` or the exact value text from `params` (e.g. `1100 ms`, `@0.5` for Colour 1×).

## Confirm hardware knob positions with a measurement
- **Mistake:** a slow-release run was really a slow-attack run (the wrong knob was turned); identical results were the only clue.
- **Rule:** when a hardware setting change should alter the result and doesn't, check the setting (e.g. with `timing`) before interpreting it.
