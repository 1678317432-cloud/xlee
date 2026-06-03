#include "PluginEditor.h"

namespace
{
const juce::Colour background { 243, 247, 251 };
const juce::Colour panel { 253, 254, 255 };
const juce::Colour panelOutline { 187, 208, 226 };
const juce::Colour gold { 45, 132, 222 };
const juce::Colour text { 30, 43, 58 };
const juce::Colour muted { 104, 125, 145 };
const juce::Colour openAccent { 33, 123, 219 };
const juce::Colour spaceAccent { 58, 157, 205 };
const juce::Colour masterAccent { 78, 116, 232 };
const juce::Colour glass { 235, 243, 250 };
const juce::Colour shadow { 158, 178, 198 };

juce::Point<float> pointOnKnobArc (juce::Point<float> centre, float radius, float angle)
{
    return centre.getPointOnCircumference (radius, angle);
}

void drawMaterialGrain (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, float alpha)
{
    g.setColour (colour.withAlpha (alpha));
    const auto x0 = static_cast<int> (area.getX()) + 4;
    const auto y0 = static_cast<int> (area.getY()) + 5;
    const auto x1 = static_cast<int> (area.getRight()) - 4;
    const auto y1 = static_cast<int> (area.getBottom()) - 4;

    for (int y = y0; y < y1; y += 8)
    {
        const auto offset = ((y / 8) & 1) * 5;
        for (int x = x0 + offset; x < x1; x += 17)
            g.fillRect (static_cast<float> (x), static_cast<float> (y), 1.0f, 1.0f);
    }
}

void drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& name)
{
    g.setColour (muted);
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    g.drawFittedText (name, area.removeFromTop (18), juce::Justification::centred, 1);
}

void drawPanel (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour accent, float phase)
{
    const auto r = area.toFloat();
    auto localPhase = phase + accent.getFloatBlue() * 0.17f;
    localPhase -= std::floor (localPhase);
    const auto sweepX = r.getX() - r.getWidth() * 0.55f + localPhase * r.getWidth() * 1.85f;

    g.setColour (shadow.withAlpha (0.2f));
    g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 10.0f);
    g.setGradientFill (juce::ColourGradient (panel, r.getX(), r.getY(),
                                             juce::Colour (239, 246, 252), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 10.0f);
    drawMaterialGrain (g, r.reduced (4.0f), juce::Colour (82, 116, 148), 0.045f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentWhite, sweepX - 74.0f, r.getY(),
                                             accent.withAlpha (0.07f), sweepX + 34.0f, r.getCentreY(), false));
    g.fillRoundedRectangle (r.reduced (3.0f), 8.0f);
    g.setColour (panelOutline.withAlpha (0.72f));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    g.setColour (accent.withAlpha (0.68f));
    g.drawRoundedRectangle (r.reduced (1.6f), 8.5f, 1.4f);
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.18f), r.getX(), r.getY(),
                                             accent.withAlpha (0.025f), r.getX(), r.getY() + 60.0f, false));
    auto header = r;
    g.fillRoundedRectangle (header.removeFromTop (42.0f), 9.0f);
    auto highlight = r.reduced (5.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.5f), highlight.getX(), highlight.getY(),
                                             juce::Colours::transparentWhite, highlight.getX(), highlight.getY() + 76.0f, false));
    g.fillRoundedRectangle (highlight.removeFromTop (52.0f), 8.0f);
}

void drawSubGroup (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour colour)
{
    const auto r = area.toFloat();
    const auto lineY = r.getY() + 2.0f;
    g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.0f), r.getX() + 8.0f, lineY,
                                             colour.withAlpha (0.32f), r.getCentreX(), lineY, false));
    g.drawLine (r.getX() + 8.0f, lineY, r.getCentreX(), lineY, 1.0f);
    g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.32f), r.getCentreX(), lineY,
                                             colour.withAlpha (0.0f), r.getRight() - 8.0f, lineY, false));
    g.drawLine (r.getCentreX(), lineY, r.getRight() - 8.0f, lineY, 1.0f);
}
}

