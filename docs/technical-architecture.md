# Super Bass - Technical Architecture

## 1. Framework Choice

Recommended framework: **JUCE**

Initial target:

- macOS AU for Logic Pro testing.
- VST3 after the AU prototype works.
- Windows VST3 later.

The plugin should be written with a modular DSP layout so **Open**, **Space**, and **Master** can be reordered without rewriting the processing code.

## 2. Plugin Format

First prototype:

- Stereo in / stereo out.
- Audio Unit component.
- Optional standalone target for fast testing.

Later:

- VST3.
- Universal macOS build if needed.
- Windows VST3.

## 3. Main Parameter Table

All values should be automatable unless noted.

| Parameter | Range | Default | Notes |
| --- | --- | --- | --- |
| Smile | 0-100% | 40% | Global amount macro. |
| Open Enabled | bool | On | Open module power. |
| Open Sub | 0-100% | 35% | Sub enhancement amount. |
| Open Sub Freq | enum | 60 Hz | 40 / 50 / 60 / 80 / 100 Hz. |
| Open Body | 0-100% | 25% | Body weight and harmonics. |
| Open Body Freq | enum | 180 Hz | 120 / 180 / 250 / 350 / 500 Hz. |
| Space Enabled | bool | On | Space module power. |
| Space Depth | 0-100% | 12% | Bass-safe depth amount. |
| Space Depth Position | enum | Mid | Near / Mid / Far. |
| Space Wide | 0-100% | 15% | Width above mono-safe low region. |
| Space Width Position | enum | Natural | Narrow / Natural / Wide. |
| Master Enabled | bool | On | Master module power. |
| Master Clipper | 0-100% | 20% | Final clipper intensity. |
| Master Auto Gain | bool | On | Output compensation. |
| EQ Low | -6 to +6 dB | 0 dB | 60 Hz musical band. |
| EQ Low Mid | -6 to +6 dB | 0 dB | 160 Hz musical band. |
| EQ Mid | -6 to +6 dB | 0 dB | 500 Hz musical band. |
| EQ High Mid | -6 to +6 dB | 0 dB | 2 kHz musical band. |
| EQ High | -6 to +6 dB | 0 dB | 8 kHz musical band. |
| Routing Order | enum | Open-Space-Master | Six possible module orders. |
| Input | -18 to +18 dB | 0 dB | Utility. |
| Output | -18 to +18 dB | 0 dB | Utility. |
| Mix | 0-100% | 100% | Dry/wet, optional in first UI. |
| HQ | bool | Off | Later oversampling. |

## 4. Smile Scaling

Smile is a global intensity scalar. Internally:

```text
smile = Smile / 100
```

Use curved scaling so low values feel usable:

```text
softSmile = sqrt(smile)
strongSmile = smile * smile
```

Suggested usage:

- Open Sub blend: `Sub * softSmile`
- Open Body drive: `Body * softSmile`
- Space Depth wet: `Depth * smile`
- Space Wide amount: `Wide * smile`
- Master Clipper drive: `Clipper * softSmile`
- EQ boost/cut: `BandGain * softSmile`
- Auto Gain compensation: based on `softSmile`

Smile must not change enum selections, module power states, or routing order.

## 5. Routing Model

Represent each module as a processor with the same interface:

```text
prepare(sampleRate, blockSize, channels)
reset()
process(audioBlock, parameters)
```

Routing order enum:

1. Open -> Space -> Master
2. Open -> Master -> Space
3. Space -> Open -> Master
4. Space -> Master -> Open
5. Master -> Open -> Space
6. Master -> Space -> Open

First prototype can expose only the default order in the UI, but the processor should be written so the order enum can be honored later.

Important routing notes:

- Master before Space can create louder ambience and should be gain-managed.
- Space before Open can make generated harmonics react to ambience and may sound creative but less clean.
- Default order should remain Open -> Space -> Master.

## 6. DSP Chain Overview

Per audio block:

1. Smooth all continuous parameters.
2. Save dry signal for Mix.
3. Apply input gain.
4. Build a processing list from Routing Order.
5. Process enabled modules in order.
6. Apply module-specific safety clamps.
7. Apply output gain.
8. Blend dry/wet if Mix exists.
9. Update meters.

## 7. Open Module

Purpose: low-end opening, sub enhancement, and body density.

Inputs:

- Open Enabled.
- Open Sub.
- Open Sub Freq.
- Open Body.
- Open Body Freq.
- Smile.

### Sub Section

Frequency options:

- 40 Hz
- 50 Hz
- 60 Hz
- 80 Hz
- 100 Hz

Prototype implementation:

- Band-pass or low-shelf region around selected Sub frequency.
- Generate harmonics from selected low band using smooth saturation.
- Blend generated harmonics back above the selected sub frequency.
- Keep pure sub controlled to avoid simple loudness bloat.

