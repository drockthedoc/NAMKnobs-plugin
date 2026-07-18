# NAMKnobs — install & use

NAMKnobs is a real-time pedal plugin: a fork of the Neural Amp Modeler plugin that loads **parametric** `.nam`
models whose knobs are live inputs to the neural net. It's mono in / mono out, so it sits **in front of** your
NAM amp and **stacks** with other instances (pedal → pedal → amp).

## Download

Grab the release archive for your OS from the repo's **Releases** page. Each contains the plugin(s) plus the
`models/` folder of NAMKnobs pedal models.

- **macOS**: `NeuralAmpModeler.vst3` and `NeuralAmpModeler.component` (AU)
- **Windows**: `NeuralAmpModeler.vst3`

## Install

**macOS**
- VST3 → `~/Library/Audio/Plug-Ins/VST3/`
- AU (`.component`) → `~/Library/Audio/Plug-Ins/Components/`
- Rescan plugins in your DAW (for AU you may need to log out/in or clear the AU cache).

**Windows**
- VST3 → `C:\Program Files\Common Files\VST3\`
- Rescan plugins in your DAW.

The build is unsigned; on macOS you may need to right-click → Open once, or clear quarantine
(`xattr -dr com.apple.quarantine <plugin>`).

## Use

1. Insert NAMKnobs on a guitar track **before** your NAM amp instance.
2. Click the model browser and load one of the `models/NAMKnobs_*.nam` pedals.
3. Turn the knobs. The plugin shows **exactly the right knobs for that pedal**, labelled with the real control
   names (RAT → *Distortion, Filter*; MXR → *Distortion*; Compressor → *Threshold/Ratio/Attack/Release*). Unused
   knob slots and the analog tone stack are hidden while a pedal is loaded. Knob motion is smoothed (~5 ms) so
   it's click-free.
4. Stack more instances before the amp as you like.

## Stacking before NAM

NAMKnobs is a mono pedal stage, exactly like a real pedal in front of an amp. A typical chain:

```
Guitar → NAMKnobs (RAT) → NAMKnobs (Tube Screamer) → NAM (amp) → cab/IR
```

## Notes / current limits

- In a DAW this shows up as **NAMKnobs** with its own unique plugin ID, so hosts treat it as a distinct plugin
  from stock NAM. **However**, the bundle *filename* on disk is still `NeuralAmpModeler.vst3` / `.component`, so
  installing it to a shared plugin folder overwrites stock NAM's file. Until the on-disk rename lands
  (see `RENAME_PLAN.md`), use a dedicated plugin folder if you want to keep both.
- Up to 4 knobs per model are shown (our largest, the Compressor, has exactly 4). All four Compressor knobs work.
- Works at any host sample rate (44.1/48/88.2/96 kHz): audio is resampled to the model rate and the knob controls
  are injected at model rate, so they're never dropped.

## What's verified

- Builds and passes `pluginval` (VST3 + AU) on macOS and Windows in CI.
- A CI job (`ci/verify_parametric`) asserts every knob on the bundled models measurably changes the sound under
  the exact NAM core the plugin ships with.
