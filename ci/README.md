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

> Scope: this exercises the model-boundary control contract. It does **not** cover `ResamplingNAM`'s resampling
> behaviour at non-model host rates (44.1/88.2/96 kHz) — that lives inside iPlug2 and is a separate gate.
