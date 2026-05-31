# Super Bass - Product Blueprint

Product name: **Super Bass**

## 1. Product Idea

Super Bass is a premium three-module low-end finishing plugin for producers and mix engineers. It is designed to make 808s, bass guitar, synth bass, kick-bass layers, drum busses, and mix low-end feel bigger, clearer, wider where appropriate, and more finished with a simple interface.

The central macro control is called **Smile**. Smile is the emotional control: turn it up and the sound should feel more open, deeper, wider, more expensive, and more finished without requiring technical decisions.

The plugin is not a clone of any reference product. The references define taste and module roles:

- Waves RBass / Slate Digital Infinity Bass: perceived bass, low-end enhancement, harmonic translation.
- MBMixHead: analog-style density, weight, and smooth front-end tone.
- Spectre: frequency-aware harmonic enhancement instead of plain EQ boost.
- soothe2: dynamic control and smoothing, especially to avoid ugly low-mid buildup.
- Shadow Hills mastering compressors: expensive density and controlled weight.
- Manley Massive Passive: broad, musical EQ curves.
- Valhalla / TrueVerb style references: depth and space, but filtered and bass-safe.
- StandardCLIP / Gold Clip: final clipping, density, loudness, and peak control.

## 2. Core Structure

Super Bass has three main modules plus one global macro:

1. **Open**
   - Creates low-end size and body.
   - Contains **Sub** and **Body**.
   - Each control has a frequency selector.

2. **Space**
   - Adds distance and width in a bass-safe way.
   - Contains **Depth** and **Wide**.
   - Depth has near/far behavior.
   - Wide has narrow/wide behavior.

3. **Master**
   - Final polish and control.
   - Contains **Clipper**, **Auto Gain**, and a high-quality 5-band EQ.
   - EQ is simple boost/cut only.

4. **Smile**
   - Global Amount control for the entire plugin.
   - Scales the intensity of Open, Space, Master, and EQ behavior while preserving module settings.

Each of the three modules has an on/off switch.

The three modules can be reordered. The default order should be:

```text
Open -> Space -> Master
```

Supported routing orders should eventually include:

```text
Open -> Space -> Master
Open -> Master -> Space
Space -> Open -> Master
Space -> Master -> Open
Master -> Open -> Space
Master -> Space -> Open
```

For the first prototype, use the default order internally, but design the parameter system so routing order can be added without rewriting the plugin.

## 3. Target Sound

The default sound should feel like:

- Low-end opens up without becoming boomy.
- Bass becomes more audible on small speakers through harmonics.
- Body gains density and expensive analog-style weight.
- Space feels controlled and musical rather than reverb-heavy.
- Width affects upper harmonics and body more than the true sub.
- Final stage controls peaks and adds modern density.
- Smile makes the whole effect more impressive without breaking gain staging.

Avoid:

- Sub smear.
- Reverb in the true low-end.
- Over-wide bass below the mono-safe region.
- Harsh clipping aliasing.
- Complicated mastering controls.
- A clone-like recreation of any reference plugin.

## 4. Module Details

### Open

Purpose: create the sense that the bass has opened up, become larger, and gained record-ready body.

Controls:

- **Open On/Off**
- **Sub**
- **Sub Frequency**
- **Body**
- **Body Frequency**

Sub:

- Adds sub weight and low harmonic perception.
- Should feel like RBass / Infinity Bass-inspired translation, not just EQ boost.
- Frequency selector chooses the focus region.

Suggested Sub Frequency options:

- 40 Hz
- 50 Hz
- 60 Hz
- 80 Hz
- 100 Hz

Body:

- Adds warmth, density, and analog-style weight.
- Inspired by MBMixHead, Spectre-style harmonic EQ, and broad passive EQ behavior.
- Should not feel like obvious distortion at moderate settings.

Suggested Body Frequency options:

- 120 Hz
- 180 Hz
- 250 Hz
- 350 Hz
- 500 Hz

### Space

Purpose: create controlled depth and width without destroying low-end focus.

Controls:

- **Space On/Off**
- **Depth**
- **Depth Position**
- **Wide**
- **Width Position**

Depth:

- Adds distance, ambience, or short spatial reflection.
- Must be filtered so true sub remains clean.

Suggested Depth Position options:

- Near
- Mid
- Far

Near:

- Shorter reflections.
- More direct.
- Best for 808 and kick-bass.

Mid:

- Balanced.
- Good default.

Far:

- More depth and tail.
- Better for synth bass, sound design, and atmospheric bass.

Wide:

