# Noose

A saturation + distortion VST3 whose GUI is a noose that tightens as you turn up
Saturation and Distortion.

## Controls
- **Saturation**: smooth tanh-style soft saturation
- **Distortion**: harder, slightly asymmetric clipping
- **Tone**: low-pass after the distortion
- **Output**: output gain in dB
- **Mix**: dry/wet

The noose tension is the average of Saturation and Distortion. The rope also
twitches with the output level. Both stages run at 2x oversampling, and the dry
signal is delay-compensated so Mix doesn't phase-smear.

## Build (needs CMake 3.22+, git, and a C++17 compiler)

### Windows (Visual Studio 2022 with "Desktop development with C++")
```
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```
Output: `build\Noose_artefacts\Release\VST3\Noose.vst3`
Copy it to `C:\Program Files\Common Files\VST3\`

### macOS (Xcode)
```
cmake -B build -G Xcode
cmake --build build --config Release
```
Output: `build/Noose_artefacts/Release/VST3/Noose.vst3`
Copy it to `~/Library/Audio/Plug-Ins/VST3/`

The first configure downloads JUCE, so it takes a minute.

## FL Studio
Options > Manage plugins > Find installed plugins (make sure the VST3 folder is
in the scan list), then add Noose from the plugin database.

A Standalone app is built too (`.../Standalone/`), handy for quick testing.

## No compiler? Let GitHub build it for you
1. Make a free GitHub account and create a new empty repository.
2. Upload everything in this folder (including the hidden `.github` folder) with
   "Add file > Upload files".
3. Open the repo's **Actions** tab, pick **Build Noose**, and wait a few minutes.
4. Download `Noose-Windows-VST3` (or `Noose-macOS-VST3`) from the finished run,
   unzip it, and put `Noose.vst3` in your VST3 folder.