void SuperBassAudioProcessorEditor::TechLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                                       float sliderPos, float rotaryStartAngle,
                                                                       float rotaryEndAngle, juce::Slider& slider)
{
    const auto area = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                             static_cast<float> (width), static_cast<float> (height)).reduced (7.0f, 5.0f);
    const auto diameter = juce::jmin (area.getWidth(), area.getHeight() - 16.0f);
    const auto knob = juce::Rectangle<float> (diameter, diameter).withCentre (area.getCentre().translated (0.0f, -5.0f));
    const auto centre = knob.getCentre();
    const auto radius = knob.getWidth() * 0.5f;
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const auto base = juce::Colour (248, 251, 254);
    const auto rim = juce::Colour (178, 202, 224);
    const auto tickOuter = radius - 1.0f;
    const auto tickInner = radius - 7.0f;
    const auto defaultValue = slider.getDoubleClickReturnValue();
    const auto defaultPos = static_cast<float> (slider.valueToProportionOfLength (defaultValue));
    const auto defaultAngle = rotaryStartAngle + defaultPos * (rotaryEndAngle - rotaryStartAngle);

    g.setColour (shadow.withAlpha (0.18f));
    g.fillEllipse (knob.translated (0.0f, 2.5f));

    for (int t = 0; t <= 28; ++t)
    {
        const auto tickNorm = static_cast<float> (t) / 28.0f;
        const auto tickAngle = rotaryStartAngle + tickNorm * (rotaryEndAngle - rotaryStartAngle);
        const auto major = t == 0 || t == 14 || t == 28 || t % 7 == 0;
        const auto active = tickNorm <= sliderPos + 0.002f;
        const auto length = major ? 8.0f : 5.0f;
        const auto inner = pointOnKnobArc (centre, tickInner - length, tickAngle);
        const auto outer = pointOnKnobArc (centre, tickOuter, tickAngle);
        g.setColour ((active ? accent : juce::Colour (176, 198, 219)).withAlpha (active ? 0.82f : 0.42f));
        g.drawLine (inner.x, inner.y, outer.x, outer.y, major ? 1.55f : 1.0f);
    }

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - 10.0f, radius - 10.0f, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (203, 218, 232));
    g.strokePath (track, juce::PathStrokeType (3.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, radius - 10.0f, radius - 10.0f, 0.0f,
                            rotaryStartAngle, angle, true);
    g.setColour (accent);
    g.strokePath (valueArc, juce::PathStrokeType (3.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto defaultInner = pointOnKnobArc (centre, radius - 18.0f, defaultAngle);
    const auto defaultOuter = pointOnKnobArc (centre, radius + 1.5f, defaultAngle);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.drawLine (defaultInner.x, defaultInner.y, defaultOuter.x, defaultOuter.y, 3.0f);
    g.setColour (accent.withAlpha (0.65f));
    g.drawLine (defaultInner.x, defaultInner.y, defaultOuter.x, defaultOuter.y, 1.25f);

    g.setGradientFill (juce::ColourGradient (juce::Colours::white, knob.getX() + knob.getWidth() * 0.24f, knob.getY(),
                                             juce::Colour (220, 233, 246), knob.getRight(), knob.getBottom(), false));
    g.fillEllipse (knob.reduced (9.0f));
    {
        juce::Path grainClip;
        grainClip.addEllipse (knob.reduced (13.0f));
        g.saveState();
        g.reduceClipRegion (grainClip);
        drawMaterialGrain (g, knob.reduced (13.0f), juce::Colour (66, 103, 136), 0.035f);
        g.restoreState();
    }
    g.setColour (rim.withAlpha (0.9f));
    g.drawEllipse (knob.reduced (9.0f), 1.2f);
    g.setColour (juce::Colours::white.withAlpha (0.72f));
    g.drawEllipse (knob.reduced (13.0f), 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.34f));
    g.fillEllipse (knob.reduced (18.0f).withTrimmedBottom (knob.getHeight() * 0.38f));

    const auto pointerLength = radius - 16.0f;
    const auto pointerStart = pointOnKnobArc (centre, 8.0f, angle);
    const auto pointerEnd = pointOnKnobArc (centre, pointerLength, angle);
    const auto haloEnd = pointOnKnobArc (centre, radius - 3.0f, angle);
    g.setColour (accent.withAlpha (0.18f));
    g.drawLine (centre.x, centre.y, haloEnd.x, haloEnd.y, 7.0f);
    g.setColour (accent.darker (0.05f));
    g.drawLine (pointerStart.x, pointerStart.y, pointerEnd.x, pointerEnd.y, 2.6f);
    g.setColour (juce::Colours::white.withAlpha (0.95f));
    g.fillEllipse (centre.x - 3.2f, centre.y - 3.2f, 6.4f, 6.4f);
    g.setColour (base.darker (0.1f));
    g.drawEllipse (centre.x - 3.2f, centre.y - 3.2f, 6.4f, 6.4f, 0.8f);
}

void SuperBassAudioProcessorEditor::TechLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                                           const juce::Colour& backgroundColour,
                                                                           bool shouldDrawButtonAsHighlighted,
                                                                           bool shouldDrawButtonAsDown)
{
    auto r = button.getLocalBounds().toFloat().reduced (1.0f);
    const auto active = button.getToggleState() || shouldDrawButtonAsDown;
    const auto accent = button.findColour (juce::TextButton::buttonOnColourId);
    const auto fill = active ? accent : backgroundColour;

    g.setColour (shadow.withAlpha (active ? 0.08f : 0.14f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 7.0f);
    g.setGradientFill (juce::ColourGradient (fill.brighter (active ? 0.15f : 0.04f), r.getX(), r.getY(),
                                             fill.darker (active ? 0.04f : 0.02f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour ((active ? accent.darker (0.1f) : panelOutline).withAlpha (shouldDrawButtonAsHighlighted ? 0.95f : 0.68f));
    g.drawRoundedRectangle (r.reduced (0.4f), 7.0f, active ? 1.35f : 1.0f);
    if (active)
    {
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.fillRoundedRectangle (r.removeFromTop (r.getHeight() * 0.45f), 7.0f);
    }
}

void SuperBassAudioProcessorEditor::TechLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                                                       bool shouldDrawButtonAsHighlighted,
                                                                       bool shouldDrawButtonAsDown)
{
    auto r = button.getLocalBounds().toFloat().reduced (2.0f, 4.0f);
    const auto active = button.getToggleState();
    const auto accent = button.findColour (juce::ToggleButton::tickColourId);
    const auto toggle = r.removeFromLeft (44.0f).reduced (0.0f, 2.0f);

    g.setColour ((active ? accent : juce::Colour (219, 230, 240)).withAlpha (shouldDrawButtonAsHighlighted ? 1.0f : 0.9f));
    g.fillRoundedRectangle (toggle, toggle.getHeight() * 0.5f);
    g.setColour (panelOutline.withAlpha (0.78f));
    g.drawRoundedRectangle (toggle.reduced (0.5f), toggle.getHeight() * 0.5f, 1.0f);
    const auto dotSize = toggle.getHeight() - 7.0f;
    const auto dotX = active ? toggle.getRight() - dotSize - 3.5f : toggle.getX() + 3.5f;
    g.setColour (juce::Colours::white);
    g.fillEllipse (dotX, toggle.getY() + 3.5f, dotSize, dotSize);
    g.setColour (shadow.withAlpha (0.25f));
    g.drawEllipse (dotX, toggle.getY() + 3.5f, dotSize, dotSize, 0.8f);

    g.setColour (active || shouldDrawButtonAsDown ? text : muted);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawFittedText (button.getButtonText(), r.toNearestInt(), juce::Justification::centredLeft, 1);
}

void SuperBassAudioProcessorEditor::TechLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                                                   int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height)).reduced (0.5f);
    g.setColour (shadow.withAlpha (0.12f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 7.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, r.getX(), r.getY(),
                                             juce::Colour (235, 244, 252), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour ((isButtonDown ? gold : panelOutline).withAlpha (0.9f));
    g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.15f);

    juce::Path arrow;
    const auto cx = r.getRight() - 17.0f;
    const auto cy = r.getCentreY() + 1.0f;
    arrow.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 4.0f);
    g.setColour (gold);
    g.fillPath (arrow);
}

SuperBassAudioProcessorEditor::SuperBassAudioProcessorEditor (SuperBassAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), routeStrip (p), vuMeter (p)
{
    setLookAndFeel (&techLookAndFeel);
    setSize (1240, 740);

    title.setText ("Super Bass", juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centred);
    title.setColour (juce::Label::textColourId, text);
    title.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    addAndMakeVisible (title);

    setupKnob (smile, "Smile", "smile");
    setupKnob (outputGain, "Gain", "output");
    setupKnob (sub, "Sub", "openSub");
    setupKnob (subFreq, "Sub Hz", "openSubFreq");
    setupKnob (body, "Body", "openBody");
    setupKnob (bodyFreq, "Body Hz", "openBodyFreq");
    setupKnob (depth, "Depth", "spaceDepth");
    setupKnob (depthDistance, "Distance", "spaceDepthDistance");
    setupKnob (wide, "Wide", "spaceWide");
    setupKnob (wideFreq, "Wide Hz", "spaceWideFreq");
    setupKnob (wideRate, "Rate", "spaceWideRate");
    setupKnob (diffusorFreq, "Diff Hz", "spaceDiffusorHz");
    setupKnob (clipper, "Clipper", "masterClipper");
    setupKnob (comp, "Comp", "masterCompression");
    setupKnob (compThreshold, "Thresh", "masterCompThreshold");
    setupKnob (compAttack, "C Atk", "masterCompAttack");
    setupKnob (compRelease, "C Rel", "masterCompRelease");
    setupKnob (transientAttack, "T Atk", "masterTransientAttack");
    setupKnob (transientRelease, "T Rel", "masterTransientRelease");
    setupKnob (transientMix, "Trans Mix", "masterTransientMix");
    setupKnob (eqLow, "45", "eqLow");
    setupKnob (eqLowMid, "72", "eqLowMid");
    setupKnob (eqMid, "108", "eqMid");
    setupKnob (eqHighMid, "185", "eqHighMid");
    setupKnob (eqHigh, "520", "eqHigh");

    setupToggle (openPower, "Open", "openEnabled");
    setupToggle (spacePower, "Space", "spaceEnabled");
    setupToggle (masterPower, "Master", "masterEnabled");
    setupToggle (allPassPower, "Diffusor", "spaceAllPassEnabled");

    setupOpenModeButton (punchButton, "PUNCH", 0);
    setupOpenModeButton (bassHeadButton, "BASSHEAD", 1);
    setupOpenModeButton (boomButton, "BOOM", 2);
    openFreqLinkButton.setButtonText ("LINK");
    openFreqLinkButton.setClickingTogglesState (true);
    openFreqLinkButton.setColour (juce::TextButton::buttonColourId, juce::Colour (237, 245, 252));
    openFreqLinkButton.setColour (juce::TextButton::buttonOnColourId, openAccent);
    openFreqLinkButton.setColour (juce::TextButton::textColourOffId, text);
    openFreqLinkButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    openFreqLinkAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "openFreqLink", openFreqLinkButton);
    addAndMakeVisible (openFreqLinkButton);
    setupModeButton (haasButton, "HASS", 0);
    setupModeButton (flangerButton, "FLANGER", 1);
    setupModeButton (phaserButton, "PHASER", 2);
    setupMeterButton (meterInButton, "IN", 0);
    setupMeterButton (meterOutButton, "OUT", 1);
    setupMeterButton (meterGrButton, "GR", 2);
    setupOversamplingButton (oversampling1xButton, "1x", 0);
    setupOversamplingButton (oversampling2xButton, "2x", 1);
    setupOversamplingButton (oversampling4xButton, "4x", 2);
    setupOversamplingButton (oversampling8xButton, "8x", 3);
    oversamplingLabel.setText ("OVERSAMPLING", juce::dontSendNotification);
    oversamplingLabel.setJustificationType (juce::Justification::centred);
    oversamplingLabel.setColour (juce::Label::textColourId, muted);
    oversamplingLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    addAndMakeVisible (oversamplingLabel);
    setupPresetControls();
    addAndMakeVisible (vuMeter);
    addAndMakeVisible (routeStrip);
    subFreq.slider.onValueChange = [this] { syncLinkedOpenFrequency ("openSubFreq"); };
    bodyFreq.slider.onValueChange = [this] { syncLinkedOpenFrequency ("openBodyFreq"); };
    openFreqLinkButton.onClick = [this]
    {
        setOpenLinkEnabled (openFreqLinkButton.getToggleState());
    };
    refreshPresetList();
    updateOpenModeButtons();
    updateOpenLinkButton();
    updateWideModeButtons();
    updateOversamplingButtons();
    updateMeterButtons();
    startTimerHz (48);
}

