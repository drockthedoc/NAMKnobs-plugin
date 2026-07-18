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
| Compressor | `NAMKnobs_Compressor.nam` | Threshold, Ratio, Attack, Release |

## How the knobs work

When you load one of these models the plugin shows **exactly the right knobs for that pedal**, labelled with the
real control names read from the model — the RAT shows *Distortion* and *Filter*, the MXR shows a single
*Distortion*, the Compressor shows *Threshold / Ratio / Attack / Release*. Unused knob slots and the plugin's
analog tone stack are hidden while a pedal is loaded (the knobs feed the model, not an EQ). Each knob's 0–10 range
maps to the model's trained 0–1 span, and motion is smoothed (~5 ms) so turns and automation are click-free.

Every knob on every model here is verified in CI to be individually wired and effective (see the verification note
below) — including all four of the Compressor's knobs.

## Verification

Every model here passes the CI parametric-control-contract check (`ci/verify_parametric`): it loads under the
pinned NAM core, reports the expected channel count, produces finite output, and each knob measurably changes the
sound. See `ci/README.md`.