- Controls stereo width above the mono-safe low region.
- Should preserve mono compatibility.

Suggested Width Position options:

- Narrow
- Natural
- Wide

Narrow:

- More mono-focused.
- Good for 808 and kick.

Natural:

- Slight upper-bass/low-mid width.
- Good default.

Wide:

- More stereo body and harmonic width.
- Should never widen true sub.

### Master

Purpose: final tone, loudness, and safety.

Controls:

- **Master On/Off**
- **Clipper**
- **Auto Gain**
- **5-Band EQ**

Clipper:

- StandardCLIP / Gold Clip-inspired final density and peak shaping.
- Should start smooth and become more obvious at high settings.
- Later versions should include oversampling.

Auto Gain:

- Keeps perceived loudness under control.
- First version can use static compensation.
- Later versions can use measured input/output loudness matching.

5-Band EQ:

- High-quality musical EQ.
- Each band only has boost/cut.
- No Q controls in the main UI.
- Intended as fast finishing tone, not surgical correction.

Suggested bands:

- Low: 60 Hz
- Low Mid: 160 Hz
- Mid: 500 Hz
- High Mid: 2 kHz
- High: 8 kHz

Suggested range:

- -6 dB to +6 dB per band.

## 5. Smile Macro

Smile is the global Amount control.

At 0%:

- The plugin should be nearly bypassed, even if module settings are high.

At 35-50%:

- The default premium finished sound.

At 75-100%:

- Stronger, more creative, more obvious effect.

Smile should scale:

- Open Sub blend.
- Open Body saturation and harmonic blend.
- Space wetness and width amount.
- Master clipper drive.
- EQ boost/cut intensity.
- Auto gain compensation strength.

Smile should not:

- Change selected frequencies.
- Turn modules on/off.
- Create unsafe output spikes.

## 6. UI Direction

The UI should be simple and premium.

Layout:

- Top center: **Super Bass** name and preset area.
- Center: large **Smile** knob.
- Three module panels: **Open**, **Space**, **Master**.
- Each module has its own power button.
- A small routing control lets users change module order.
- Metering on the right side or bottom.

Open panel:

- Sub knob plus frequency selector.
- Body knob plus frequency selector.

Space panel:

- Depth knob plus Near/Mid/Far selector.
- Wide knob plus Narrow/Natural/Wide selector.

Master panel:

- Clipper knob.
- Auto Gain toggle.
- Five compact EQ boost/cut controls.

Design taste:

- Premium studio hardware feel.
- Minimal text.
- Clear module boundaries.
- No overly complex analyzer as the main visual.
- Avoid a generic one-color dark blue or purple interface.

## 7. Preset Direction

MVP presets:

- 808 Super Bass
- Tight Trap Low
- R&B Velvet Body
- Pop Bass Smile
- Kick Bass Open
- Synth Bass Wide
- Mix Bus Low Finish
- Clean Translation
- Heavy Modern Clip
- Warm Sub Body

## 8. MVP Definition

The first milestone is a sound-validation prototype.

MVP must:

- Build as an audio plugin or standalone audio processor.
- Process stereo audio.
- Expose Smile and all three modules.
- Include Open, Space, and Master on/off switches.
- Include Sub and Body with frequency selectors.
- Include Depth and Wide with position selectors.
- Include Clipper and Auto Gain.
- Include a simple 5-band EQ with boost/cut only.
- Use the default routing order internally.
- Avoid crashes, denormals, and extreme output spikes.

MVP can skip:

- Fancy UI.
- Installer.
- Authorization.
- Perfect oversampling.
- True peak limiting.
- Full routing-order UI if the internal architecture is ready for it.

## 9. Development Plan

### Phase 1 - Specification

- Define the three-module structure.
- Define parameters and ranges.
- Define routing-order model.
- Define DSP behavior per module.

### Phase 2 - Prototype

- Create JUCE plugin project.
- Implement parameter tree.
- Implement Open module first.
- Implement Master module second.
- Implement Space module third.
- Add basic UI.
- Build AU locally if tooling is available.

### Phase 3 - Sound Validation

- Test on 808, bass guitar, synth bass, kick-bass, and full mix examples.
- Tune Smile scaling.
- Tune default frequencies.
- Tune EQ band ranges.
- Add preset maps.

### Phase 4 - Polish

- Oversampling.
- Better clipper.
- Better depth.
- Better dynamic low control.
- Routing-order UI.
- UI identity.
- Installer and compatibility checks.

## 10. Immediate Next Step

Update technical architecture to match the Open / Space / Master / Smile structure, then prepare the JUCE project skeleton.

