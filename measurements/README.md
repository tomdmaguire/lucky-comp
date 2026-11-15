# Measurements

`tools/Measure.cpp` (`LuckyCompressorMeasure`) is a headless host that plays test signals through a VST3 at 48 kHz and
analyses the output with Goertzel filters: frequency response, harmonics against level, static curves and attack /
release timing. With `--hardware "<device>" <out> <in>` it plays through an audio interface instead, so hardware can
be measured in a loop (measure a loopback cable first, to know the interface's own part).

- `lucky/`: Lucky Compressor itself, before Colour.
- `676/`: our own Behringer 676 (hardware) and Lucky Compressor's FET at matching settings; see its README and
  `docs/676-comparison.md`.

Envelope dumps (`*.envelope.csv`) are not committed.
