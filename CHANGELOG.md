# Changelog

## v1.0.0

First release.

- Four modes on one set of dials: Clean, FET (think 1176), Opto (think LA-2A) and VCA (think SSL bus compressor), each following its classic's published attack and release ranges, ratios and release behaviour, with its own memory of settings.
- Level match on by default, never pushing peaks above −1 dBFS (if it can't match without doing so, the uncompressed side is turned down instead), predicted from the last 3 seconds of your audio so changes are matched almost at once. With Mix or Blend, the compressed side is matched before it meets the dry signal, so the Mix knob is even across its range. The match is saved with your session.
- Live view of your audio before and after, with the threshold and the gain reduction drawn as a line; the gain-reduction number stays hidden until you ask for it.
- A one-line tip for each mode: what it is and what it's good for.
- The Lab: a real snare through your settings, drawn cycle by cycle. S plays it; C compares it with uncompressed.
- Colour for FET, Opto and VCA, voiced from published measurements of real units and from our own Behringer 676, from Off straight to 1× and on to 20×. Turning it shows the harmonics it adds.
- Compare with uncompressed (C, the hint by the tips, or the ••• menu): your sound without compression at the same loudness; the interface turns grey while you listen.
- Mix (dry to compressed) or Blend (dry at 0 dB, compressed added underneath).
- VST3 and Audio Unit for Apple Silicon and Intel, macOS 13 or newer.
