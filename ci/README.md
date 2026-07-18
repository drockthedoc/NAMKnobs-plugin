# CI: parametric-control contract verification

`verify_parametric.cpp` is a standalone check (no iPlug2) that builds against the **same pinned
`NeuralAmpModelerCore` submodule** the plugin uses, loads a bundled parametric `.nam`, and asserts the
`[audio ; K controls]` contract that `ResamplingNAM` relies on:

1. loads without throwing; `NumInputChannels() == 1 + K` with `K >= 1`;
2. mono, finite output (no NaN/Inf);
3. holding every control low vs. high produces a **measurably different** output — i.e. the control channels
   actually reach and steer the model, and are not silently dropped.

This is the value-level assertion `pluginval` cannot make (pluginval never loads a `.nam`). It runs as its own
`verify-parametric` job on `ubuntu-latest` in `.github/workflows/build-native.yml`.

Fixture: `models/rat_parametric.nam` — our RAT hybrid (2 controls: distortion, filter).

Build/run locally:

```bash
cmake -S ci -B ci/build -DCMAKE_BUILD_TYPE=Release
cmake --build ci/build -j
ci/build/verify_parametric ci/models/rat_parametric.nam
```

Exit 0 = pass; any non-zero = fail.

> Note: this exercises the model-boundary contract. It does **not** cover `ResamplingNAM`'s resampling behaviour
> at non-model host rates (44.1/88.2/96 kHz) — that lives inside iPlug2 and is the next verification gate.
