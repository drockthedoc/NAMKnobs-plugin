# CI: parametric-control contract verification

`verify_parametric.cpp` is a standalone check (no iPlug2) that builds against the **same pinned
`NeuralAmpModelerCore` submodule** the plugin uses, loads a parametric `.nam`, and asserts the
`[audio ; K controls]` contract that `ResamplingNAM` relies on:

1. loads without throwing; `NumInputChannels() == 1 + K` with `K >= 1`; mono, finite output (no NaN/Inf);
2. if the model declares `metadata.controls`, its count equals `K` (labels match the channel count);
3. **every control is individually wired and effective**: sweeping control *k* alone (0.0 vs 1.0) while holding
   the others at 0.5 measurably changes the output (`relDiff >= 0.01`). Sweeping all controls together is not
   enough — a model could ignore one knob and still pass — so each control is swept independently.

This is the value-level assertion `pluginval` cannot make (pluginval never loads a `.nam`). The `verify-parametric`
job on `ubuntu-latest` (`.github/workflows/build-native.yml`) runs it against **every** bundled
`models/NAMKnobs_*.nam`, so the release's "every knob on every pedal is wired" claim is actually tested.

Build/run locally:

```bash
cmake -S ci -B ci/build -DCMAKE_BUILD_TYPE=Release
cmake --build ci/build -j
for m in models/NAMKnobs_*.nam; do ci/build/verify_parametric "$m"; done
```

Exit 0 = pass; any non-zero = fail.

## Multi-rate control survival (`verify_resample.cpp`)

`verify_resample.cpp` closes the non-48 kHz gate. It mirrors `ResamplingNAM`'s **exact** runtime path — a mono
`dsp::ResamplingContainer<NAM_SAMPLE,1,12>` at the model's rate, with the K control channels injected inside the
block callback — and runs each model at **44.1 / 48 / 88.2 / 96 kHz**. At each rate it sweeps control 0 (low vs
high) and requires a finite, measurable delta. If resampling dropped the control channels, low and high would be
identical (`relDiff ≈ 0`) and it fails. In practice `relDiff` is essentially identical across all four rates
(e.g. RAT 0.7734 at every rate), so controls survive resampling exactly. The `verify-parametric` CI job runs it
over every bundled model alongside the per-control contract check.

> Both tests exercise the model + resampling boundary the plugin relies on, without iPlug2. They do not replace a
> DAW listen — the visual knob layout still wants a human eye.
