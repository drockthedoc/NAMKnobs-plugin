# Full bundle rename plan (NeuralAmpModeler → NAMKnobs on disk)

## What's already done (config.h, shipped)
The plugin's **identity** is already NAMKnobs: `PLUG_NAME "NAMKnobs"`, unique `PLUG_UNIQUE_ID 'NKb1'` /
`PLUG_MFR_ID 'NKnb'`. In a DAW it shows as **NAMKnobs** and registers as a distinct plugin from stock NAM.

## What's left (needs a local mac + Windows build to iterate — do NOT do this blind on CI)
The bundle **filename** is still `NeuralAmpModeler.vst3` / `.component`, driven by `BUNDLE_NAME` and the project
files. So installing to a shared plugin folder overwrites stock NAM's file. Fixing that = the on-disk rename below.

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