SuperBassAudioProcessorEditor::~SuperBassAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void SuperBassAudioProcessorEditor::setupKnob (Knob& knob, const juce::String& labelText, const juce::String& paramId)
{
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                     juce::MathConstants<float>::pi * 2.75f,
                                     true);
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 20);
    knob.slider.setColour (juce::Slider::rotarySliderFillColourId, gold);
    knob.slider.setColour (juce::Slider::rotarySliderOutlineColourId, panelOutline);
    knob.slider.setColour (juce::Slider::thumbColourId, text);
    knob.slider.setColour (juce::Slider::textBoxTextColourId, text);
    knob.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    knob.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (knob.slider);

    knob.label.setText (labelText, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    knob.label.setColour (juce::Label::textColourId, muted);
    knob.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (knob.label);

    knob.attachment = std::make_unique<SliderAttachment> (processor.apvts, paramId, knob.slider);

    if (auto* param = processor.apvts.getParameter (paramId))
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            knob.slider.setDoubleClickReturnValue (true, ranged->convertFrom0to1 (ranged->getDefaultValue()));
}

void SuperBassAudioProcessorEditor::setupToggle (Toggle& toggle, const juce::String& label, const juce::String& paramId)
{
    toggle.button.setButtonText (label);
    toggle.button.setColour (juce::ToggleButton::textColourId, text);
    toggle.button.setColour (juce::ToggleButton::tickColourId, gold);
    addAndMakeVisible (toggle.button);
    toggle.attachment = std::make_unique<ButtonAttachment> (processor.apvts, paramId, toggle.button);
}

void SuperBassAudioProcessorEditor::setupChoice (Choice& choice, const juce::String& labelText, const juce::String& paramId)
{
    choice.label.setText (labelText, juce::dontSendNotification);
    choice.label.setJustificationType (juce::Justification::centred);
    choice.label.setColour (juce::Label::textColourId, muted);
    choice.label.setFont (juce::FontOptions (12.0f, juce::Font::plain));
    addAndMakeVisible (choice.label);

    choice.box.setColour (juce::ComboBox::backgroundColourId, glass);
    choice.box.setColour (juce::ComboBox::textColourId, text);
    choice.box.setColour (juce::ComboBox::outlineColourId, panelOutline);
    choice.box.setColour (juce::ComboBox::arrowColourId, gold);
    addAndMakeVisible (choice.box);

    choice.attachment = std::make_unique<ComboBoxAttachment> (processor.apvts, paramId, choice.box);
}

void SuperBassAudioProcessorEditor::setupModeButton (juce::TextButton& button, const juce::String& buttonText, int mode)
{
    button.setButtonText (buttonText);
    button.setClickingTogglesState (false);
    button.setColour (juce::TextButton::buttonColourId, juce::Colour (236, 245, 251));
    button.setColour (juce::TextButton::buttonOnColourId, spaceAccent);
    button.setColour (juce::TextButton::textColourOffId, text);
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    button.onClick = [this, mode] { setWideMode (mode); };
    addAndMakeVisible (button);
}

void SuperBassAudioProcessorEditor::setupOpenModeButton (juce::TextButton& button, const juce::String& buttonText, int mode)
{
    button.setButtonText (buttonText);
    button.setClickingTogglesState (false);
    button.setColour (juce::TextButton::buttonColourId, juce::Colour (237, 245, 252));
    button.setColour (juce::TextButton::buttonOnColourId, openAccent);
    button.setColour (juce::TextButton::textColourOffId, text);
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    button.onClick = [this, mode] { setOpenMode (mode); };
    addAndMakeVisible (button);
}

void SuperBassAudioProcessorEditor::setupOversamplingButton (juce::TextButton& button, const juce::String& buttonText, int mode)
{
    button.setButtonText (buttonText);
    button.setClickingTogglesState (false);
    button.setColour (juce::TextButton::buttonColourId, juce::Colour (239, 246, 252));
    button.setColour (juce::TextButton::buttonOnColourId, gold);
    button.setColour (juce::TextButton::textColourOffId, text);
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    button.onClick = [this, mode] { setOversamplingMode (mode); };
    addAndMakeVisible (button);
}

void SuperBassAudioProcessorEditor::setupMeterButton (juce::TextButton& button, const juce::String& buttonText, int mode)
{
    button.setButtonText (buttonText);
    button.setClickingTogglesState (false);
    button.setColour (juce::TextButton::buttonColourId, juce::Colour (239, 246, 252));
    button.setColour (juce::TextButton::buttonOnColourId, gold);
    button.setColour (juce::TextButton::textColourOffId, text);
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    button.onClick = [this, mode] { setMeterMode (mode); };
    addAndMakeVisible (button);
}

void SuperBassAudioProcessorEditor::setWideMode (int mode)
{
    if (auto* param = processor.apvts.getParameter ("spaceWideMode"))
    {
        const auto normalised = static_cast<float> (juce::jlimit (0, 2, mode)) / 2.0f;
        param->beginChangeGesture();
        param->setValueNotifyingHost (normalised);
        param->endChangeGesture();
    }

    updateWideModeButtons();
}

void SuperBassAudioProcessorEditor::setOpenMode (int mode)
{
    if (auto* param = processor.apvts.getParameter ("openMode"))
    {
        const auto normalised = static_cast<float> (juce::jlimit (0, 2, mode)) / 2.0f;
        param->beginChangeGesture();
        param->setValueNotifyingHost (normalised);
        param->endChangeGesture();
    }

    updateOpenModeButtons();
}

void SuperBassAudioProcessorEditor::updateOpenModeButtons()
{
    const auto mode = static_cast<int> (processor.apvts.getRawParameterValue ("openMode")->load());
    punchButton.setToggleState (mode == 0, juce::dontSendNotification);
    bassHeadButton.setToggleState (mode == 1, juce::dontSendNotification);
    boomButton.setToggleState (mode == 2, juce::dontSendNotification);
    punchButton.repaint();
    bassHeadButton.repaint();
    boomButton.repaint();
}

