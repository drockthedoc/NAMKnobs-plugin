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
3. Turn the knobs. In this build the pedal's knobs are driven by the tone controls in order —
   **Bass = knob 1, Middle = knob 2, Treble = knob 3** (e.g. for the RAT, Bass = Distortion, Middle = Filter).
   See `models/README.md` for each pedal's knob list. Knob motion is smoothed (~5 ms) so it's click-free, and the
   analog tone stack is bypassed automatically while a parametric model is loaded.
4. Stack more instances before the amp as you like.

## Stacking before NAM

NAMKnobs is a mono pedal stage, exactly like a real pedal in front of an amp. A typical chain:

```
Guitar → NAMKnobs (RAT) → NAMKnobs (Tube Screamer) → NAM (amp) → cab/IR
```

## Notes / current limits

- Models with more than three knobs (the **Compressor**, 4 knobs) can't yet reach every knob from the three tone
  controls — its **Release** is fixed at midpoint. Dedicated, correctly-labelled per-model knobs are the next
  feature.
- Works at any host sample rate (44.1/48/88.2/96 kHz): audio is resampled to the model rate and the knob controls
  are injected at model rate, so they're never dropped.

## What's verified

- Builds and passes `pluginval` (VST3 + AU) on macOS and Windows in CI.
- A CI job (`ci/verify_parametric`) asserts every knob on the bundled models measurably changes the sound under
  the exact NAM core the plugin ships with.
