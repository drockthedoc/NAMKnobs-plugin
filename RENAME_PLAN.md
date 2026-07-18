# Optional: source-level bundle rename (NeuralAmpModeler → NAMKnobs in the project files)

## Coexistence is already DONE (shipped, CI-green)
NAMKnobs already coexists with stock NAM on every axis that matters to a user:
- **Identity** (`config.h`): `PLUG_NAME "NAMKnobs"`, unique `PLUG_UNIQUE_ID 'NKb1'` / `PLUG_MFR_ID 'NKnb'`.
- **On-disk filename**: the CI **post-build rename** step ships the bundles as `NAMKnobs.vst3` / `.component` /
  `.app` (and `NAMKnobs.vst3` on Windows) — pluginval-validated on both OSes. See `.github/workflows/build-native.yml`.
- **macOS ObjC prefix**: `OBJC_PREFIX`/`SWELL_APP_PREFIX` = `vNAMKnobs`, so both can be loaded in one host.

So installing NAMKnobs does **not** overwrite or clash with a stock NAM install. **You do not need this plan** for
coexistence.

## What this plan is for (optional, cosmetic)
The *build machinery* still uses `BUNDLE_NAME "NeuralAmpModeler"` and project files named `NeuralAmpModeler-*`;
the CI post-build step renames the OUTPUT. If you'd rather the project itself produce `NAMKnobs.*` directly (no
post-build step), do the source-level rename below — best on a **local mac + Windows build** (do NOT iterate it
blind on CI; it touches 200+ project-file references). This is polish, not a functional requirement.

### Why it's risky to do blind
The token `NeuralAmpModeler` is overloaded. It is simultaneously:
- the plugin identity (rename target),
- the C++ class `PLUG_CLASS_NAME NeuralAmpModeler` and source files `NeuralAmpModeler.{h,cpp}` (KEEP),
- the source directory `NeuralAmpModeler/` that CI does `cd NeuralAmpModeler/scripts` into (KEEP),
- the core submodule path `../NeuralAmpModelerCore` (KEEP).

A blanket find/replace breaks the KEEP items. The rename must be surgical, and there are ~206 refs in the mac
`project.pbxproj` and ~42 per Windows `.vcxproj`. Validate each change with an actual build.

### Steps
1. **config.h**: set `BUNDLE_NAME "NAMKnobs"` (leave `PLUG_CLASS_NAME`, `BUNDLE_MFR`, `BUNDLE_DOMAIN`).
2. **mac** (`NeuralAmpModeler/`):
   - `config/NeuralAmpModeler-mac.xcconfig` → `config/NAMKnobs-mac.xcconfig`; inside set `BINARY_NAME = NAMKnobs`
     and change `OBJC_PREFIX` / `SWELL_APP_PREFIX` off `vNeuralAmpModeler` (needed so NAMKnobs and stock NAM can be
     loaded together without Objective-C class collisions).
   - `projects/NeuralAmpModeler-macOS.xcodeproj` → `projects/NAMKnobs-macOS.xcodeproj`; in `project.pbxproj` update
     the xcconfig `baseConfigurationReference` names and the **app** target's product/target name (the VST3/AU
     targets already use `PRODUCT_NAME = $(BINARY_NAME)`, so they follow `BINARY_NAME` automatically). Leave source
     file references (`NeuralAmpModeler.cpp/.h`) untouched.
   - `projects/NeuralAmpModeler-macOS.entitlements` → `projects/NAMKnobs-macOS.entitlements` (update pbxproj ref).
   - `makedist-mac.sh` reads `BUNDLE_NAME` for `PLUGIN_NAME` and then opens `projects/$PLUGIN_NAME-macOS.xcodeproj`
     + `config/$PLUGIN_NAME-mac.xcconfig`, so the file renames above are what it keys on.
3. **Windows** (`NeuralAmpModeler/`):
   - `config/NeuralAmpModeler-win.props` → `config/NAMKnobs-win.props` (set the binary/target name inside).
   - `projects/NeuralAmpModeler-vst3.vcxproj` (+ `.filters`/`.user`) and `-app.vcxproj` → `NAMKnobs-*`; update
     `<TargetName>`/`<ProjectName>`/output names inside; keep `ClCompile` references to `NeuralAmpModeler.cpp`.
   - `makedist-win.bat` / `makezip-win.py` references to the project/bundle name.
4. **CI** (`.github/workflows/build-native.yml`): today `PROJECT_NAME: NeuralAmpModeler` is used both for the
   **source dir** (`cd NeuralAmpModeler/scripts`) and the **bundle name** (`NeuralAmpModeler.vst3` for pluginval,
   artifact names). Split these: keep the source dir as `NeuralAmpModeler`, but point the pluginval `--validate`
   and artifact steps at `NAMKnobs.vst3` / `NAMKnobs.component`.
5. Bump `PLUG_VERSION_STR` and re-cut the release once green.

### Validation
Build locally on macOS (`makedist-mac.sh full`) and Windows (`makedist-win.bat full`) BEFORE pushing — each
project-file edit needs a real build to confirm. Then push and let CI re-validate. Finish by loading both NAMKnobs
and stock NAM in one DAW session to confirm they coexist (distinct files, distinct IDs, no ObjC collision).