void SuperBassAudioProcessorEditor::setOpenLinkEnabled (bool shouldBeEnabled)
{
    if (shouldBeEnabled)
    {
        const auto linkedHz = juce::jlimit (60.0, 256.0, subFreq.slider.getValue());
        suppressOpenLinkSync = true;
        if (std::abs (subFreq.slider.getValue() - linkedHz) > 0.0001)
            subFreq.slider.setValue (linkedHz, juce::sendNotificationSync);
        if (std::abs (bodyFreq.slider.getValue() - linkedHz) > 0.0001)
            bodyFreq.slider.setValue (linkedHz, juce::sendNotificationSync);
        suppressOpenLinkSync = false;
    }

    updateOpenLinkButton();
}

void SuperBassAudioProcessorEditor::updateOpenLinkButton()
{
    const auto linked = processor.apvts.getRawParameterValue ("openFreqLink")->load() > 0.5f;
    openFreqLinkButton.setToggleState (linked, juce::dontSendNotification);
    if (linked)
        syncLinkedOpenFrequency ("openSubFreq");
    openFreqLinkButton.repaint();
}

void SuperBassAudioProcessorEditor::syncLinkedOpenFrequency (const juce::String& changedParamId)
{
    if (suppressOpenLinkSync || processor.apvts.getRawParameterValue ("openFreqLink")->load() <= 0.5f)
        return;

    suppressOpenLinkSync = true;
    const auto requestedHz = changedParamId == "openSubFreq" ? subFreq.slider.getValue()
                                                             : bodyFreq.slider.getValue();
    const auto linkedHz = juce::jlimit (60.0, 256.0, requestedHz);
    if (std::abs (subFreq.slider.getValue() - linkedHz) > 0.0001)
        subFreq.slider.setValue (linkedHz, juce::sendNotificationSync);
    if (std::abs (bodyFreq.slider.getValue() - linkedHz) > 0.0001)
        bodyFreq.slider.setValue (linkedHz, juce::sendNotificationSync);
    suppressOpenLinkSync = false;
}

void SuperBassAudioProcessorEditor::updateWideModeButtons()
{
    const auto mode = static_cast<int> (processor.apvts.getRawParameterValue ("spaceWideMode")->load());
    haasButton.setToggleState (mode == 0, juce::dontSendNotification);
    flangerButton.setToggleState (mode == 1, juce::dontSendNotification);
    phaserButton.setToggleState (mode == 2, juce::dontSendNotification);
    haasButton.repaint();
    flangerButton.repaint();
    phaserButton.repaint();
}

void SuperBassAudioProcessorEditor::setOversamplingMode (int mode)
{
    if (auto* param = processor.apvts.getParameter ("oversampling"))
    {
        const auto normalised = static_cast<float> (juce::jlimit (0, 3, mode)) / 3.0f;
        param->beginChangeGesture();
        param->setValueNotifyingHost (normalised);
        param->endChangeGesture();
    }

    updateOversamplingButtons();
}

void SuperBassAudioProcessorEditor::updateOversamplingButtons()
{
    const auto mode = static_cast<int> (processor.apvts.getRawParameterValue ("oversampling")->load());
    oversampling1xButton.setToggleState (mode == 0, juce::dontSendNotification);
    oversampling2xButton.setToggleState (mode == 1, juce::dontSendNotification);
    oversampling4xButton.setToggleState (mode == 2, juce::dontSendNotification);
    oversampling8xButton.setToggleState (mode == 3, juce::dontSendNotification);
    oversampling1xButton.repaint();
    oversampling2xButton.repaint();
    oversampling4xButton.repaint();
    oversampling8xButton.repaint();
}

void SuperBassAudioProcessorEditor::setMeterMode (int mode)
{
    meterMode = juce::jlimit (0, 2, mode);
    vuMeter.setMode (meterMode);
    updateMeterButtons();
}

void SuperBassAudioProcessorEditor::updateMeterButtons()
{
    meterInButton.setToggleState (meterMode == 0, juce::dontSendNotification);
    meterOutButton.setToggleState (meterMode == 1, juce::dontSendNotification);
    meterGrButton.setToggleState (meterMode == 2, juce::dontSendNotification);
    meterInButton.repaint();
    meterOutButton.repaint();
    meterGrButton.repaint();
}

void SuperBassAudioProcessorEditor::setupPresetControls()
{
    presetLabel.setText ("PRESET", juce::dontSendNotification);
    presetLabel.setJustificationType (juce::Justification::centredRight);
    presetLabel.setColour (juce::Label::textColourId, muted);
    presetLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (presetLabel);

    presetBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (27, 27, 28));
    presetBox.setColour (juce::ComboBox::textColourId, text);
    presetBox.setColour (juce::ComboBox::outlineColourId, panelOutline);
    presetBox.setColour (juce::ComboBox::arrowColourId, gold);
    presetBox.onChange = [this]
    {
        if (! suppressPresetChange)
            loadSelectedPreset();
    };
    addAndMakeVisible (presetBox);

    auto setupButton = [this] (juce::TextButton& button, const juce::String& label)
    {
        button.setButtonText (label);
        button.setColour (juce::TextButton::buttonColourId, juce::Colour (239, 246, 252));
        button.setColour (juce::TextButton::buttonOnColourId, gold);
        button.setColour (juce::TextButton::textColourOffId, text);
        button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        addAndMakeVisible (button);
    };

    setupButton (presetLoadButton, "LOAD");
    setupButton (presetSaveButton, "SAVE AS");
    setupButton (presetRenameButton, "FOLDER");
    setupButton (presetBrowseButton, "BROWSE");

    presetLoadButton.onClick = [this] { loadSelectedPreset(); };
    presetSaveButton.onClick = [this] { savePreset(); };
    presetRenameButton.onClick = [this] { renamePreset(); };
    presetBrowseButton.onClick = [this] { browsePresets(); };
}

juce::File SuperBassAudioProcessorEditor::getPresetDirectory() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Xlee Audio")
        .getChildFile ("Super Bass")
        .getChildFile ("Presets");
}

void SuperBassAudioProcessorEditor::refreshPresetList()
{
    const auto previousId = presetBox.getSelectedId();
    presetEntries.clear();
    presetBox.clear (juce::dontSendNotification);

    presetEntries.push_back ({ "Factory Presets", true, true, -1, {} });
    presetBox.addSectionHeading (presetEntries.back().name);
    const juce::StringArray factoryNames { "Default", "Punch Weight", "BassHead Depth", "Boom Thick" };
    for (int i = 0; i < factoryNames.size(); ++i)
    {
        presetEntries.push_back ({ factoryNames[i], true, false, i, {} });
        presetBox.addItem (presetEntries.back().name, static_cast<int> (presetEntries.size()));
    }

    auto presetDir = getPresetDirectory();
    presetDir.createDirectory();
    juce::Array<juce::File> files;
    presetDir.findChildFiles (files, juce::File::findFiles, false, "*.superbass");
    files.sort();

    presetEntries.push_back ({ "User Presets", false, true, -1, {} });
    presetBox.addSectionHeading (presetEntries.back().name);
    for (const auto& file : files)
    {
        presetEntries.push_back ({ file.getFileNameWithoutExtension(), false, false, -1, file });
        presetBox.addItem (presetEntries.back().name, static_cast<int> (presetEntries.size()));
    }

    suppressPresetChange = true;
    presetBox.setSelectedId (previousId > 0 && previousId <= static_cast<int> (presetEntries.size()) ? previousId : 2,
                             juce::dontSendNotification);
    suppressPresetChange = false;
}