Suggested generated harmonic high-pass:

```text
harmonicHighPass = max(90 Hz, SubFreq * 1.6)
```

Saturation curve:

```text
y = tanh(drive * x) / tanh(drive)
```

### Body Section

Frequency options:

- 120 Hz
- 180 Hz
- 250 Hz
- 350 Hz
- 500 Hz

Prototype implementation:

- Broad bell around selected Body frequency.
- Body saturation path focused around 120-500 Hz.
- Blend harmonic body signal with original.

Body should be smoother than an obvious distortion stage.

### Open Safety

- Clamp nonlinear stage input.
- Use denormal protection.
- Cap added gain at aggressive settings.
- Avoid widening inside Open.

## 8. Space Module

Purpose: depth and width that do not damage mono low-end.

Inputs:

- Space Enabled.
- Space Depth.
- Space Depth Position.
- Space Wide.
- Space Width Position.
- Smile.

### Depth Section

Depth positions:

- Near: short delay/reflection, low feedback, minimal tail.
- Mid: moderate short ambience.
- Far: longer filtered tail, still bass-safe.

Prototype implementation:

- High-pass send before depth.
- Use short filtered delay/diffusion approximation.
- Mix back at conservative level.

Suggested high-pass:

- Near: 220 Hz.
- Mid: 180 Hz.
- Far: 150 Hz.

Suggested delay/ambience:

- Near: 8-18 ms.
- Mid: 18-35 ms.
- Far: 35-70 ms.

### Wide Section

Width positions:

- Narrow: reduce side below 200 Hz and keep width conservative.
- Natural: remove side below 120 Hz, mild width above.
- Wide: remove side below 100 Hz, add more side above cutoff.

Prototype implementation:

- Convert left/right to mid/side.
- High-pass side signal around cutoff.
- Scale side signal based on Wide and Smile.
- Recombine.

Never widen true sub.

## 9. Master Module

Purpose: final EQ, clipper, and output gain behavior.

Inputs:

- Master Enabled.
- Master Clipper.
- Master Auto Gain.
- 5 EQ band gains.
- Smile.

### 5-Band EQ

Bands:

- Low: 60 Hz.
- Low Mid: 160 Hz.
- Mid: 500 Hz.
- High Mid: 2 kHz.
- High: 8 kHz.

Range:

- -6 dB to +6 dB per band.

Prototype implementation:

- Low band: shelf or broad bell at 60 Hz.
- Low Mid / Mid / High Mid: broad bell filters.
- High: shelf or broad bell at 8 kHz.
- Apply Smile scaling to boost/cut values.

No Q controls in the main UI.

### Clipper

Prototype implementation:

- Soft clip curve after EQ.
- Clipper parameter increases pre-clip gain and applies post compensation.

Starting curve:

```text
if abs(x) < threshold:
    y = x
else:
    y = sign(x) * (threshold + softness * tanh((abs(x) - threshold) / softness))
```

Later:

- Oversampling.
- True peak ceiling.
- Multiple clip characters.

### Auto Gain

MVP version:

- Static output compensation based on Smile, Sub, Body, and Clipper.
- Keep it stable and predictable.

Later:

- Short-term RMS/LUFS-style matching.
- Optional learned compensation curves per mode/preset.

## 10. Safety Requirements

The first prototype must:

- Avoid output spikes at extreme settings.
- Smooth all continuous parameters.
- Reset filters and delay lines cleanly on sample-rate changes.
- Support 44.1, 48, 88.2, and 96 kHz.
- Handle mono input if the host reports it.
- Preserve mono compatibility below the Space module side cutoff.
- Avoid denormals.

## 11. Build Checklist

Before writing DSP code:

- Confirm JUCE is installed or choose a local setup method.
- Create project folder.
- Create plugin target.
- Add all parameters.
- Verify blank plugin builds.

After first DSP pass:

- Build AU.
- Run plugin validation if available.
- Test bypass and parameter automation.
- Test Smile at 0%, 40%, 100%.
- Test each module on/off.
- Test 808, bass guitar, synth bass, kick-bass, and full mix examples.
- Test routing order after the routing enum is implemented.

## 12. Known Risks

- Reorderable modules increase gain-staging complexity.
- Space before Master may make ambience louder after clipping.
- Master before Open can produce extra harmonic build-up.
- Clipper without oversampling may alias at aggressive settings.
- Auto Gain can feel unstable if implemented as dynamic loudness matching too early.
- 5-band EQ can make the UI too busy if not visually compact.

## 13. Next Implementation Step

Set up the project skeleton:

1. Install or locate JUCE.
2. Install CMake if using JUCE CMake workflow.
3. Create `SuperBass` plugin project.
4. Add parameter definitions.
5. Build pass-through AU.
6. Implement Open module first.

