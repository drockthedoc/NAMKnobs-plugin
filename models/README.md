# NAMKnobs pedal models

Continuously-adjustable NAM-compatible pedal models. Each is a single `.nam` whose knob(s) are **real input
channels** to the neural net — so the model responds to knob positions in real time, without capturing the pedal
separately at every setting. Load any of these into the NAMKnobs plugin (or stock NAM) via the model browser.

These are our own trained models (WaveNet, except the compressor which is an LSTM). They are *based on* the named
circuits and are not affiliated with or endorsed by the original manufacturers.

## Included

| Pedal | File | Knobs (in order) |
|---|---|---|
| RAT | `NAMKnobs_RAT.nam` | Distortion, Filter |
| Tube Screamer | `NAMKnobs_TubeScreamer.nam` | Drive, Tone |
| Big Muff (Green Russian) | `NAMKnobs_BigMuff_GreenRussian.nam` | Sustain, Tone |
| MXR Distortion+ | `NAMKnobs_MXR_DistortionPlus.nam` | Distortion |
| Boss DS-1 | `NAMKnobs_BossDS1.nam` | Dist, Tone |
| Fuzz Face | `NAMKnobs_FuzzFace.nam` | Fuzz |
| Compressor (beta) | `NAMKnobs_Compressor.nam` | Threshold, Ratio, Attack, Release |

## How the knobs map today

In this build the model's knobs are driven by the plugin's existing tone controls, in order:

- **Bass knob → knob 1**
- **Middle knob → knob 2**
- **Treble knob → knob 3**

So for the RAT: **Bass = Distortion**, **Middle = Filter**. For a one-knob pedal (MXR, Fuzz Face) only **Bass**
is live. Each knob's range 0–10 maps to the model's trained 0–1 span; motion is smoothed (~5 ms) so turns and
automation are click-free. When one of these models is loaded, the plugin's analog tone stack is bypassed
automatically (the knobs feed the model, not an EQ).

> **Compressor is beta**: it has four knobs, but this build only exposes three tone controls, so **Release** is
> currently fixed at its midpoint. Dedicated, correctly-labelled per-model knobs (showing *Distortion/Filter*,
> *Threshold/Ratio/Attack/Release*, etc. instead of *Bass/Middle/Treble*) are the next feature.

## Verification

Every model here passes the CI parametric-control-contract check (`ci/verify_parametric`): it loads under the
pinned NAM core, reports the expected channel count, produces finite output, and each knob measurably changes the
sound. See `ci/README.md`.