void SuperBassAudioProcessorEditor::setPresetParameter (const juce::String& paramId, float value)
{
    if (auto* ranged = processor.apvts.getParameter (paramId))
    {
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (ranged))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            parameter->endChangeGesture();
        }
    }
}

void SuperBassAudioProcessorEditor::applyFactoryPreset (int factoryIndex)
{
    setPresetParameter ("smile", 100.0f);
    setPresetParameter ("output", 0.0f);
    setPresetParameter ("openEnabled", 1.0f);
    setPresetParameter ("spaceEnabled", 1.0f);
    setPresetParameter ("masterEnabled", 1.0f);
    setPresetParameter ("openFreqLink", 0.0f);
    setPresetParameter ("routingOrder", 0.0f);
    setPresetParameter ("oversampling", 0.0f);
    setPresetParameter ("eqLow", 0.0f);
    setPresetParameter ("eqLowMid", 0.0f);
    setPresetParameter ("eqMid", 0.0f);
    setPresetParameter ("eqHighMid", 0.0f);
    setPresetParameter ("eqHigh", 0.0f);

    if (factoryIndex == 1)
    {
        setPresetParameter ("openMode", 0.0f);
        setPresetParameter ("openSub", 82.0f);
        setPresetParameter ("openSubFreq", 58.0f);
        setPresetParameter ("openBody", 78.0f);
        setPresetParameter ("openBodyFreq", 135.0f);
        setPresetParameter ("spaceDepth", 34.0f);
        setPresetParameter ("spaceWide", 38.0f);
        setPresetParameter ("masterClipper", 18.0f);
        setPresetParameter ("masterCompression", 16.0f);
    }
    else if (factoryIndex == 2)
    {
        setPresetParameter ("openMode", 1.0f);
        setPresetParameter ("openSub", 100.0f);
        setPresetParameter ("openSubFreq", 48.0f);
        setPresetParameter ("openBody", 62.0f);
        setPresetParameter ("openBodyFreq", 96.0f);
        setPresetParameter ("spaceDepth", 56.0f);
        setPresetParameter ("spaceWide", 28.0f);
        setPresetParameter ("masterClipper", 10.0f);
        setPresetParameter ("masterCompression", 12.0f);
    }
    else if (factoryIndex == 3)
    {
        setPresetParameter ("openMode", 2.0f);
        setPresetParameter ("openSub", 72.0f);
        setPresetParameter ("openSubFreq", 64.0f);
        setPresetParameter ("openBody", 92.0f);
        setPresetParameter ("openBodyFreq", 180.0f);
        setPresetParameter ("spaceDepth", 42.0f);
        setPresetParameter ("spaceWide", 52.0f);
        setPresetParameter ("masterClipper", 22.0f);
        setPresetParameter ("masterCompression", 14.0f);
    }
    else
    {
        setPresetParameter ("openMode", 0.0f);
        setPresetParameter ("openSub", 100.0f);
        setPresetParameter ("openSubFreq", 61.1f);
        setPresetParameter ("openBody", 100.0f);
        setPresetParameter ("openBodyFreq", 93.2f);
        setPresetParameter ("spaceDepth", 100.0f);
        setPresetParameter ("spaceDepthDistance", 80.0f);
        setPresetParameter ("spaceWide", 100.0f);
        setPresetParameter ("spaceWideFreq", 300.0f);
        setPresetParameter ("spaceWideRate", 0.7f);
        setPresetParameter ("spaceWideMode", 1.0f);
        setPresetParameter ("spaceAllPassEnabled", 0.0f);
        setPresetParameter ("spaceDiffusorHz", 200.0f);
        setPresetParameter ("masterClipper", 0.0f);
        setPresetParameter ("masterCompression", 0.0f);
    }

    setPresetParameter ("masterCompThreshold", 0.0f);
    setPresetParameter ("masterCompAttack", 80.0f);
    setPresetParameter ("masterCompRelease", 30.0f);
    setPresetParameter ("masterTransientAttack", 0.0f);
    setPresetParameter ("masterTransientRelease", 0.0f);
    setPresetParameter ("masterTransientMix", 0.0f);
    updateOpenModeButtons();
    updateOpenLinkButton();
    updateWideModeButtons();
    updateOversamplingButtons();
}

void SuperBassAudioProcessorEditor::loadSelectedPreset()
{
    const auto index = presetBox.getSelectedId() - 1;
    if (! juce::isPositiveAndBelow (index, static_cast<int> (presetEntries.size())))
        return;

    const auto& entry = presetEntries[static_cast<size_t> (index)];
    if (entry.isHeader)
        return;

    if (entry.isFactory)
        applyFactoryPreset (entry.factoryIndex);
    else
        loadPresetFile (entry.file);
}

void SuperBassAudioProcessorEditor::writePresetFile (const juce::File& file)
{
    if (auto xml = processor.apvts.copyState().createXml())
    {
        file.getParentDirectory().createDirectory();
        xml->writeTo (file);
    }
}

void SuperBassAudioProcessorEditor::loadPresetFile (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
        if (xml->hasTagName (processor.apvts.state.getType()))
            processor.apvts.replaceState (juce::ValueTree::fromXml (*xml));

    updateOpenModeButtons();
    updateOpenLinkButton();
    updateWideModeButtons();
    updateOversamplingButtons();
}

void SuperBassAudioProcessorEditor::savePreset()
{
    const auto selectedIndex = presetBox.getSelectedId() - 1;
    juce::String name = "User Preset";
    if (juce::isPositiveAndBelow (selectedIndex, static_cast<int> (presetEntries.size())))
    {
        const auto& entry = presetEntries[static_cast<size_t> (selectedIndex)];
        if (! entry.isFactory && ! entry.isHeader)
            name = entry.file.getFileNameWithoutExtension();
    }

    auto file = getPresetDirectory().getChildFile (juce::File::createLegalFileName (name.trim()) + ".superbass");
    if (file.existsAsFile())
    {
        int suffix = 2;
        while (file.existsAsFile())
            file = getPresetDirectory().getChildFile (juce::File::createLegalFileName (name.trim()) + " " + juce::String (suffix++) + ".superbass");
    }

    writePresetFile (file);
    refreshPresetList();
    for (int i = 0; i < static_cast<int> (presetEntries.size()); ++i)
        if (! presetEntries[static_cast<size_t> (i)].isFactory && ! presetEntries[static_cast<size_t> (i)].isHeader && presetEntries[static_cast<size_t> (i)].file == file)
            presetBox.setSelectedId (i + 1, juce::dontSendNotification);
}

void SuperBassAudioProcessorEditor::renamePreset()
{
    browsePresets();
    refreshPresetList();
}

void SuperBassAudioProcessorEditor::browsePresets()
{
    getPresetDirectory().createDirectory();
    getPresetDirectory().revealToUser();
}

void SuperBassAudioProcessorEditor::timerCallback()
{
    visualMotionPhase += 0.012f;
    if (visualMotionPhase > 1.0f)
        visualMotionPhase -= 1.0f;

    updateOpenModeButtons();
    updateOpenLinkButton();
    updateWideModeButtons();
    updateOversamplingButtons();
    updateMeterButtons();
    vuMeter.repaint();
    routeStrip.repaint();
    repaint();
}

void SuperBassAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (252, 254, 255), full.getX(), full.getY(),
                                             background, full.getX(), full.getBottom(), false));
    g.fillAll();
    drawMaterialGrain (g, full.reduced (8.0f), juce::Colour (84, 116, 150), 0.032f);
    g.setColour (juce::Colour (211, 225, 238).withAlpha (0.16f));
    for (int y = 34; y < getHeight(); y += 34)
        g.drawHorizontalLine (y, 22.0f, static_cast<float> (getWidth() - 22));
    g.setColour (juce::Colours::white.withAlpha (0.32f));
    for (int x = 42; x < getWidth(); x += 64)
        g.drawVerticalLine (x, 18.0f, static_cast<float> (getHeight() - 18));
    g.setColour (juce::Colours::white.withAlpha (0.62f));
    g.fillRoundedRectangle (full.reduced (10.0f), 14.0f);
    g.setColour (panelOutline.withAlpha (0.44f));
    g.drawRoundedRectangle (full.reduced (10.5f), 14.0f, 1.0f);
    const auto sweepX = full.getX() - full.getWidth() * 0.35f + visualMotionPhase * full.getWidth() * 1.7f;
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentWhite, sweepX - 180.0f, full.getY(),
                                             juce::Colour (102, 174, 238).withAlpha (0.055f), sweepX, full.getCentreY(), true));
    g.fillRoundedRectangle (full.reduced (14.0f), 12.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.5f), full.getX() + 22.0f, full.getY() + 12.0f,
                                             juce::Colours::transparentWhite, full.getX() + 22.0f, full.getY() + 104.0f, false));
    g.fillRoundedRectangle (full.reduced (16.0f).withHeight (86.0f), 10.0f);

    auto bounds = getLocalBounds().reduced (18);
    bounds.removeFromTop (54);
    auto presetArea = bounds.removeFromTop (38).withTrimmedLeft (276).withTrimmedRight (12);
    g.setColour (shadow.withAlpha (0.13f));
    g.fillRoundedRectangle (presetArea.toFloat().translated (0.0f, 2.0f), 9.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, static_cast<float> (presetArea.getX()), static_cast<float> (presetArea.getY()),
                                             glass, static_cast<float> (presetArea.getX()), static_cast<float> (presetArea.getBottom()), false));
    g.fillRoundedRectangle (presetArea.toFloat(), 9.0f);
    g.setColour (panelOutline.withAlpha (0.78f));
    g.drawRoundedRectangle (presetArea.toFloat().reduced (0.5f), 9.0f, 1.0f);
    g.setColour (gold.withAlpha (0.2f));
    g.fillRoundedRectangle (presetArea.removeFromLeft (5).toFloat(), 5.0f);
    bounds.removeFromTop (36);

    auto smileArea = bounds.removeFromLeft (198);
    drawPanel (g, smileArea, gold, visualMotionPhase);

    auto modules = bounds.reduced (12, 0);
    auto open = modules.removeFromLeft (modules.getWidth() / 3).reduced (6);
    auto space = modules.removeFromLeft (modules.getWidth() / 2).reduced (6);
    auto master = modules.reduced (6);

    drawPanel (g, open, openAccent, visualMotionPhase);
    drawPanel (g, space, spaceAccent, visualMotionPhase + 0.08f);
    drawPanel (g, master, masterAccent, visualMotionPhase + 0.16f);

    auto spaceGroups = space.reduced (16);
    spaceGroups.removeFromTop (46);
    drawSubGroup (g, spaceGroups.removeFromTop (132).reduced (0, 4), spaceAccent.darker (0.7f));
    drawSubGroup (g, spaceGroups.removeFromTop (168).reduced (0, 4), spaceAccent);
    drawSubGroup (g, spaceGroups.removeFromTop (40).reduced (0, 4), spaceAccent.brighter (0.35f));

    drawSectionTitle (g, smileArea.reduced (12, 10), "GLOBAL");
    drawSectionTitle (g, open.reduced (12, 10), "OPEN");
    drawSectionTitle (g, space.reduced (12, 10), "SPACE");
    drawSectionTitle (g, master.reduced (12, 10), "MASTER");
}

void SuperBassAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (18);
    title.setBounds (bounds.removeFromTop (44));

    auto presetArea = bounds.removeFromTop (38).withTrimmedLeft (276).withTrimmedRight (12).reduced (10, 5);
    presetLabel.setBounds (presetArea.removeFromLeft (62));
    presetArea.removeFromLeft (8);
    presetBrowseButton.setBounds (presetArea.removeFromRight (74).reduced (3, 1));
    presetRenameButton.setBounds (presetArea.removeFromRight (72).reduced (3, 1));
    presetSaveButton.setBounds (presetArea.removeFromRight (76).reduced (3, 1));
    presetLoadButton.setBounds (presetArea.removeFromRight (62).reduced (3, 1));
    presetArea.removeFromRight (8);
    presetBox.setBounds (presetArea.reduced (2, 1));
    bounds.removeFromTop (36);

    auto content = bounds.removeFromTop (630);
    auto smileArea = content.removeFromLeft (208).reduced (16);
    smileArea.removeFromTop (22);
    vuMeter.setBounds (smileArea.removeFromTop (132));
    auto meterButtons = smileArea.removeFromTop (32).reduced (0, 3);
    const auto meterButtonWidth = meterButtons.getWidth() / 3;
    meterInButton.setBounds (meterButtons.removeFromLeft (meterButtonWidth).reduced (2));
    meterOutButton.setBounds (meterButtons.removeFromLeft (meterButtonWidth).reduced (2));
    meterGrButton.setBounds (meterButtons.reduced (2));
    smileArea.removeFromTop (10);
    layoutKnob (smile, smileArea.removeFromTop (152));
    auto osArea = smileArea.removeFromTop (54).reduced (0, 4);
    oversamplingLabel.setBounds (osArea.removeFromTop (18));
    const auto osButtonWidth = osArea.getWidth() / 4;
    oversampling1xButton.setBounds (osArea.removeFromLeft (osButtonWidth).reduced (2));
    oversampling2xButton.setBounds (osArea.removeFromLeft (osButtonWidth).reduced (2));
    oversampling4xButton.setBounds (osArea.removeFromLeft (osButtonWidth).reduced (2));
    oversampling8xButton.setBounds (osArea.reduced (2));
    smileArea.removeFromTop (2);
    layoutKnob (outputGain, smileArea.removeFromTop (126));

    routeStrip.setBounds (content.removeFromTop (70).reduced (12, 6));

    auto modules = content.reduced (12, 0);
    auto openArea = modules.removeFromLeft (modules.getWidth() / 3).reduced (16);
    auto spaceArea = modules.removeFromLeft (modules.getWidth() / 2).reduced (16);
    auto masterArea = modules.reduced (16);

    openArea.removeFromTop (18);
    openPower.button.setBounds (openArea.removeFromTop (28));
    auto openKnobs = openArea.removeFromTop (174);
    const auto openKnobWidth = openKnobs.getWidth() / 2;
    const auto subBounds = openKnobs.removeFromLeft (openKnobWidth).reduced (5);
    const auto subFreqBounds = openKnobs.reduced (5);
    layoutKnob (sub, subBounds);
    layoutKnob (subFreq, subFreqBounds);
    auto bodyKnobs = openArea.removeFromTop (174);
    const auto bodyKnobWidth = bodyKnobs.getWidth() / 2;
    const auto bodyBounds = bodyKnobs.removeFromLeft (bodyKnobWidth).reduced (5);
    const auto bodyFreqBounds = bodyKnobs.reduced (5);
    layoutKnob (body, bodyBounds);
    layoutKnob (bodyFreq, bodyFreqBounds);
    const auto linkX = subBounds.getRight() - 15;
    const auto linkY = subBounds.getBottom() + 8;
    const auto linkW = juce::jmax (54, subFreqBounds.getX() - subBounds.getRight() + 30);
    openFreqLinkButton.setBounds (linkX, linkY, linkW, 24);
    auto openButtons = openArea.removeFromTop (66).reduced (2, 8);
    const auto openButtonWidth = openButtons.getWidth() / 3;
    punchButton.setBounds (openButtons.removeFromLeft (openButtonWidth).reduced (3));
    bassHeadButton.setBounds (openButtons.removeFromLeft (openButtonWidth).reduced (3));
    boomButton.setBounds (openButtons.reduced (3));

    spaceArea.removeFromTop (18);
    spacePower.button.setBounds (spaceArea.removeFromTop (28));
    auto spaceKnobs = spaceArea.removeFromTop (132);
    layoutKnob (depth, spaceKnobs.removeFromLeft (spaceKnobs.getWidth() / 2).reduced (5));
    layoutKnob (wide, spaceKnobs.reduced (5));
    auto toneKnobs = spaceArea.removeFromTop (122);
    const int toneWidth = toneKnobs.getWidth() / 4;
    layoutKnob (depthDistance, toneKnobs.removeFromLeft (toneWidth).reduced (4));
    layoutKnob (wideFreq, toneKnobs.removeFromLeft (toneWidth).reduced (4));
    layoutKnob (wideRate, toneKnobs.removeFromLeft (toneWidth).reduced (4));
    layoutKnob (diffusorFreq, toneKnobs.reduced (4));
    auto diffusorRow = spaceArea.removeFromTop (36).reduced (8, 4);
    allPassPower.button.setBounds (diffusorRow);
    auto buttons = spaceArea.removeFromTop (46).reduced (2, 5);
    const auto buttonWidth = buttons.getWidth() / 3;
    haasButton.setBounds (buttons.removeFromLeft (buttonWidth).reduced (3));
    flangerButton.setBounds (buttons.removeFromLeft (buttonWidth).reduced (3));
    phaserButton.setBounds (buttons.reduced (3));

    masterArea.removeFromTop (18);
    masterPower.button.setBounds (masterArea.removeFromTop (28));
    auto masterTop = masterArea.removeFromTop (112);
    const int masterTopWidth = masterTop.getWidth() / 3;
    layoutKnob (clipper, masterTop.removeFromLeft (masterTopWidth).reduced (4));
    layoutKnob (comp, masterTop.removeFromLeft (masterTopWidth).reduced (4));
    layoutKnob (compThreshold, masterTop.reduced (4));

    auto compArea = masterArea.removeFromTop (112);
    layoutKnob (compAttack, compArea.removeFromLeft (compArea.getWidth() / 2).reduced (4));
    layoutKnob (compRelease, compArea.reduced (4));

    auto transArea = masterArea.removeFromTop (110);
    const int transWidth = transArea.getWidth() / 3;
    layoutKnob (transientAttack, transArea.removeFromLeft (transWidth).reduced (3));
    layoutKnob (transientRelease, transArea.removeFromLeft (transWidth).reduced (3));
    layoutKnob (transientMix, transArea.reduced (3));

    masterArea.removeFromTop (8);
    auto eqArea = masterArea.removeFromTop (146);
    const int eqWidth = eqArea.getWidth() / 5;
    layoutKnob (eqLow, eqArea.removeFromLeft (eqWidth).reduced (2));
    layoutKnob (eqLowMid, eqArea.removeFromLeft (eqWidth).reduced (2));
    layoutKnob (eqMid, eqArea.removeFromLeft (eqWidth).reduced (2));
    layoutKnob (eqHighMid, eqArea.removeFromLeft (eqWidth).reduced (2));
    layoutKnob (eqHigh, eqArea.reduced (2));
}

void SuperBassAudioProcessorEditor::layoutKnob (Knob& knob, juce::Rectangle<int> bounds)
{
    knob.label.setBounds (bounds.removeFromTop (20));
    knob.slider.setBounds (bounds);
}

void SuperBassAudioProcessorEditor::layoutChoice (Choice& choice, juce::Rectangle<int> bounds)
{
    choice.label.setBounds (bounds.removeFromTop (18));
    choice.box.setBounds (bounds.reduced (2, 4));
}

SuperBassAudioProcessorEditor::VuMeter::VuMeter (SuperBassAudioProcessor& p)
    : processor (p)
{
}

void SuperBassAudioProcessorEditor::VuMeter::setMode (int newMode)
{
    mode = juce::jlimit (0, 2, newMode);
    displayDb = mode == 0 ? processor.getInputMeterDb()
             : mode == 1 ? processor.getOutputMeterDb()
                         : processor.getGainReductionMeterDb();
}

void SuperBassAudioProcessorEditor::VuMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto face = juce::Colour (248, 252, 255);
    const auto ink = juce::Colour (32, 50, 68);
    const auto meterBlue = juce::Colour (36, 127, 220);

    g.setColour (shadow.withAlpha (0.2f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), 9.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, bounds.getX(), bounds.getY(),
                                             juce::Colour (224, 237, 248), bounds.getX(), bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 9.0f);
    g.setColour (panelOutline.withAlpha (0.9f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 9.0f, 1.1f);

    const auto rawDb = mode == 0 ? processor.getInputMeterDb()
                     : mode == 1 ? processor.getOutputMeterDb()
                                 : processor.getGainReductionMeterDb();
    const auto attack = 0.64f;
    const auto release = mode == 2 ? 0.125f : 0.098f;
    displayDb += (rawDb - displayDb) * (rawDb > displayDb ? attack : release);

    auto meter = getLocalBounds().reduced (10, 8);
    const auto meterFloat = meter.toFloat();
    g.setGradientFill (juce::ColourGradient (face, meterFloat.getX(), meterFloat.getY(),
                                             juce::Colour (229, 241, 250), meterFloat.getX(), meterFloat.getBottom(), false));
    g.fillRoundedRectangle (meterFloat, 7.0f);
    g.setColour (meterBlue.withAlpha (0.08f));
    g.fillRoundedRectangle (meterFloat.reduced (5.0f, 23.0f), 7.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.58f), meterFloat.getX(), meterFloat.getY(),
                                             juce::Colours::transparentWhite, meterFloat.getX(), meterFloat.getCentreY(), false));
    auto glassHighlight = meterFloat;
    g.fillRoundedRectangle (glassHighlight.removeFromTop (37.0f), 7.0f);
    g.setColour (panelOutline.withAlpha (0.65f));
    g.drawRoundedRectangle (meter.toFloat().reduced (0.5f), 7.0f, 1.0f);

    auto labelArea = meter.removeFromTop (18);
    g.setColour (ink.withAlpha (0.78f));
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.drawText (mode == 0 ? "IN" : mode == 1 ? "OUT" : "GR", labelArea, juce::Justification::centred);

    const auto centre = juce::Point<float> (bounds.getCentreX(), bounds.getBottom() - 18.0f);
    const auto radius = juce::jmin (bounds.getWidth() * 0.46f, bounds.getHeight() * 0.92f);
    const auto startAngle = -2.35f;
    const auto endAngle = -0.79f;
    const auto vuValue = displayDb + 18.0f;
    const auto norm = mode == 2
        ? 1.0f - juce::jlimit (0.0f, 1.0f, std::abs (displayDb) / 20.0f)
        : juce::jlimit (0.0f, 1.0f, (vuValue + 20.0f) / 26.0f);

    g.setColour (meterBlue.withAlpha (0.32f));
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const std::array<float, 9> tickValues = mode == 2
        ? std::array<float, 9> { 0.0f, 0.06f, 0.14f, 0.25f, 0.39f, 0.55f, 0.72f, 0.86f, 1.0f }
        : std::array<float, 9> { 0.0f, 0.16f, 0.32f, 0.48f, 0.64f, 0.77f, 0.86f, 0.93f, 1.0f };

    for (auto tick : tickValues)
    {
        const auto angle = startAngle + (endAngle - startAngle) * tick;
        const auto important = tick >= 0.77f || tick <= 0.001f || (tick > 0.45f && tick < 0.5f);
        const auto inner = centre + juce::Point<float> (std::cos (angle), std::sin (angle)) * (radius - (important ? 11.0f : 8.0f));
        const auto outer = centre + juce::Point<float> (std::cos (angle), std::sin (angle)) * (radius - 1.5f);
        g.setColour ((important ? ink : ink.withAlpha (0.58f)));
        g.drawLine (inner.x, inner.y, outer.x, outer.y, important ? 2.0f : 1.1f);
    }

    g.setFont (juce::FontOptions (12.2f, juce::Font::bold));
    g.setColour (ink.withAlpha (0.78f));
    const auto numberArea = getLocalBounds().reduced (15, 13).removeFromBottom (28);
    g.drawText (mode == 2 ? "-20" : "-20", numberArea, juce::Justification::bottomLeft);
    g.drawText (mode == 2 ? "0" : "+6", numberArea, juce::Justification::bottomRight);
    if (mode != 2)
        g.drawText ("-10", numberArea.withTrimmedLeft (45).withTrimmedRight (80),
                    juce::Justification::bottomLeft);

    if (mode != 2)
    {
        const auto markY = getLocalBounds().getY() + 52;
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.setColour (meterBlue.darker (0.08f).withAlpha (0.96f));
        g.drawText ("0", getLocalBounds().withTrimmedLeft (98).withTrimmedRight (47).withY (markY).withHeight (14),
                    juce::Justification::centred);
        g.drawText ("+3", getLocalBounds().withTrimmedLeft (126).withTrimmedRight (16).withY (markY).withHeight (14),
                    juce::Justification::centred);
    }

    const auto needleAngle = startAngle + (endAngle - startAngle) * norm;
    const auto needleEnd = centre + juce::Point<float> (std::cos (needleAngle), std::sin (needleAngle)) * (radius - 11.0f);
    g.setColour (meterBlue.withAlpha (0.18f));
    g.drawLine (centre.x, centre.y, needleEnd.x, needleEnd.y, 4.8f);
    g.setColour (juce::Colour (18, 92, 185));
    g.drawLine (centre.x, centre.y, needleEnd.x, needleEnd.y, 2.1f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, centre.x - 5.0f, centre.y - 5.0f,
                                             juce::Colour (152, 188, 220), centre.x + 5.0f, centre.y + 5.0f, false));
    g.fillEllipse (centre.x - 5.0f, centre.y - 5.0f, 10.0f, 10.0f);
    g.setColour (ink.withAlpha (0.55f));
    g.drawEllipse (centre.x - 5.0f, centre.y - 5.0f, 10.0f, 10.0f, 0.9f);

    auto valueText = mode == 2
        ? juce::String (displayDb, 1) + " dB"
        : juce::String (vuValue, 1) + " VU";
    g.setColour (ink);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText (valueText, getLocalBounds().removeFromBottom (16), juce::Justification::centred);
}

SuperBassAudioProcessorEditor::RouteStrip::RouteStrip (SuperBassAudioProcessor& p)
    : processor (p)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void SuperBassAudioProcessorEditor::RouteStrip::paint (juce::Graphics& g)
{
    syncFromParameter();
    const char* names[] { "OPEN", "SPACE", "MASTER" };

    const auto r = getLocalBounds().toFloat();
    g.setColour (shadow.withAlpha (0.14f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 9.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, r.getX(), r.getY(),
                                             glass, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (panelOutline.withAlpha (0.76f));
    g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.0f);

    for (int i = 0; i < 3; ++i)
    {
        auto slot = slotBounds (i);
        const auto active = i == dragging;
        g.setColour (active ? gold.withAlpha (0.18f) : juce::Colour (240, 247, 253));
        g.fillRoundedRectangle (slot.toFloat(), 7.0f);
        g.setColour (active ? gold : panelOutline.withAlpha (0.72f));
        g.drawRoundedRectangle (slot.toFloat(), 7.0f, 1.4f);
        g.setColour (active ? gold.darker (0.25f) : text);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawFittedText (names[order[static_cast<size_t> (i)]], slot, juce::Justification::centred, 1);

        if (i < 2)
        {
            auto from = slot.getRight() + 8;
            auto y = slot.getCentreY();
            auto to = slotBounds (i + 1).getX() - 8;
            g.setColour (gold.withAlpha (0.58f));
            g.drawLine (static_cast<float> (from), static_cast<float> (y), static_cast<float> (to), static_cast<float> (y), 2.0f);
            juce::Path arrow;
            arrow.addTriangle (static_cast<float> (to), static_cast<float> (y),
                               static_cast<float> (to - 8), static_cast<float> (y - 5),
                               static_cast<float> (to - 8), static_cast<float> (y + 5));
            g.fillPath (arrow);
        }
    }
}

void SuperBassAudioProcessorEditor::RouteStrip::mouseDown (const juce::MouseEvent& e)
{
    dragging = slotForPosition (e.x);
    repaint();
}

void SuperBassAudioProcessorEditor::RouteStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;

    const auto target = slotForPosition (e.x);
    if (target >= 0 && target != dragging)
    {
        std::swap (order[static_cast<size_t> (dragging)], order[static_cast<size_t> (target)]);
        dragging = target;
        publishOrder();
        repaint();
    }
}

void SuperBassAudioProcessorEditor::RouteStrip::mouseUp (const juce::MouseEvent&)
{
    dragging = -1;
    publishOrder();
    repaint();
}

void SuperBassAudioProcessorEditor::RouteStrip::syncFromParameter()
{
    if (dragging >= 0)
        return;

    switch (static_cast<int> (processor.apvts.getRawParameterValue ("routingOrder")->load()))
    {
        case 1: order = { 0, 2, 1 }; break;
        case 2: order = { 1, 0, 2 }; break;
        case 3: order = { 1, 2, 0 }; break;
        case 4: order = { 2, 0, 1 }; break;
        case 5: order = { 2, 1, 0 }; break;
        default: order = { 0, 1, 2 }; break;
    }
}

void SuperBassAudioProcessorEditor::RouteStrip::publishOrder()
{
    if (auto* param = processor.apvts.getParameter ("routingOrder"))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (static_cast<float> (orderToChoice (order)) / 5.0f);
        param->endChangeGesture();
    }
}

int SuperBassAudioProcessorEditor::RouteStrip::slotForPosition (int x) const
{
    for (int i = 0; i < 3; ++i)
        if (slotBounds (i).contains (x, getHeight() / 2))
            return i;

    return juce::jlimit (0, 2, x * 3 / juce::jmax (1, getWidth()));
}

juce::Rectangle<int> SuperBassAudioProcessorEditor::RouteStrip::slotBounds (int index) const
{
    auto area = getLocalBounds().reduced (16, 12);
    const auto slotWidth = (area.getWidth() - 54) / 3;
    return { area.getX() + index * (slotWidth + 27), area.getY(), slotWidth, area.getHeight() };
}

int SuperBassAudioProcessorEditor::RouteStrip::orderToChoice (const std::array<int, 3>& order)
{
    if (order == std::array<int, 3> { 0, 2, 1 }) return 1;
    if (order == std::array<int, 3> { 1, 0, 2 }) return 2;
    if (order == std::array<int, 3> { 1, 2, 0 }) return 3;
    if (order == std::array<int, 3> { 2, 0, 1 }) return 4;
    if (order == std::array<int, 3> { 2, 1, 0 }) return 5;
    return 0;
}
