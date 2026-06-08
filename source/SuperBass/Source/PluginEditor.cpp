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
constexpr int meterTextColourId = 0x2000100;
constexpr int meterMutedColourId = 0x2000101;
constexpr int meterAccentColourId = 0x2000102;
constexpr int meterBackgroundColourId = 0x2000103;

juce::Colour lerpColour (juce::Colour a, juce::Colour b, float amount)
{
    return a.interpolatedWith (b, juce::jlimit (0.0f, 1.0f, amount));
}

juce::Point<float> pointOnKnobArc (juce::Point<float> centre, float radius, float angle)
{
    return centre.getPointOnCircumference (radius, angle);
}

int scaleInt (float scale, float value)
{
    return juce::roundToInt (value * scale);
}

float fractFloat (float value)
{
    return value - std::floor (value);
}

float smootherStep (float value)
{
    const auto x = juce::jlimit (0.0f, 1.0f, value);
    return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
}

float liquidHash (int index, int seed)
{
    const auto value = std::sin (static_cast<float> (index) * 12.9898f
                               + static_cast<float> (seed) * 78.233f) * 43758.5453f;
    return fractFloat (value) * 2.0f - 1.0f;
}

float periodicValueNoise (float norm, int cells, float phase, int seed)
{
    const auto wrapped = fractFloat (norm);
    const auto x = wrapped * static_cast<float> (cells) + phase;
    const auto base = static_cast<int> (std::floor (x));
    const auto frac = smootherStep (x - std::floor (x));
    const auto i0 = ((base % cells) + cells) % cells;
    const auto i1 = (i0 + 1) % cells;
    return juce::jmap (frac, liquidHash (i0, seed), liquidHash (i1, seed));
}

float liquidNoise (float norm, float phase, float audioEnergy, int seed)
{
    const auto octaveA = periodicValueNoise (norm, 7, phase * (1.18f + audioEnergy * 0.48f), seed);
    const auto octaveB = periodicValueNoise (norm + 0.073f, 13, -phase * (0.9f + audioEnergy * 0.34f), seed + 17);
    const auto octaveC = periodicValueNoise (norm + 0.191f, 23, phase * (0.58f + audioEnergy * 0.24f), seed + 41);
    const auto micro = periodicValueNoise (norm + 0.037f, 43, -phase * (1.92f + audioEnergy * 0.7f), seed + 89);
    const auto shimmer = periodicValueNoise (norm + 0.149f, 59, phase * (1.46f + audioEnergy * 0.52f), seed + 113);
    const auto tremble = periodicValueNoise (norm + 0.223f, 79, phase * (1.16f + audioEnergy * 0.44f), seed + 131);
    const auto softMicro = std::tanh ((micro * 0.48f + shimmer * 0.32f + tremble * 0.2f) * 1.28f) * (0.1f + audioEnergy * 0.045f);
    return octaveA * 0.5f + octaveB * 0.27f + octaveC * 0.15f + softMicro;
}

void setToggleStateIfChanged (juce::Button& button, bool shouldBeOn)
{
    if (button.getToggleState() != shouldBeOn)
        button.setToggleState (shouldBeOn, juce::dontSendNotification);
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

void drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& name, juce::Colour titleColour = muted)
{
    g.setColour (titleColour);
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    g.drawFittedText (name, area.removeFromTop (18), juce::Justification::centred, 1, 0.72f);
}

void drawPanel (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour accent, float phase, float smileAmount = 1.0f)
{
    const auto r = area.toFloat();
    auto localPhase = phase + accent.getFloatBlue() * 0.17f;
    localPhase -= std::floor (localPhase);
    const auto sweepX = r.getX() - r.getWidth() * 0.55f + localPhase * r.getWidth() * 1.85f;
    const auto dawn = juce::jlimit (0.0f, 1.0f, smileAmount * 2.0f);
    const auto day = juce::jlimit (0.0f, 1.0f, smileAmount * 2.0f - 1.0f);
    const auto lit = juce::jlimit (0.0f, 1.0f, 0.25f + smileAmount * 0.9f);
    const auto panelTop = lerpColour (lerpColour (juce::Colour (15, 24, 49), juce::Colour (54, 66, 86), dawn), panel, day);
    const auto panelBottom = lerpColour (lerpColour (juce::Colour (25, 40, 72), juce::Colour (238, 225, 205), dawn),
                                         juce::Colour (239, 246, 252), day);
    const auto outline = lerpColour (accent.withAlpha (0.55f), panelOutline.withAlpha (0.72f), lit);
    const auto topGloss = lerpColour (juce::Colours::white.withAlpha (0.08f), juce::Colours::white.withAlpha (0.5f), lit);

    g.setColour (shadow.withAlpha (0.14f + 0.06f * day));
    g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 10.0f);
    g.setGradientFill (juce::ColourGradient (panelTop, r.getX(), r.getY(),
                                             panelBottom, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 10.0f);
    drawMaterialGrain (g, r.reduced (4.0f), juce::Colour (82, 116, 148), 0.035f + 0.012f * (1.0f - day));
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentWhite, sweepX - 74.0f, r.getY(),
                                             accent.withAlpha (0.045f + 0.04f * lit), sweepX + 34.0f, r.getCentreY(), false));
    g.fillRoundedRectangle (r.reduced (3.0f), 8.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    g.setColour (accent.withAlpha (0.42f + 0.26f * lit));
    g.drawRoundedRectangle (r.reduced (1.6f), 8.5f, 1.4f);
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.1f + 0.08f * lit), r.getX(), r.getY(),
                                             accent.withAlpha (0.02f + 0.015f * day), r.getX(), r.getY() + 60.0f, false));
    auto header = r;
    g.fillRoundedRectangle (header.removeFromTop (42.0f), 9.0f);
    auto highlight = r.reduced (5.0f);
    g.setGradientFill (juce::ColourGradient (topGloss, highlight.getX(), highlight.getY(),
                                             juce::Colours::transparentWhite, highlight.getX(), highlight.getY() + 76.0f, false));
    g.fillRoundedRectangle (highlight.removeFromTop (52.0f), 8.0f);
}

void drawArrowButtonGlyph (juce::Graphics& g, juce::Rectangle<int> area, bool pointsRight, juce::Colour accent)
{
    const auto r = area.toFloat().reduced (5.0f);
    g.setColour (shadow.withAlpha (0.18f));
    g.fillEllipse (r.translated (0.0f, 2.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, r.getX(), r.getY(),
                                             glass, r.getRight(), r.getBottom(), false));
    g.fillEllipse (r);
    g.setColour (panelOutline.withAlpha (0.86f));
    g.drawEllipse (r.reduced (0.5f), 1.2f);

    juce::Path arrow;
    const auto cx = r.getCentreX();
    const auto cy = r.getCentreY();
    const auto dir = pointsRight ? 1.0f : -1.0f;
    arrow.startNewSubPath (cx - dir * 9.0f, cy - 8.0f);
    arrow.lineTo (cx + dir * 8.0f, cy);
    arrow.lineTo (cx - dir * 9.0f, cy + 8.0f);
    g.setColour (accent);
    g.strokePath (arrow, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
}

void SuperBassAudioProcessorEditor::TechLookAndFeel::setUiScale (float newScale) noexcept
{
    uiScale = juce::jlimit (0.5f, 2.0f, newScale);
}

juce::Font SuperBassAudioProcessorEditor::TechLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::FontOptions (juce::jlimit (10.0f * uiScale, 15.5f * uiScale, static_cast<float> (buttonHeight) * 0.48f),
                              juce::Font::bold);
}

juce::Font SuperBassAudioProcessorEditor::TechLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::FontOptions (13.0f * uiScale, juce::Font::bold);
}

juce::Font SuperBassAudioProcessorEditor::TechLookAndFeel::getPopupMenuFont()
{
    return juce::FontOptions (15.0f * uiScale, juce::Font::bold);
}

void SuperBassAudioProcessorEditor::TechLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                                       float sliderPos, float rotaryStartAngle,
                                                                       float rotaryEndAngle, juce::Slider& slider)
{
    const auto s = juce::jlimit (0.5f, 2.0f, uiScale);
    const auto area = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                             static_cast<float> (width), static_cast<float> (height)).reduced (7.0f, 5.0f);
    const auto diameter = juce::jmin (area.getWidth(), area.getHeight() - 16.0f * s);
    const auto knob = juce::Rectangle<float> (diameter, diameter).withCentre (area.getCentre().translated (0.0f, -5.0f * s));
    const auto centre = knob.getCentre();
    const auto radius = knob.getWidth() * 0.5f;
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const auto base = juce::Colour (248, 251, 254);
    const auto rim = juce::Colour (178, 202, 224);
    const auto isSmile = slider.getName() == "smile";

    if (isSmile)
    {
        const auto inner = knob.reduced (radius * 0.23f);
        const auto face = knob.reduced (radius * 0.15f);

        g.setColour (shadow.withAlpha (0.24f));
        g.fillEllipse (knob.translated (0.0f, 8.0f * s));

        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.22f), knob.getX(), knob.getY(),
                                                 juce::Colour (244, 249, 253).withAlpha (0.88f), knob.getRight(), knob.getBottom(), false));
        g.fillEllipse (knob);
        drawMaterialGrain (g, knob.reduced (8.0f * s), juce::Colour (52, 86, 126), 0.05f);

        for (int ring = 0; ring < 3; ++ring)
        {
            const auto inset = 7.0f * s + static_cast<float> (ring) * 18.0f * s;
            g.setColour ((ring == 0 ? accent : juce::Colours::white).withAlpha (ring == 0 ? 0.62f : 0.22f));
            g.drawEllipse (knob.reduced (inset), ring == 0 ? 2.4f * s : 1.0f * s);
        }

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius - 24.0f * s, radius - 24.0f * s, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colour (151, 177, 204).withAlpha (0.45f));
        g.strokePath (track, juce::PathStrokeType (8.0f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc (centre.x, centre.y, radius - 24.0f * s, radius - 24.0f * s, 0.0f,
                                rotaryStartAngle, angle, true);
        g.setGradientFill (juce::ColourGradient (accent.brighter (0.55f), knob.getX(), knob.getY(),
                                                 accent.darker (0.22f), knob.getRight(), knob.getBottom(), false));
        g.strokePath (valueArc, juce::PathStrokeType (9.0f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.94f), face.getX(), face.getY(),
                                                 accent.withAlpha (0.15f), face.getRight(), face.getBottom(), false));
        g.fillEllipse (face);
        g.setColour (accent.withAlpha (0.5f));
        g.drawEllipse (face.reduced (1.0f * s), 1.4f * s);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.55f), inner.getX(), inner.getY(),
                                                 juce::Colours::transparentWhite, inner.getX(), inner.getCentreY(), false));
        g.fillEllipse (inner.withTrimmedBottom (inner.getHeight() * 0.42f));

        const auto pointerStart = pointOnKnobArc (centre, radius * 0.38f, angle);
        const auto pointerEnd = pointOnKnobArc (centre, radius * 0.76f, angle);
        g.setColour (accent.withAlpha (0.2f));
        g.drawLine (centre.x, centre.y, pointerEnd.x, pointerEnd.y, 9.0f * s);
        g.setColour (accent.darker (0.08f));
        g.drawLine (pointerStart.x, pointerStart.y, pointerEnd.x, pointerEnd.y, 4.2f * s);

        g.setColour (juce::Colour (28, 43, 62).withAlpha (0.92f));
        g.setFont (juce::FontOptions (32.0f * s, juce::Font::bold));
        g.drawFittedText ("SMILE", face.toNearestInt().withTrimmedTop (static_cast<int> (face.getHeight() * 0.36f)).withHeight (44),
                          juce::Justification::centred, 1);
        g.setColour (accent.withAlpha (0.92f));
        g.setFont (juce::FontOptions (16.0f * s, juce::Font::bold));
        g.drawFittedText (juce::String (juce::roundToInt (sliderPos * 100.0f)) + "%",
                          face.toNearestInt().withTrimmedTop (static_cast<int> (face.getHeight() * 0.58f)).withHeight (28),
                          juce::Justification::centred, 1);
        return;
    }

    const auto tickOuter = radius - 1.0f;
    const auto tickInner = radius - 7.0f * s;
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
        const auto length = major ? 8.0f * s : 5.0f * s;
        const auto inner = pointOnKnobArc (centre, tickInner - length, tickAngle);
        const auto outer = pointOnKnobArc (centre, tickOuter, tickAngle);
        g.setColour ((active ? accent : juce::Colour (176, 198, 219)).withAlpha (active ? 0.82f : 0.42f));
        g.drawLine (inner.x, inner.y, outer.x, outer.y, major ? 1.55f * s : 1.0f * s);
    }

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - 10.0f * s, radius - 10.0f * s, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (203, 218, 232));
    g.strokePath (track, juce::PathStrokeType (3.3f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, radius - 10.0f * s, radius - 10.0f * s, 0.0f,
                            rotaryStartAngle, angle, true);
    g.setColour (accent);
    g.strokePath (valueArc, juce::PathStrokeType (3.8f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto defaultInner = pointOnKnobArc (centre, radius - 18.0f * s, defaultAngle);
    const auto defaultOuter = pointOnKnobArc (centre, radius + 1.5f * s, defaultAngle);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.drawLine (defaultInner.x, defaultInner.y, defaultOuter.x, defaultOuter.y, 3.0f * s);
    g.setColour (accent.withAlpha (0.65f));
    g.drawLine (defaultInner.x, defaultInner.y, defaultOuter.x, defaultOuter.y, 1.25f * s);

    g.setGradientFill (juce::ColourGradient (juce::Colours::white, knob.getX() + knob.getWidth() * 0.24f, knob.getY(),
                                             juce::Colour (220, 233, 246), knob.getRight(), knob.getBottom(), false));
    g.fillEllipse (knob.reduced (9.0f * s));
    for (int ring = 0; ring < 5; ++ring)
    {
        const auto inset = (15.0f + static_cast<float> (ring) * 5.5f) * s;
        g.setColour ((ring % 2 == 0 ? juce::Colours::white : rim).withAlpha (ring % 2 == 0 ? 0.18f : 0.14f));
        g.drawEllipse (knob.reduced (inset), 0.75f * s);
    }
    {
        juce::Path grainClip;
        grainClip.addEllipse (knob.reduced (13.0f * s));
        g.saveState();
        g.reduceClipRegion (grainClip);
        drawMaterialGrain (g, knob.reduced (13.0f * s), juce::Colour (66, 103, 136), 0.035f);
        g.restoreState();
    }
    g.setColour (rim.withAlpha (0.9f));
    g.drawEllipse (knob.reduced (9.0f * s), 1.2f * s);
    g.setColour (juce::Colours::white.withAlpha (0.72f));
    g.drawEllipse (knob.reduced (13.0f * s), 1.0f * s);
    g.setColour (juce::Colours::white.withAlpha (0.34f));
    g.fillEllipse (knob.reduced (18.0f * s).withTrimmedBottom (knob.getHeight() * 0.38f));

    const auto pointerLength = radius - 16.0f * s;
    const auto pointerStart = pointOnKnobArc (centre, 8.0f * s, angle);
    const auto pointerEnd = pointOnKnobArc (centre, pointerLength, angle);
    const auto haloEnd = pointOnKnobArc (centre, radius - 3.0f * s, angle);
    g.setColour (accent.withAlpha (0.18f));
    g.drawLine (centre.x, centre.y, haloEnd.x, haloEnd.y, 7.0f * s);
    g.setColour (accent.darker (0.05f));
    g.drawLine (pointerStart.x, pointerStart.y, pointerEnd.x, pointerEnd.y, 2.6f * s);
    g.setColour (juce::Colours::white.withAlpha (0.95f));
    g.fillEllipse (centre.x - 3.2f * s, centre.y - 3.2f * s, 6.4f * s, 6.4f * s);
    g.setColour (base.darker (0.1f));
    g.drawEllipse (centre.x - 3.2f * s, centre.y - 3.2f * s, 6.4f * s, 6.4f * s, 0.8f * s);
}

void SuperBassAudioProcessorEditor::TechLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                                           const juce::Colour& backgroundColour,
                                                                           bool shouldDrawButtonAsHighlighted,
                                                                           bool shouldDrawButtonAsDown)
{
    const auto s = juce::jlimit (0.5f, 2.0f, uiScale);
    auto r = button.getLocalBounds().toFloat().reduced (1.0f);
    const auto active = button.getToggleState() || shouldDrawButtonAsDown;
    const auto accent = button.findColour (juce::TextButton::buttonOnColourId);
    const auto fill = active ? accent : backgroundColour;

    g.setColour (shadow.withAlpha (active ? 0.08f : 0.14f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f * s), 7.0f * s);
    g.setGradientFill (juce::ColourGradient (fill.brighter (active ? 0.15f : 0.04f), r.getX(), r.getY(),
                                             fill.darker (active ? 0.04f : 0.02f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 7.0f * s);
    g.setColour ((active ? accent.darker (0.1f) : panelOutline).withAlpha (shouldDrawButtonAsHighlighted ? 0.95f : 0.68f));
    g.drawRoundedRectangle (r.reduced (0.4f * s), 7.0f * s, active ? 1.35f * s : 1.0f * s);
    if (active)
    {
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.fillRoundedRectangle (r.removeFromTop (r.getHeight() * 0.45f), 7.0f * s);
    }
}

void SuperBassAudioProcessorEditor::TechLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                                                     bool, bool)
{
    const auto s = juce::jlimit (0.5f, 2.0f, uiScale);
    auto textArea = button.getLocalBounds().reduced (juce::roundToInt (5.0f * s), juce::roundToInt (2.0f * s));
    const auto textToDraw = button.getButtonText();
    const auto active = button.getToggleState();
    auto fontSize = juce::jmin (14.5f * s, static_cast<float> (button.getHeight()) * 0.46f);
    if (textToDraw.length() <= 2)
        fontSize = juce::jmin (13.5f * s, static_cast<float> (button.getHeight()) * 0.5f);

    g.setColour (button.findColour (active ? juce::TextButton::textColourOnId
                                           : juce::TextButton::textColourOffId)
                    .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.45f));
    g.setFont (juce::FontOptions (juce::jmax (8.0f, fontSize), juce::Font::bold));
    g.drawFittedText (textToDraw, textArea, juce::Justification::centred, 1, 0.72f);
}

void SuperBassAudioProcessorEditor::TechLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                                                       bool shouldDrawButtonAsHighlighted,
                                                                       bool shouldDrawButtonAsDown)
{
    const auto s = juce::jlimit (0.5f, 2.0f, uiScale);
    auto r = button.getLocalBounds().toFloat().reduced (2.0f * s, 4.0f * s);
    const auto active = button.getToggleState();
    const auto accent = button.findColour (juce::ToggleButton::tickColourId);
    const auto toggle = r.removeFromLeft (44.0f * s).reduced (0.0f, 2.0f * s);

    g.setColour ((active ? accent : juce::Colour (219, 230, 240)).withAlpha (shouldDrawButtonAsHighlighted ? 1.0f : 0.9f));
    g.fillRoundedRectangle (toggle, toggle.getHeight() * 0.5f);
    g.setColour (panelOutline.withAlpha (0.78f));
    g.drawRoundedRectangle (toggle.reduced (0.5f * s), toggle.getHeight() * 0.5f, 1.0f * s);
    const auto dotSize = toggle.getHeight() - 7.0f * s;
    const auto dotX = active ? toggle.getRight() - dotSize - 3.5f * s : toggle.getX() + 3.5f * s;
    g.setColour (juce::Colours::white);
    g.fillEllipse (dotX, toggle.getY() + 3.5f * s, dotSize, dotSize);
    g.setColour (shadow.withAlpha (0.25f));
    g.drawEllipse (dotX, toggle.getY() + 3.5f * s, dotSize, dotSize, 0.8f * s);

    g.setColour (active || shouldDrawButtonAsDown ? text : muted);
    g.setFont (juce::FontOptions (13.0f * s, juce::Font::bold));
    g.drawFittedText (button.getButtonText(), r.toNearestInt(), juce::Justification::centredLeft, 1);
}

void SuperBassAudioProcessorEditor::TechLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                                                   int, int, int, int, juce::ComboBox& box)
{
    const auto s = juce::jlimit (0.5f, 2.0f, uiScale);
    auto r = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height)).reduced (0.5f);
    const auto boxBackground = box.findColour (juce::ComboBox::backgroundColourId);
    const auto outline = box.findColour (juce::ComboBox::outlineColourId);
    const auto arrowColour = box.findColour (juce::ComboBox::arrowColourId);
    g.setColour (shadow.withAlpha (0.12f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f * s), 7.0f * s);
    g.setGradientFill (juce::ColourGradient (boxBackground.brighter (0.08f), r.getX(), r.getY(),
                                             boxBackground.darker (0.04f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 7.0f * s);
    g.setColour ((isButtonDown ? arrowColour : outline).withAlpha (0.9f));
    g.drawRoundedRectangle (r.reduced (0.5f * s), 7.0f * s, 1.15f * s);

    juce::Path arrow;
    const auto cx = r.getRight() - 17.0f * s;
    const auto cy = r.getCentreY() + 1.0f * s;
    arrow.addTriangle (cx - 4.0f * s, cy - 2.0f * s, cx + 4.0f * s, cy - 2.0f * s, cx, cy + 4.0f * s);
    g.setColour (arrowColour);
    g.fillPath (arrow);
}

SuperBassAudioProcessorEditor::SuperBassAudioProcessorEditor (SuperBassAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), routeStrip (p), meterStrip (p), vuMeter (p)
{
    setLookAndFeel (&techLookAndFeel);
    setOpaque (true);
    setBufferedToImage (false);
    setSize (1240, 740);
    setResizable (true, true);
    setResizeLimits (620, 370, 2480, 1480);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (1240.0 / 740.0);

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
    setupModeButton (haasButton, "MOD 1", 0);
    setupModeButton (flangerButton, "MOD 2", 1);
    setupModeButton (phaserButton, "MOD 3", 2);
    setupMeterButton (meterInButton, "IN", 0);
    setupMeterButton (meterOutButton, "OUT", 1);
    setupMeterButton (meterGrButton, "GR", 2);
    setupOversamplingButton (oversampling1xButton, "x1", 0);
    setupOversamplingButton (oversampling2xButton, "x2", 1);
    setupOversamplingButton (oversampling4xButton, "x4", 2);
    setupOversamplingButton (oversampling8xButton, "x8", 3);
    oversamplingLabel.setText ("OVERSAMPLING", juce::dontSendNotification);
    oversamplingLabel.setJustificationType (juce::Justification::centred);
    oversamplingLabel.setColour (juce::Label::textColourId, muted);
    oversamplingLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    addAndMakeVisible (oversamplingLabel);
    setupPresetControls();
    nextViewButton.setButtonText ({});
    nextViewButton.setColour (juce::TextButton::buttonColourId, juce::Colour (239, 246, 252));
    nextViewButton.setColour (juce::TextButton::buttonOnColourId, gold);
    nextViewButton.setColour (juce::TextButton::textColourOffId, text);
    nextViewButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    nextViewButton.onClick = [this] { setActiveScreen (1); };
    addAndMakeVisible (nextViewButton);

    backViewButton.setButtonText ({});
    backViewButton.setColour (juce::TextButton::buttonColourId, juce::Colour (239, 246, 252));
    backViewButton.setColour (juce::TextButton::buttonOnColourId, gold);
    backViewButton.setColour (juce::TextButton::textColourOffId, text);
    backViewButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    backViewButton.onClick = [this] { setActiveScreen (0); };
    addAndMakeVisible (backViewButton);

    addAndMakeVisible (meterStrip);
    addAndMakeVisible (vuMeter);
    addAndMakeVisible (routeStrip);
    subFreq.slider.onValueChange = [this] { syncLinkedOpenFrequency ("openSubFreq"); };
    bodyFreq.slider.onValueChange = [this] { syncLinkedOpenFrequency ("openBodyFreq"); };
    openFreqLinkButton.onClick = [this]
    {
        setOpenLinkEnabled (openFreqLinkButton.getToggleState());
    };
    refreshPresetList();
    if (activeScreen == 1)
    {
        updateOpenModeButtons();
        updateOpenLinkButton();
        updateWideModeButtons();
        updateMeterButtons();
    }
    updateOversamplingButtons();
    updateDynamicUiColours();
    updateScreenVisibility();
    updateScaledFonts();
    startTimerHz (48);
}

SuperBassAudioProcessorEditor::~SuperBassAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void SuperBassAudioProcessorEditor::setupKnob (Knob& knob, const juce::String& labelText, const juce::String& paramId)
{
    knob.slider.setName (paramId);
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
    if (paramId == "smile")
    {
        knob.slider.setBufferedToImage (false);
        knob.slider.setOpaque (false);
    }

    knob.label.setText (labelText, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    knob.label.setColour (juce::Label::textColourId, muted);
    knob.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    knob.label.setMinimumHorizontalScale (0.72f);
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
    setToggleStateIfChanged (punchButton, mode == 0);
    setToggleStateIfChanged (bassHeadButton, mode == 1);
    setToggleStateIfChanged (boomButton, mode == 2);
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
    setToggleStateIfChanged (openFreqLinkButton, linked);
    if (linked)
        syncLinkedOpenFrequency ("openSubFreq");
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
    setToggleStateIfChanged (haasButton, mode == 0);
    setToggleStateIfChanged (flangerButton, mode == 1);
    setToggleStateIfChanged (phaserButton, mode == 2);
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
    setToggleStateIfChanged (oversampling1xButton, mode == 0);
    setToggleStateIfChanged (oversampling2xButton, mode == 1);
    setToggleStateIfChanged (oversampling4xButton, mode == 2);
    setToggleStateIfChanged (oversampling8xButton, mode == 3);
}

void SuperBassAudioProcessorEditor::setMeterMode (int mode)
{
    meterMode = juce::jlimit (0, 2, mode);
    vuMeter.setMode (meterMode);
    updateMeterButtons();
}

void SuperBassAudioProcessorEditor::updateMeterButtons()
{
    setToggleStateIfChanged (meterInButton, meterMode == 0);
    setToggleStateIfChanged (meterOutButton, meterMode == 1);
    setToggleStateIfChanged (meterGrButton, meterMode == 2);
}

void SuperBassAudioProcessorEditor::setActiveScreen (int screenIndex)
{
    activeScreen = juce::jlimit (0, 1, screenIndex);
    lastAdvancedRepaintSmile = -1.0f;
    lastAdvancedRepaintRoutingChoice = -1;
    invalidateThemeCache();
    updateScreenVisibility();
    updateDynamicUiColours();
    resized();
    repaint();
}

float SuperBassAudioProcessorEditor::getSmileNormalised() const
{
    if (auto* value = processor.apvts.getRawParameterValue ("smile"))
        return juce::jlimit (0.0f, 1.0f, value->load() / 100.0f);

    return 1.0f;
}

void SuperBassAudioProcessorEditor::invalidateThemeCache()
{
    themeCache = {};
    themeCacheWidth = 0;
    themeCacheHeight = 0;
    themeCacheScreen = -1;
    lastRenderedThemeSmileAmount = -1.0f;
    lastRenderedThemePhase = -1.0f;
    lastAdvancedRepaintSmile = -1.0f;
    lastAdvancedRepaintRoutingChoice = -1;
}

void SuperBassAudioProcessorEditor::renderThemeLayer (juce::Graphics& g, juce::Rectangle<int> canvas,
                                                      float smileAmount, float loopPhase)
{
    const auto full = canvas.toFloat();
    const auto dawn = juce::jlimit (0.0f, 1.0f, smileAmount * 2.0f);
    const auto day = juce::jlimit (0.0f, 1.0f, smileAmount * 2.0f - 1.0f);
    const auto nightTop = juce::Colour (8, 15, 35);
    const auto nightBottom = juce::Colour (20, 35, 70);
    const auto sunriseTop = juce::Colour (255, 180, 104);
    const auto sunriseBottom = juce::Colour (106, 166, 234);
    const auto dayTop = juce::Colour (252, 254, 255);
    const auto dayBottom = background;
    const auto top = lerpColour (lerpColour (nightTop, sunriseTop, dawn), dayTop, day);
    const auto bottom = lerpColour (lerpColour (nightBottom, sunriseBottom, dawn), dayBottom, day);
    const auto dynamicAccent = lerpColour (lerpColour (juce::Colour (91, 143, 255), juce::Colour (255, 150, 74), dawn),
                                           gold, day);

    g.setGradientFill (juce::ColourGradient (top, full.getX(), full.getY(), bottom, full.getX(), full.getBottom(), false));
    g.fillAll();

    if (smileAmount < 0.42f)
    {
        g.setColour (juce::Colours::white.withAlpha ((0.42f - smileAmount) * 0.36f));
        for (int i = 0; i < 58; ++i)
        {
            const auto x = full.getX() + 24.0f + static_cast<float> ((i * 97) % juce::jmax (1, canvas.getWidth() - 48));
            const auto y = full.getY() + 82.0f + static_cast<float> ((i * 53) % juce::jmax (1, canvas.getHeight() - 130));
            const auto twinkle = 0.65f + 0.35f * std::sin (visualMotionPhase * juce::MathConstants<float>::twoPi + static_cast<float> (i) * 0.71f);
            const auto size = (1.0f + static_cast<float> ((i * 11) % 3) * 0.45f) * twinkle;
            g.fillEllipse (x, y, size, size);
        }
    }
    else if (smileAmount > 0.72f)
    {
        const auto flare = juce::jlimit (0.0f, 1.0f, (smileAmount - 0.72f) / 0.28f);
        const auto centre = full.getCentre().translated (-180.0f * uiScale + 28.0f * uiScale * std::sin (visualMotionPhase * 4.0f),
                                                         -170.0f * uiScale);
        const auto flareRadius = 520.0f * uiScale;
        g.setGradientFill (juce::ColourGradient (juce::Colour (255, 204, 102).withAlpha (0.16f * flare), centre.x, centre.y,
                                                 juce::Colours::transparentWhite, centre.x + flareRadius, centre.y + 380.0f * uiScale, true));
        g.fillEllipse (centre.x - flareRadius, centre.y - flareRadius, flareRadius * 2.0f, flareRadius * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.035f * flare));
        for (int y = canvas.getY() + scaleInt (uiScale, 120.0f); y < canvas.getBottom() - scaleInt (uiScale, 90.0f); y += scaleInt (uiScale, 22.0f))
            g.drawHorizontalLine (y + static_cast<int> (std::sin (visualMotionPhase * 5.0f + static_cast<float> (y) * 0.04f) * 2.0f),
                                  full.getX() + 28.0f * uiScale, full.getRight() - 28.0f * uiScale);
    }

    drawMaterialGrain (g, full.reduced (8.0f * uiScale), juce::Colour (84, 116, 150), 0.025f + 0.016f * day);

    const auto gridFade = juce::jlimit (0.0f, 1.0f, (smileAmount - 0.32f) / 0.38f);
    if (gridFade > 0.0f)
    {
        g.setColour (juce::Colours::white.withAlpha ((0.06f + 0.18f * day) * gridFade));
        for (int y = canvas.getY() + scaleInt (uiScale, 34.0f); y < canvas.getBottom(); y += scaleInt (uiScale, 34.0f))
            g.drawHorizontalLine (y, full.getX() + 22.0f * uiScale, full.getRight() - 22.0f * uiScale);
        g.setColour (juce::Colours::white.withAlpha ((0.04f + 0.2f * day) * gridFade));
        for (int x = canvas.getX() + scaleInt (uiScale, 42.0f); x < canvas.getRight(); x += scaleInt (uiScale, 64.0f))
            g.drawVerticalLine (x, full.getY() + 18.0f * uiScale, full.getBottom() - 18.0f * uiScale);
    }

    g.setColour (lerpColour (juce::Colour (16, 27, 54), juce::Colours::white, 0.48f + day * 0.42f).withAlpha (0.42f + day * 0.2f));
    g.fillRoundedRectangle (full.reduced (10.0f * uiScale), 14.0f * uiScale);
    g.setColour (lerpColour (dynamicAccent, panelOutline, day).withAlpha (0.36f + day * 0.12f));
    g.drawRoundedRectangle (full.reduced (10.5f * uiScale), 14.0f * uiScale, juce::jmax (1.0f, uiScale));

    const auto sweepX = full.getX() - full.getWidth() * 0.35f + loopPhase * full.getWidth() * 1.7f;
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentWhite, sweepX - 180.0f * uiScale, full.getY(),
                                             dynamicAccent.withAlpha (0.055f + smileAmount * 0.04f), sweepX, full.getCentreY(), true));
    g.fillRoundedRectangle (full.reduced (14.0f * uiScale), 12.0f * uiScale);
}

void SuperBassAudioProcessorEditor::drawSmileWaveOrbit (juce::Graphics& g, juce::Point<float> centre,
                                                        juce::Colour accent, juce::Colour textColour,
                                                        float smileAmount)
{
    const auto liquidEnergy = juce::jlimit (0.0f, 1.0f, smileLiquidEnv);
    const auto rippleEnergy = juce::jlimit (0.0f, 1.0f, smileRippleEnv * 1.8f);
    const auto flowPhase = visualMotionPhase * (0.48f + liquidEnergy * 0.9f + rippleEnergy * 0.58f);
    const auto baseRadius = (206.0f + liquidEnergy * 14.0f + rippleEnergy * 18.0f) * uiScale;
    const auto warpAmount = (12.0f + liquidEnergy * 34.0f + rippleEnergy * 26.0f) * uiScale;
    const auto ribbonWidth = (18.0f + liquidEnergy * 18.0f + rippleEnergy * 16.0f) * uiScale;
    const auto glowRadius = baseRadius + warpAmount + ribbonWidth + 18.0f * uiScale;
    const auto glowBounds = juce::Rectangle<float> (glowRadius * 2.0f, glowRadius * 2.0f).withCentre (centre);

    juce::Path clipPath;
    clipPath.addEllipse (glowBounds.expanded (12.0f * uiScale));
    g.saveState();
    g.reduceClipRegion (clipPath);

    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.08f + 0.14f * liquidEnergy + 0.08f * rippleEnergy), centre.x, centre.y,
                                             juce::Colours::transparentWhite, centre.x + glowRadius, centre.y, true));
    g.fillEllipse (glowBounds);

    auto radiusAt = [this, baseRadius, warpAmount, ribbonWidth, flowPhase, liquidEnergy, rippleEnergy] (float norm, float edge, int seed)
    {
        const auto drift = liquidNoise (norm, flowPhase, liquidEnergy + rippleEnergy, seed);
        const auto thicknessDrift = liquidNoise (norm + 0.117f, -flowPhase * 0.74f, liquidEnergy + rippleEnergy, seed + 73);
        const auto outwardRipple = std::sin ((norm * juce::MathConstants<float>::twoPi * (2.3f + rippleEnergy * 1.4f))
                                           - visualMotionPhase * (4.4f + liquidEnergy * 2.2f + rippleEnergy * 1.6f)) * rippleEnergy * 0.34f;
        const auto microRipple = liquidNoise (norm + 0.271f, flowPhase * 1.42f, liquidEnergy + rippleEnergy, seed + 151)
                               * (0.09f + rippleEnergy * 0.08f);
        const auto surface = std::tanh ((drift + outwardRipple + microRipple) * 1.04f) / std::tanh (1.04f);
        const auto localWidth = ribbonWidth * (0.72f + 0.28f * thicknessDrift + rippleEnergy * 0.2f);
        return baseRadius + surface * warpAmount + edge * localWidth;
    };

    auto makeRibbon = [&] (float outerEdge, float innerEdge, int seed)
    {
        juce::Path ribbon;
        constexpr int points = 224;
        for (int i = 0; i <= points; ++i)
        {
            const auto norm = static_cast<float> (i) / static_cast<float> (points);
            const auto angle = norm * juce::MathConstants<float>::twoPi;
            const auto radius = radiusAt (norm, outerEdge, seed);
            const auto point = centre + juce::Point<float> (std::cos (angle), std::sin (angle)) * radius;
            if (i == 0)
                ribbon.startNewSubPath (point);
            else
                ribbon.lineTo (point);
        }

        for (int i = points; i >= 0; --i)
        {
            const auto norm = static_cast<float> (i) / static_cast<float> (points);
            const auto angle = norm * juce::MathConstants<float>::twoPi;
            const auto radius = radiusAt (norm, innerEdge, seed + 19);
            const auto point = centre + juce::Point<float> (std::cos (angle), std::sin (angle)) * radius;
            ribbon.lineTo (point);
        }
        ribbon.closeSubPath();
        return ribbon;
    };

    auto makeEdge = [&] (float edge, int seed)
    {
        juce::Path edgePath;
        constexpr int points = 224;
        for (int i = 0; i <= points; ++i)
        {
            const auto norm = static_cast<float> (i) / static_cast<float> (points);
            const auto angle = norm * juce::MathConstants<float>::twoPi;
            const auto radius = radiusAt (norm, edge, seed);
            const auto point = centre + juce::Point<float> (std::cos (angle), std::sin (angle)) * radius;
            if (i == 0)
                edgePath.startNewSubPath (point);
            else
                edgePath.lineTo (point);
        }
        edgePath.closeSubPath();
        return edgePath;
    };

    const auto broadRibbon = makeRibbon (0.64f, -0.54f, 11);
    const auto highlightRibbon = makeRibbon (0.82f, 0.5f, 37);
    const auto outerEdge = makeEdge (0.72f, 11);
    const auto innerEdge = makeEdge (-0.58f, 31);

    g.setColour (accent.withAlpha (0.045f + liquidEnergy * 0.16f + rippleEnergy * 0.09f));
    g.fillPath (broadRibbon);
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.09f + liquidEnergy * 0.16f), centre.x - glowRadius, centre.y - glowRadius,
                                             textColour.withAlpha (0.018f), centre.x + glowRadius, centre.y + glowRadius, false));
    g.fillPath (broadRibbon);
    g.setColour (juce::Colours::white.withAlpha (0.018f + liquidEnergy * 0.04f));
    g.fillPath (highlightRibbon);

    g.setColour (accent.withAlpha (0.2f + liquidEnergy * 0.34f + rippleEnergy * 0.16f));
    g.strokePath (outerEdge, juce::PathStrokeType ((2.1f + liquidEnergy * 2.2f + rippleEnergy * 1.5f) * uiScale,
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    g.setColour (textColour.withAlpha (0.03f + liquidEnergy * 0.08f));
    g.strokePath (innerEdge, juce::PathStrokeType ((0.8f + liquidEnergy * 0.45f) * uiScale,
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

    for (int i = 0; i < 36; ++i)
    {
        const auto seed = static_cast<float> (i);
        const auto norm = fractFloat (seed / 36.0f + visualMotionPhase * (0.026f + liquidEnergy * 0.064f + rippleEnergy * 0.038f));
        const auto angle = norm * juce::MathConstants<float>::twoPi;
        const auto radius = radiusAt (norm, 0.16f + liquidNoise (norm, flowPhase * 0.7f, liquidEnergy, 97) * 0.5f, 127);
        const auto point = centre + juce::Point<float> (std::cos (angle), std::sin (angle)) * radius;
        const auto particle = (0.6f + liquidEnergy * 1.7f + rippleEnergy * 2.2f + std::fmod (seed, 3.0f) * 0.18f) * uiScale;
        g.setColour (lerpColour (juce::Colours::white, accent.brighter (0.65f), smileAmount)
                         .withAlpha ((0.018f + liquidEnergy * 0.28f + rippleEnergy * 0.26f)
                                    * (0.76f + 0.24f * std::sin (seed * 2.17f + visualMotionPhase * 1.7f))));
        g.fillEllipse (point.x - particle * 0.5f, point.y - particle * 0.5f, particle, particle);
    }

    g.restoreState();
}

std::array<int, 3> SuperBassAudioProcessorEditor::getRoutingOrderForUi() const
{
    switch (static_cast<int> (processor.apvts.getRawParameterValue ("routingOrder")->load()))
    {
        case 1: return { 0, 2, 1 };
        case 2: return { 1, 0, 2 };
        case 3: return { 1, 2, 0 };
        case 4: return { 2, 0, 1 };
        case 5: return { 2, 1, 0 };
        default: return { 0, 1, 2 };
    }
}

void SuperBassAudioProcessorEditor::showScaleContextMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("UI SCALE");
    menu.addItem (1, "50%", true, std::abs (uiScale - 0.5f) < 0.02f);
    menu.addItem (2, "100%", true, std::abs (uiScale - 1.0f) < 0.02f);
    menu.addItem (3, "125%", true, std::abs (uiScale - 1.25f) < 0.02f);
    menu.addItem (4, "150%", true, std::abs (uiScale - 1.5f) < 0.02f);
    menu.addItem (5, "200%", true, std::abs (uiScale - 2.0f) < 0.02f);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [this] (int result)
                        {
                            if (result == 1) applyScalePreset (0.5f);
                            if (result == 2) applyScalePreset (1.0f);
                            if (result == 3) applyScalePreset (1.25f);
                            if (result == 4) applyScalePreset (1.5f);
                            if (result == 5) applyScalePreset (2.0f);
                        });
}

void SuperBassAudioProcessorEditor::applyScalePreset (float scale)
{
    const auto constrained = juce::jlimit (0.5f, 2.0f, scale);
    setSize (scaleInt (constrained, 1240.0f), scaleInt (constrained, 740.0f));
}

void SuperBassAudioProcessorEditor::updateScaledFonts()
{
    const auto s = juce::jlimit (0.5f, 2.0f, uiScale);
    techLookAndFeel.setUiScale (s);

    title.setFont (juce::FontOptions (30.0f * s, juce::Font::bold));
    oversamplingLabel.setFont (juce::FontOptions (juce::jmax (9.5f, 11.0f * s), juce::Font::bold));
    presetLabel.setFont (juce::FontOptions (juce::jmax (10.0f, 12.0f * s), juce::Font::bold));

    for (auto* knob : { &smile, &outputGain, &sub, &subFreq, &body, &bodyFreq, &depth, &depthDistance, &wide, &wideFreq,
                        &wideRate, &diffusorFreq, &clipper, &comp, &compThreshold, &compAttack, &compRelease,
                        &transientAttack, &transientRelease, &transientMix, &eqLow, &eqLowMid, &eqMid,
                        &eqHighMid, &eqHigh })
    {
        knob->label.setFont (juce::FontOptions (juce::jmax (10.0f, 13.0f * s), juce::Font::bold));
        knob->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, scaleInt (s, 58.0f), scaleInt (s, 20.0f));
    }

    for (auto* knob : { &eqLow, &eqLowMid, &eqMid, &eqHighMid, &eqHigh })
    {
        knob->label.setFont (juce::FontOptions (juce::jmax (8.0f, 11.2f * s), juce::Font::bold));
        knob->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, scaleInt (s, 44.0f), scaleInt (s, 18.0f));
    }

    for (auto* choice : { &presetBox })
        choice->setLookAndFeel (&techLookAndFeel);
}

void SuperBassAudioProcessorEditor::mouseUp (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        showScaleContextMenu();
}

void SuperBassAudioProcessorEditor::updateScreenVisibility()
{
    const auto main = activeScreen == 0;

    title.setVisible (true);
    presetLabel.setVisible (true);
    presetBox.setVisible (true);
    presetLoadButton.setVisible (true);
    presetSaveButton.setVisible (true);
    presetRenameButton.setVisible (true);
    presetBrowseButton.setVisible (true);
    oversamplingLabel.setVisible (true);
    oversampling1xButton.setVisible (true);
    oversampling2xButton.setVisible (true);
    oversampling4xButton.setVisible (true);
    oversampling8xButton.setVisible (true);

    meterStrip.setVisible (true);
    vuMeter.setVisible (false);
    meterInButton.setVisible (false);
    meterOutButton.setVisible (false);
    meterGrButton.setVisible (false);
    nextViewButton.setVisible (main);
    backViewButton.setVisible (! main);
    routeStrip.setVisible (! main);

    smile.slider.setVisible (main);
    smile.label.setVisible (main);
    outputGain.slider.setVisible (true);
    outputGain.label.setVisible (true);

    const auto advanced = ! main;
    openPower.button.setVisible (advanced);
    openPower.button.setInterceptsMouseClicks (advanced, advanced);
    spacePower.button.setVisible (advanced);
    spacePower.button.setInterceptsMouseClicks (advanced, advanced);
    masterPower.button.setVisible (advanced);
    masterPower.button.setInterceptsMouseClicks (advanced, advanced);
    allPassPower.button.setVisible (advanced);
    allPassPower.button.setInterceptsMouseClicks (advanced, advanced);
    openFreqLinkButton.setVisible (advanced);
    openFreqLinkButton.setInterceptsMouseClicks (advanced, advanced);
    punchButton.setVisible (advanced);
    punchButton.setInterceptsMouseClicks (advanced, advanced);
    bassHeadButton.setVisible (advanced);
    bassHeadButton.setInterceptsMouseClicks (advanced, advanced);
    boomButton.setVisible (advanced);
    boomButton.setInterceptsMouseClicks (advanced, advanced);
    haasButton.setVisible (advanced);
    haasButton.setInterceptsMouseClicks (advanced, advanced);
    flangerButton.setVisible (advanced);
    flangerButton.setInterceptsMouseClicks (advanced, advanced);
    phaserButton.setVisible (advanced);
    phaserButton.setInterceptsMouseClicks (advanced, advanced);

    for (auto* knob : { &sub, &subFreq, &body, &bodyFreq, &depth, &depthDistance, &wide, &wideFreq, &wideRate,
                        &diffusorFreq, &clipper, &comp, &compThreshold, &compAttack, &compRelease,
                        &transientAttack, &transientRelease, &transientMix, &eqLow, &eqLowMid, &eqMid,
                        &eqHighMid, &eqHigh })
    {
        knob->slider.setVisible (advanced);
        knob->slider.setInterceptsMouseClicks (advanced, advanced);
        knob->label.setVisible (advanced);
        knob->label.setInterceptsMouseClicks (false, false);
    }
}

void SuperBassAudioProcessorEditor::updateDynamicUiColours()
{
    const auto smileAmount = smoothedSmileAmount;
    const auto dawn = juce::jlimit (0.0f, 1.0f, smileAmount * 2.0f);
    const auto day = juce::jlimit (0.0f, 1.0f, smileAmount * 2.0f - 1.0f);
    const auto nightAccent = juce::Colour (91, 143, 255);
    const auto sunriseAccent = juce::Colour (255, 150, 74);
    const auto daylightAccent = juce::Colour (39, 132, 226);
    const auto dynamicAccent = lerpColour (lerpColour (nightAccent, sunriseAccent, dawn), daylightAccent, day);
    const auto dynamicText = lerpColour (juce::Colour (226, 236, 252), text, juce::jlimit (0.0f, 1.0f, smileAmount * 1.35f));
    const auto dynamicMuted = lerpColour (juce::Colour (145, 168, 205), muted, smileAmount);

    title.setColour (juce::Label::textColourId, dynamicText);
    presetLabel.setColour (juce::Label::textColourId, dynamicMuted);
    oversamplingLabel.setColour (juce::Label::textColourId, dynamicMuted);
    presetBox.setColour (juce::ComboBox::textColourId, dynamicText);
    presetBox.setColour (juce::ComboBox::backgroundColourId, lerpColour (juce::Colour (12, 22, 46), juce::Colours::white, day).withAlpha (0.22f + day * 0.5f));
    presetBox.setColour (juce::ComboBox::outlineColourId, dynamicAccent.withAlpha (0.42f));
    presetBox.setColour (juce::ComboBox::arrowColourId, dynamicAccent);
    meterStrip.setColour (meterTextColourId, dynamicText);
    meterStrip.setColour (meterMutedColourId, dynamicMuted);
    meterStrip.setColour (meterAccentColourId, dynamicAccent);
    meterStrip.setColour (meterBackgroundColourId, lerpColour (juce::Colour (14, 25, 50), juce::Colour (244, 249, 253), day).withAlpha (0.88f));

    auto updateKnobColours = [dynamicAccent, dynamicText, dynamicMuted] (Knob& knob)
    {
        knob.slider.setColour (juce::Slider::rotarySliderFillColourId, dynamicAccent);
        knob.slider.setColour (juce::Slider::textBoxTextColourId, dynamicText);
        knob.label.setColour (juce::Label::textColourId, dynamicMuted);
    };

    updateKnobColours (smile);
    updateKnobColours (outputGain);

    if (activeScreen == 1)
        for (auto* knob : { &sub, &subFreq, &body, &bodyFreq, &depth, &depthDistance, &wide, &wideFreq,
                            &wideRate, &diffusorFreq, &clipper, &comp, &compThreshold, &compAttack, &compRelease,
                            &transientAttack, &transientRelease, &transientMix, &eqLow, &eqLowMid, &eqMid,
                            &eqHighMid, &eqHigh })
            updateKnobColours (*knob);

    for (auto* button : { &presetLoadButton, &presetSaveButton, &presetRenameButton, &presetBrowseButton,
                          &nextViewButton, &backViewButton, &oversampling1xButton, &oversampling2xButton,
                          &oversampling4xButton, &oversampling8xButton, &meterInButton, &meterOutButton,
                          &meterGrButton })
    {
        button->setColour (juce::TextButton::buttonOnColourId, dynamicAccent);
        button->setColour (juce::TextButton::buttonColourId, lerpColour (juce::Colour (12, 24, 50), juce::Colour (239, 246, 252), day).withAlpha (0.86f));
        button->setColour (juce::TextButton::textColourOffId, dynamicText);
    }

    if (activeScreen == 1)
    {
        for (auto* button : { &openFreqLinkButton, &punchButton, &bassHeadButton, &boomButton, &haasButton, &flangerButton,
                              &phaserButton })
        {
            button->setColour (juce::TextButton::buttonOnColourId, dynamicAccent);
            button->setColour (juce::TextButton::buttonColourId, lerpColour (juce::Colour (14, 26, 54), juce::Colour (237, 245, 252), day).withAlpha (0.88f));
            button->setColour (juce::TextButton::textColourOffId, dynamicText);
        }

        for (auto* toggle : { &openPower.button, &spacePower.button, &masterPower.button, &allPassPower.button })
        {
            toggle->setColour (juce::ToggleButton::textColourId, dynamicText);
            toggle->setColour (juce::ToggleButton::tickColourId, dynamicAccent);
        }
    }
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
        .getChildFile ("Super Audio")
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
    const juce::StringArray factoryNames {
        "Default",
        "Punch Weight",
        "BassHead Depth",
        "Boom Thick",
        "voice of mm1",
        "voice of mm2",
        "voice of mm3",
        "voice of sg1",
        "voice of sg2",
        "voice of sg3",
        "voice of tn1",
        "voice of tn2",
        "voice of tn3",
        "Axer 808",
        juce::String::fromUTF8 ("XLee" "\xe3\x81\xae" "Line Bass")
    };
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
        const auto userPresetName = file.getFileNameWithoutExtension().trim();
        if (userPresetName == "Axer 808" || userPresetName == juce::String::fromUTF8 ("XLee" "\xe3\x81\xae" "Line Bass"))
            continue;

        presetEntries.push_back ({ userPresetName, false, false, -1, file });
        presetBox.addItem (presetEntries.back().name, static_cast<int> (presetEntries.size()));
    }

    suppressPresetChange = true;
    presetBox.setSelectedId (previousId > 0 && previousId <= static_cast<int> (presetEntries.size()) ? previousId : 2,
                             juce::dontSendNotification);
    suppressPresetChange = false;
    syncPresetBoxToProcessorState();
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
    auto setOpen = [this] (int mode, float subAmount, float subHz, float bodyAmount, float bodyHz, bool linked)
    {
        setPresetParameter ("openMode", static_cast<float> (juce::jlimit (0, 2, mode)));
        setPresetParameter ("openSub", subAmount);
        setPresetParameter ("openSubFreq", subHz);
        setPresetParameter ("openBody", bodyAmount);
        setPresetParameter ("openBodyFreq", bodyHz);
        setPresetParameter ("openFreqLink", linked ? 1.0f : 0.0f);
    };

    auto setSpace = [this] (bool enabled, float depthAmount, float distanceAmount, float wideAmount,
                            float wideHz, float wideRateValue, int wideMode, bool diffusor, float diffusorHz)
    {
        setPresetParameter ("spaceEnabled", enabled ? 1.0f : 0.0f);
        setPresetParameter ("spaceDepth", depthAmount);
        setPresetParameter ("spaceDepthDistance", distanceAmount);
        setPresetParameter ("spaceWide", wideAmount);
        setPresetParameter ("spaceWideFreq", wideHz);
        setPresetParameter ("spaceWideRate", wideRateValue);
        setPresetParameter ("spaceWideMode", static_cast<float> (juce::jlimit (0, 2, wideMode)));
        setPresetParameter ("spaceAllPassEnabled", diffusor ? 1.0f : 0.0f);
        setPresetParameter ("spaceDiffusorHz", diffusorHz);
    };

    auto setMaster = [this] (float clipperAmount, float compressionAmount, float thresholdDb,
                             float attackMs, float releaseMs, float transientAttackAmount,
                             float transientReleaseAmount, float transientMixAmount)
    {
        setPresetParameter ("masterClipper", clipperAmount);
        setPresetParameter ("masterCompression", compressionAmount);
        setPresetParameter ("masterCompThreshold", thresholdDb);
        setPresetParameter ("masterCompAttack", attackMs);
        setPresetParameter ("masterCompRelease", releaseMs);
        setPresetParameter ("masterTransientAttack", transientAttackAmount);
        setPresetParameter ("masterTransientRelease", transientReleaseAmount);
        setPresetParameter ("masterTransientMix", transientMixAmount);
    };

    auto setEq = [this] (float low, float lowMid, float mid, float highMid, float high)
    {
        setPresetParameter ("eqLow", low);
        setPresetParameter ("eqLowMid", lowMid);
        setPresetParameter ("eqMid", mid);
        setPresetParameter ("eqHighMid", highMid);
        setPresetParameter ("eqHigh", high);
    };

    auto setGlobal = [this] (float smileAmount, int oversamplingIndex, float outputDb)
    {
        setPresetParameter ("smile", smileAmount);
        setPresetParameter ("oversampling", static_cast<float> (juce::jlimit (0, 3, oversamplingIndex)));
        setPresetParameter ("output", outputDb);
    };

    setPresetParameter ("openEnabled", 1.0f);
    setPresetParameter ("masterEnabled", 1.0f);
    setPresetParameter ("routingOrder", 0.0f);
    setOpen (0, 100.0f, 61.1f, 100.0f, 93.2f, false);
    setSpace (true, 100.0f, 80.0f, 100.0f, 300.0f, 0.7f, 1, false, 200.0f);
    setMaster (0.0f, 0.0f, 0.0f, 80.0f, 30.0f, 0.0f, 0.0f, 0.0f);
    setEq (0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    setGlobal (100.0f, 0, 0.0f);

    if (factoryIndex == 1)
    {
        setOpen (0, 76.0f, 58.0f, 86.0f, 132.0f, false);
        setSpace (true, 24.0f, 34.0f, 24.0f, 180.0f, 0.42f, 0, false, 220.0f);
        setMaster (34.0f, 38.0f, -14.0f, 12.0f, 110.0f, 28.0f, 8.0f, 48.0f);
        setEq (1.2f, 3.2f, 2.0f, 0.8f, 0.9f);
        setGlobal (62.0f, 1, -1.8f);
    }
    else if (factoryIndex == 2)
    {
        setOpen (1, 100.0f, 42.0f, 58.0f, 92.0f, false);
        setSpace (true, 10.0f, 18.0f, 10.0f, 140.0f, 0.34f, 0, false, 180.0f);
        setMaster (28.0f, 46.0f, -21.0f, 18.0f, 160.0f, 14.0f, 42.0f, 52.0f);
        setEq (4.2f, 1.8f, -1.8f, 0.9f, 0.0f);
        setGlobal (78.0f, 2, -2.4f);
    }
    else if (factoryIndex == 3)
    {
        setOpen (2, 66.0f, 54.0f, 94.0f, 145.0f, false);
        setSpace (true, 20.0f, 42.0f, 32.0f, 190.0f, 0.56f, 0, false, 260.0f);
        setMaster (42.0f, 36.0f, -15.5f, 18.0f, 135.0f, 30.0f, 18.0f, 52.0f);
        setEq (1.6f, 3.8f, 1.2f, 1.5f, 1.6f);
        setGlobal (70.0f, 2, -2.6f);
    }
    else if (factoryIndex == 4)
    {
        setOpen (1, 65.0f, 60.0f, 40.0f, 60.0f, true);
        setSpace (false, 0.0f, 0.0f, 0.0f, 180.0f, 0.28f, 0, false, 200.0f);
        setMaster (15.0f, 42.0f, -18.5f, 35.0f, 180.0f, 10.0f, 25.0f, 38.0f);
        setEq (3.5f, 1.5f, -1.0f, 0.5f, 0.0f);
        setGlobal (62.0f, 2, -1.5f);
    }
    else if (factoryIndex == 5)
    {
        setOpen (2, 55.0f, 50.0f, 60.0f, 140.0f, false);
        setSpace (true, 15.0f, 10.0f, 0.0f, 180.0f, 0.32f, 0, false, 220.0f);
        setMaster (30.0f, 46.0f, -15.0f, 45.0f, 220.0f, 0.0f, 40.0f, 42.0f);
        setEq (2.0f, 4.0f, 1.0f, -1.5f, 0.5f);
        setGlobal (75.0f, 2, -2.2f);
    }
    else if (factoryIndex == 6)
    {
        setOpen (0, 50.0f, 60.0f, 50.0f, 60.0f, true);
        setSpace (true, 20.0f, 25.0f, 28.0f, 180.0f, 0.5f, 0, false, 250.0f);
        setMaster (45.0f, 52.0f, -12.0f, 15.0f, 120.0f, 35.0f, 10.0f, 52.0f);
        setEq (1.5f, 3.0f, 2.5f, 1.0f, 1.5f);
        setGlobal (55.0f, 1, -3.0f);
    }
    else if (factoryIndex == 7)
    {
        setOpen (0, 85.0f, 62.0f, 70.0f, 62.0f, true);
        setSpace (false, 0.0f, 0.0f, 0.0f, 250.0f, 0.25f, 0, false, 250.0f);
        setMaster (65.0f, 74.0f, -22.0f, 1.0f, 50.0f, 75.0f, -40.0f, 82.0f);
        setEq (-1.0f, 5.0f, -3.5f, 3.5f, 1.0f);
        setGlobal (45.0f, 3, -4.5f);
    }
    else if (factoryIndex == 8)
    {
        setOpen (1, 80.0f, 60.0f, 60.0f, 60.0f, true);
        setSpace (true, 5.0f, 12.0f, 0.0f, 120.0f, 0.32f, 0, false, 200.0f);
        setMaster (40.0f, 68.0f, -26.0f, 12.0f, 120.0f, 20.0f, 50.0f, 62.0f);
        setEq (4.5f, 2.0f, -1.5f, 1.2f, 0.0f);
        setGlobal (82.0f, 2, -2.8f);
    }
    else if (factoryIndex == 9)
    {
        setOpen (2, 40.0f, 60.0f, 75.0f, 60.0f, true);
        setSpace (true, 10.0f, 18.0f, 0.0f, 200.0f, 0.34f, 0, false, 220.0f);
        setMaster (55.0f, 58.0f, -16.5f, 4.0f, 85.0f, 45.0f, 15.0f, 58.0f);
        setEq (1.0f, 3.5f, 0.5f, 2.0f, 2.5f);
        setGlobal (68.0f, 2, -3.5f);
    }
    else if (factoryIndex == 10)
    {
        setOpen (2, 95.0f, 48.0f, 20.0f, 150.0f, false);
        setSpace (true, 45.0f, 60.0f, 72.0f, 150.0f, 0.9f, 0, false, 260.0f);
        setMaster (90.0f, 88.0f, -30.0f, 1.0f, 40.0f, 90.0f, 60.0f, 92.0f);
        setEq (5.0f, -2.0f, 4.5f, 6.0f, 5.5f);
        setGlobal (95.0f, 3, -6.0f);
    }
    else if (factoryIndex == 11)
    {
        setOpen (1, 90.0f, 60.0f, 85.0f, 60.0f, true);
        setSpace (true, 25.0f, 50.0f, 46.0f, 110.0f, 0.72f, 2, true, 420.0f);
        setMaster (75.0f, 76.0f, -24.0f, 8.0f, 70.0f, 60.0f, 80.0f, 86.0f);
        setEq (6.0f, 1.0f, -3.0f, 4.0f, 3.0f);
        setGlobal (88.0f, 3, -4.8f);
    }
    else if (factoryIndex == 12)
    {
        setOpen (0, 75.0f, 60.0f, 50.0f, 60.0f, true);
        setSpace (true, 35.0f, 54.0f, 56.0f, 160.0f, 0.78f, 0, true, 360.0f);
        setMaster (60.0f, 66.0f, -18.0f, 2.5f, 70.0f, 80.0f, 30.0f, 78.0f);
        setEq (3.0f, 4.5f, 1.0f, 2.5f, 4.0f);
        setGlobal (85.0f, 2, -4.0f);
    }
    else if (factoryIndex == 13)
    {
        setPresetParameter ("routingOrder", 1.0f);
        setOpen (0, 68.300003f, 256.0f, 49.100002f, 72.900002f, false);
        setSpace (true, 100.0f, 34.100002f, 54.600002f, 29.600000f, 0.348000f, 0, true, 80.0f);
        setMaster (100.0f, 100.0f, -7.099999f, 40.100002f, 1267.0f, 0.0f, 0.0f, 0.0f);
        setEq (0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        setGlobal (68.400002f, 1, 0.0f);
    }
    else if (factoryIndex == 14)
    {
        setPresetParameter ("routingOrder", 4.0f);
        setOpen (0, 80.300003f, 165.5f, 84.099998f, 256.0f, false);
        setSpace (true, 65.300003f, 57.600002f, 100.0f, 300.0f, 0.700000f, 2, true, 4941.200195f);
        setMaster (0.0f, 77.400002f, -48.400002f, 80.0f, 1040.0f, 0.0f, 0.0f, 0.0f);
        setEq (0.0f, 0.0f, 2.100000f, -4.900000f, 0.0f);
        setGlobal (100.0f, 2, 0.0f);
    }

    updateOpenModeButtons();
    updateOpenLinkButton();
    updateWideModeButtons();
    updateOversamplingButtons();
    updateDynamicUiColours();
}

void SuperBassAudioProcessorEditor::loadSelectedPreset()
{
    const auto index = presetBox.getSelectedId() - 1;
    if (! juce::isPositiveAndBelow (index, static_cast<int> (presetEntries.size())))
        return;

    const auto& entry = presetEntries[static_cast<size_t> (index)];
    if (entry.isHeader)
        return;

    processor.setCurrentPresetName (entry.name);
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
        {
            presetBox.setSelectedId (i + 1, juce::dontSendNotification);
            processor.setCurrentPresetName (presetEntries[static_cast<size_t> (i)].name);
        }
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

void SuperBassAudioProcessorEditor::syncPresetBoxToProcessorState()
{
    const auto presetName = processor.getCurrentPresetName();
    if (presetName.isEmpty())
        return;

    for (int i = 0; i < static_cast<int> (presetEntries.size()); ++i)
    {
        const auto& entry = presetEntries[static_cast<size_t> (i)];
        if (! entry.isHeader && entry.name == presetName)
        {
            if (presetBox.getSelectedId() != i + 1)
            {
                suppressPresetChange = true;
                presetBox.setSelectedId (i + 1, juce::dontSendNotification);
                suppressPresetChange = false;
            }
            return;
        }
    }

    suppressPresetChange = true;
    presetBox.setText (presetName, juce::dontSendNotification);
    suppressPresetChange = false;
}

void SuperBassAudioProcessorEditor::timerCallback()
{
    visualMotionPhase += 0.0155f + smileOrbitGlowEnv * 0.0065f + smileRippleEnv * 0.004f;
    if (visualMotionPhase > 10000.0f)
        visualMotionPhase -= 10000.0f;

    const auto targetSmile = getSmileNormalised();
    smoothedSmileAmount += (targetSmile - smoothedSmileAmount) * (targetSmile > smoothedSmileAmount ? 0.38f : 0.3f);
    themeSmileAmount += (smoothedSmileAmount - themeSmileAmount) * 0.18f;
    if (std::abs (targetSmile - smoothedSmileAmount) < 0.0008f)
        smoothedSmileAmount = targetSmile;
    if (std::abs (smoothedSmileAmount - themeSmileAmount) < 0.0008f)
        themeSmileAmount = smoothedSmileAmount;

    const auto inputDb = processor.getInputMeterDb();
    const auto outputDb = processor.getOutputMeterDb();
    const auto visualDb = juce::jmax (inputDb, outputDb - 3.0f);
    const auto orbitTarget = juce::jlimit (0.0f, 1.0f, (visualDb + 56.0f) / 44.0f);
    smileOrbitFastEnv += (orbitTarget - smileOrbitFastEnv) * (orbitTarget > smileOrbitFastEnv ? 0.46f : 0.11f);
    smileOrbitGlowEnv += (smileOrbitFastEnv - smileOrbitGlowEnv) * (smileOrbitFastEnv > smileOrbitGlowEnv ? 0.36f : 0.055f);
    smileOrbitEnv += (smileOrbitGlowEnv - smileOrbitEnv) * (smileOrbitGlowEnv > smileOrbitEnv ? 0.24f : 0.04f);
    smileLiquidEnv += (orbitTarget - smileLiquidEnv) * (orbitTarget > smileLiquidEnv ? 0.22f : 0.068f);
    const auto transientLift = juce::jmax (0.0f, smileOrbitFastEnv - smileLiquidEnv);
    smileRippleEnv += (transientLift - smileRippleEnv) * (transientLift > smileRippleEnv ? 0.42f : 0.074f);

    if (activeScreen == 1)
    {
        updateOpenModeButtons();
        updateOpenLinkButton();
        updateWideModeButtons();
        updateMeterButtons();
    }
    updateOversamplingButtons();
    updateDynamicUiColours();
    const auto routingChoice = static_cast<int> (processor.apvts.getRawParameterValue ("routingOrder")->load());
    auto shouldRepaintAdvanced = false;
    if (activeScreen == 1 && routingChoice != lastLayoutRoutingChoice)
    {
        lastLayoutRoutingChoice = routingChoice;
        shouldRepaintAdvanced = true;
        resized();
    }
    meterStrip.repaint();

    if (activeScreen == 0)
    {
        repaint();
    }
    else
    {
        shouldRepaintAdvanced = shouldRepaintAdvanced
                             || lastAdvancedRepaintRoutingChoice != routingChoice
                             || std::abs (themeSmileAmount - lastAdvancedRepaintSmile) > 0.0065f;
        if (shouldRepaintAdvanced)
        {
            lastAdvancedRepaintSmile = themeSmileAmount;
            lastAdvancedRepaintRoutingChoice = routingChoice;
            repaint();
        }
    }
}

void SuperBassAudioProcessorEditor::paint (juce::Graphics& g)
{
    updateScreenVisibility();
    const auto canvas = getLocalBounds();
    const auto smileAmount = smoothedSmileAmount;
    const auto dawn = juce::jlimit (0.0f, 1.0f, smileAmount * 2.0f);
    const auto day = juce::jlimit (0.0f, 1.0f, smileAmount * 2.0f - 1.0f);
    const auto dynamicAccent = lerpColour (lerpColour (juce::Colour (91, 143, 255), juce::Colour (255, 150, 74), dawn),
                                           gold, day);
    const auto dynamicText = lerpColour (juce::Colour (229, 239, 255), text, juce::jlimit (0.0f, 1.0f, smileAmount * 1.35f));
    const auto dynamicMuted = lerpColour (juce::Colour (142, 164, 205), muted, smileAmount);
    const auto loopPhase = visualMotionPhase - std::floor (visualMotionPhase);

    const auto needsThemeRefresh = ! themeCache.isValid()
                                || themeCacheWidth != canvas.getWidth()
                                || themeCacheHeight != canvas.getHeight()
                                || themeCacheScreen != activeScreen
                                || std::abs (themeSmileAmount - lastRenderedThemeSmileAmount) > 0.0025f
                                || std::abs (loopPhase - lastRenderedThemePhase) > 0.0001f;
    if (needsThemeRefresh)
    {
        if (! themeCache.isValid() || themeCacheWidth != canvas.getWidth() || themeCacheHeight != canvas.getHeight())
            themeCache = juce::Image (juce::Image::ARGB, juce::jmax (1, canvas.getWidth()), juce::jmax (1, canvas.getHeight()), true);

        juce::Graphics cacheGraphics (themeCache);
        renderThemeLayer (cacheGraphics, juce::Rectangle<int> (0, 0, themeCache.getWidth(), themeCache.getHeight()), themeSmileAmount, loopPhase);
        themeCacheWidth = canvas.getWidth();
        themeCacheHeight = canvas.getHeight();
        themeCacheScreen = activeScreen;
        lastRenderedThemeSmileAmount = themeSmileAmount;
        lastRenderedThemePhase = loopPhase;
    }

    g.setColour (background);
    g.fillAll();
    if (themeCache.isValid())
        g.drawImageAt (themeCache, canvas.getX(), canvas.getY());
    else
        renderThemeLayer (g, canvas, smileAmount, loopPhase);

    auto bounds = canvas.reduced (scaleInt (uiScale, 18.0f));
    bounds.removeFromTop (scaleInt (uiScale, 54.0f));
    auto header = bounds.removeFromTop (scaleInt (uiScale, 50.0f))
                      .withTrimmedLeft (activeScreen == 1 ? scaleInt (uiScale, 58.0f) : 0)
                      .withTrimmedRight (scaleInt (uiScale, 12.0f));
    g.setColour (dynamicAccent.withAlpha (0.22f));
    g.drawLine (static_cast<float> (header.getX()), static_cast<float> (header.getBottom()),
                static_cast<float> (header.getRight()), static_cast<float> (header.getBottom()), 1.2f);

    if (activeScreen == 0)
    {
        const auto centre = canvas.toFloat().getCentre().translated (0.0f, -8.0f * uiScale);
        const auto haloRadius = (260.0f + smileAmount * 58.0f) * uiScale;
        g.setGradientFill (juce::ColourGradient (dynamicAccent.withAlpha (0.12f + 0.04f * smileAmount), centre.x, centre.y,
                                                 juce::Colours::transparentWhite, centre.x + haloRadius, centre.y, true));
        g.fillEllipse (centre.x - haloRadius, centre.y - haloRadius, haloRadius * 2.0f, haloRadius * 2.0f);
        g.setColour (dynamicText.withAlpha (0.06f + smileAmount * 0.03f));
        g.drawEllipse (centre.x - 210.0f * uiScale, centre.y - 210.0f * uiScale,
                       420.0f * uiScale, 420.0f * uiScale, juce::jmax (0.75f, 1.0f * uiScale));
        drawSmileWaveOrbit (g, centre, dynamicAccent, dynamicText, smileAmount);

        g.setColour (dynamicAccent.withAlpha (0.7f));
        g.setFont (juce::FontOptions (13.0f * uiScale, juce::Font::bold));
        g.drawFittedText ("SMILE CORE", juce::Rectangle<int> (static_cast<int> (centre.x - 90.0f * uiScale),
                                                              static_cast<int> (centre.y + 132.0f * uiScale),
                                                              scaleInt (uiScale, 180.0f),
                                                              scaleInt (uiScale, 22.0f)),
                          juce::Justification::centred, 1);
        drawArrowButtonGlyph (g, nextViewButton.getBounds(), true, dynamicAccent);
        return;
    }

    drawArrowButtonGlyph (g, backViewButton.getBounds(), false, dynamicAccent);
    bounds.removeFromTop (scaleInt (uiScale, 14.0f));
    auto matrix = bounds.withTrimmedBottom (scaleInt (uiScale, 86.0f)).reduced (0, scaleInt (uiScale, 6.0f));
    auto routeLane = matrix.removeFromLeft (scaleInt (uiScale, 188.0f)).reduced (0, scaleInt (uiScale, 10.0f));
    drawPanel (g, routeLane, dynamicAccent, loopPhase, smileAmount);
    drawSectionTitle (g, routeLane.reduced (scaleInt (uiScale, 12.0f), scaleInt (uiScale, 10.0f)), "ROUTE", dynamicMuted);

    auto modules = matrix.reduced (scaleInt (uiScale, 12.0f), 0);
    const auto order = getRoutingOrderForUi();
    auto preferredHeightForModule = [this] (int module)
    {
        if (module == 2)
            return scaleInt (uiScale, 210.0f);
        if (module == 1)
            return scaleInt (uiScale, 176.0f);
        return scaleInt (uiScale, 158.0f);
    };

    for (int i = 0; i < 3; ++i)
    {
        const auto module = order[static_cast<size_t> (i)];
        const auto remainingSlots = 3 - i;
        const auto rowMinimum = scaleInt (uiScale, 138.0f);
        const auto minimumForRemaining = (remainingSlots - 1) * rowMinimum;
        const auto requested = preferredHeightForModule (module);
        const auto rowHeight = i == 2 ? modules.getHeight()
                                      : juce::jlimit (rowMinimum, juce::jmax (rowMinimum, modules.getHeight() - minimumForRemaining), requested);
        auto row = modules.removeFromTop (rowHeight).reduced (0, scaleInt (uiScale, 6.0f));
        const auto accent = module == 0 ? openAccent : module == 1 ? spaceAccent : masterAccent;
        drawPanel (g, row, accent, loopPhase + 0.07f * static_cast<float> (i), smileAmount);
        auto titleArea = row.reduced (scaleInt (uiScale, 12.0f), scaleInt (uiScale, 8.0f)).removeFromTop (scaleInt (uiScale, 20.0f));
        drawSectionTitle (g, titleArea, module == 0 ? "OPEN" : module == 1 ? "DIMENSION" : "MASTER", dynamicMuted);
    }
}

void SuperBassAudioProcessorEditor::resized()
{
    const auto newScale = juce::jlimit (0.5f, 2.0f, juce::jmin (static_cast<float> (getWidth()) / 1240.0f,
                                                                 static_cast<float> (getHeight()) / 740.0f));
    if (std::abs (newScale - uiScale) > 0.001f)
    {
        uiScale = newScale;
        invalidateThemeCache();
        updateScaledFonts();
    }

    auto canvas = getLocalBounds();
    auto bounds = canvas.reduced (scaleInt (uiScale, 18.0f));
    title.setBounds (bounds.removeFromTop (scaleInt (uiScale, 44.0f)));

    auto presetArea = bounds.removeFromTop (scaleInt (uiScale, 50.0f))
                          .withTrimmedLeft (activeScreen == 1 ? scaleInt (uiScale, 58.0f) : 0)
                          .withTrimmedRight (scaleInt (uiScale, 12.0f))
                          .reduced (scaleInt (uiScale, 12.0f), scaleInt (uiScale, 9.0f));
    if (activeScreen == 1)
        backViewButton.setBounds (canvas.getX() + scaleInt (uiScale, 28.0f),
                                  canvas.getY() + scaleInt (uiScale, 68.0f),
                                  scaleInt (uiScale, 44.0f),
                                  scaleInt (uiScale, 44.0f));
    presetLabel.setBounds (presetArea.removeFromLeft (scaleInt (uiScale, 62.0f)));
    presetArea.removeFromLeft (scaleInt (uiScale, 8.0f));
    presetBrowseButton.setBounds (presetArea.removeFromRight (scaleInt (uiScale, 76.0f)).reduced (scaleInt (uiScale, 3.0f), scaleInt (uiScale, 1.0f)));
    presetRenameButton.setBounds (presetArea.removeFromRight (scaleInt (uiScale, 74.0f)).reduced (scaleInt (uiScale, 3.0f), scaleInt (uiScale, 1.0f)));
    presetSaveButton.setBounds (presetArea.removeFromRight (scaleInt (uiScale, 78.0f)).reduced (scaleInt (uiScale, 3.0f), scaleInt (uiScale, 1.0f)));
    presetLoadButton.setBounds (presetArea.removeFromRight (scaleInt (uiScale, 62.0f)).reduced (scaleInt (uiScale, 3.0f), scaleInt (uiScale, 1.0f)));
    auto osArea = presetArea.removeFromRight (scaleInt (uiScale, 188.0f)).reduced (scaleInt (uiScale, 2.0f), 0);
    oversamplingLabel.setBounds (osArea.removeFromLeft (scaleInt (uiScale, 72.0f)).withTrimmedTop (scaleInt (uiScale, 2.0f)));
    const auto osButtonWidth = osArea.getWidth() / 4;
    oversampling1xButton.setBounds (osArea.removeFromLeft (osButtonWidth).reduced (scaleInt (uiScale, 2.0f)));
    oversampling2xButton.setBounds (osArea.removeFromLeft (osButtonWidth).reduced (scaleInt (uiScale, 2.0f)));
    oversampling4xButton.setBounds (osArea.removeFromLeft (osButtonWidth).reduced (scaleInt (uiScale, 2.0f)));
    oversampling8xButton.setBounds (osArea.reduced (scaleInt (uiScale, 2.0f)));
    presetArea.removeFromRight (scaleInt (uiScale, 8.0f));
    presetBox.setBounds (presetArea.reduced (scaleInt (uiScale, 2.0f), scaleInt (uiScale, 1.0f)));

    auto globalRow = bounds.removeFromBottom (scaleInt (uiScale, 86.0f)).reduced (scaleInt (uiScale, 40.0f), scaleInt (uiScale, 10.0f));
    outputGain.label.setText ("Level", juce::dontSendNotification);
    const auto levelWidth = scaleInt (uiScale, 104.0f);
    auto levelCell = globalRow.removeFromRight (levelWidth);
    outputGain.label.setBounds (levelCell.removeFromTop (scaleInt (uiScale, 20.0f)));
    outputGain.slider.setBounds (levelCell.withSizeKeepingCentre (scaleInt (uiScale, 92.0f), scaleInt (uiScale, 82.0f)));
    const auto stripWidth = juce::jmin (scaleInt (uiScale, 870.0f), juce::jmax (scaleInt (uiScale, 520.0f),
                                                                                globalRow.getWidth() - scaleInt (uiScale, 30.0f)));
    auto stripArea = juce::Rectangle<int> (stripWidth, globalRow.getHeight())
                         .withCentre ({ canvas.getCentreX(), globalRow.getCentreY() });
    stripArea = stripArea.getIntersection (globalRow);
    meterStrip.setBounds (stripArea.reduced (scaleInt (uiScale, 8.0f), scaleInt (uiScale, 7.0f)));

    if (activeScreen == 0)
    {
        const auto centre = canvas.getCentre().translated (0, -scaleInt (uiScale, 28.0f));
        const auto knobSize = scaleInt (uiScale, 368.0f);
        smile.label.setBounds (centre.x - scaleInt (uiScale, 100.0f),
                               centre.y - scaleInt (uiScale, 214.0f),
                               scaleInt (uiScale, 200.0f),
                               scaleInt (uiScale, 28.0f));
        smile.slider.setBounds (centre.x - knobSize / 2,
                                centre.y - knobSize / 2 - scaleInt (uiScale, 8.0f),
                                knobSize,
                                knobSize + scaleInt (uiScale, 48.0f));
        nextViewButton.setBounds (canvas.getRight() - scaleInt (uiScale, 92.0f),
                                  centre.y - scaleInt (uiScale, 28.0f),
                                  scaleInt (uiScale, 56.0f),
                                  scaleInt (uiScale, 56.0f));
        updateScreenVisibility();
        return;
    }

    bounds.removeFromTop (scaleInt (uiScale, 14.0f));
    auto matrix = bounds.reduced (0, scaleInt (uiScale, 6.0f));
    auto routeLane = matrix.removeFromLeft (scaleInt (uiScale, 188.0f)).reduced (0, scaleInt (uiScale, 10.0f));
    routeStrip.setBounds (routeLane.reduced (scaleInt (uiScale, 20.0f), scaleInt (uiScale, 34.0f)));

    auto modules = matrix.reduced (scaleInt (uiScale, 12.0f), 0);
    const auto order = getRoutingOrderForUi();
    std::array<juce::Rectangle<int>, 3> moduleRows;
    auto preferredHeightForModule = [this] (int module)
    {
        if (module == 2)
            return scaleInt (uiScale, 210.0f);
        if (module == 1)
            return scaleInt (uiScale, 176.0f);
        return scaleInt (uiScale, 158.0f);
    };

    for (int i = 0; i < 3; ++i)
    {
        const auto module = order[static_cast<size_t> (i)];
        const auto remainingSlots = 3 - i;
        const auto requested = preferredHeightForModule (module);
        const auto rowMinimum = scaleInt (uiScale, 138.0f);
        const auto minimumForRemaining = (remainingSlots - 1) * rowMinimum;
        const auto rowHeight = i == 2 ? modules.getHeight()
                                      : juce::jlimit (rowMinimum, juce::jmax (rowMinimum, modules.getHeight() - minimumForRemaining), requested);
        moduleRows[static_cast<size_t> (module)] = modules.removeFromTop (rowHeight)
                                                       .reduced (scaleInt (uiScale, 18.0f), scaleInt (uiScale, 12.0f));
    }

    auto contentArea = [this] (juce::Rectangle<int> row, int module)
    {
        auto area = row.reduced (scaleInt (uiScale, 4.0f), scaleInt (uiScale, 2.0f));
        auto powerColumn = area.removeFromLeft (scaleInt (uiScale, module == 2 ? 86.0f : module == 1 ? 92.0f : 84.0f));
        (void) powerColumn;
        area.removeFromTop (scaleInt (uiScale, 20.0f));
        return area.reduced (scaleInt (uiScale, 5.0f), scaleInt (uiScale, 2.0f));
    };

    auto openArea = moduleRows[0];
    const auto openIsBottom = order[2] == 0;
    const auto openScale = openIsBottom ? 0.85f : 1.0f;
    auto openPowerArea = openArea.reduced (0, scaleInt (uiScale, 2.0f)).removeFromLeft (scaleInt (uiScale, 86.0f));
    openPower.button.setBounds (openPowerArea.removeFromTop (scaleInt (uiScale, 30.0f)));
    auto openContent = contentArea (moduleRows[0], 0);
    auto openModes = openContent.removeFromRight (scaleInt (uiScale * openScale, 134.0f)).reduced (scaleInt (uiScale * openScale, 5.0f));
    const auto modeHeight = openModes.getHeight() / 3;
    punchButton.setBounds (openModes.removeFromTop (modeHeight).reduced (scaleInt (uiScale * openScale, 3.0f)));
    bassHeadButton.setBounds (openModes.removeFromTop (modeHeight).reduced (scaleInt (uiScale * openScale, 3.0f)));
    boomButton.setBounds (openModes.reduced (scaleInt (uiScale * openScale, 3.0f)));
    auto openGrid = openContent.reduced (scaleInt (uiScale * openScale, 4.0f), 0);
    const auto openControlWidth = openGrid.getWidth() / 5;
    const auto openKnobSize = scaleInt (uiScale * openScale, 88.0f);
    layoutKnob (sub, openGrid.removeFromLeft (openControlWidth).reduced (scaleInt (uiScale * openScale, 4.0f), 0), openKnobSize);
    layoutKnob (subFreq, openGrid.removeFromLeft (openControlWidth).reduced (scaleInt (uiScale * openScale, 4.0f), 0), openKnobSize);
    openFreqLinkButton.setBounds (openGrid.removeFromLeft (openControlWidth).withSizeKeepingCentre (scaleInt (uiScale * openScale, 116.0f),
                                                                                                    scaleInt (uiScale * openScale, 42.0f)));
    layoutKnob (bodyFreq, openGrid.removeFromLeft (openControlWidth).reduced (scaleInt (uiScale * openScale, 4.0f), 0), openKnobSize);
    layoutKnob (body, openGrid.reduced (scaleInt (uiScale * openScale, 4.0f), 0), openKnobSize);

    auto spaceArea = moduleRows[1];
    auto spacePowerArea = spaceArea.reduced (0, scaleInt (uiScale, 2.0f)).removeFromLeft (scaleInt (uiScale, 92.0f));
    spacePower.button.setBounds (spacePowerArea.removeFromTop (scaleInt (uiScale, 30.0f)));
    auto spaceContent = contentArea (moduleRows[1], 1);
    auto diffusion = spaceContent.removeFromRight (scaleInt (uiScale, 150.0f)).reduced (scaleInt (uiScale, 4.0f), 0);
    allPassPower.button.setBounds (diffusion.removeFromTop (scaleInt (uiScale, 30.0f)).reduced (scaleInt (uiScale, 1.0f)));
    layoutKnob (diffusorFreq, diffusion.reduced (scaleInt (uiScale, 8.0f), 0), scaleInt (uiScale, 82.0f));
    auto depthArea = spaceContent.removeFromLeft (scaleInt (uiScale, 220.0f)).reduced (scaleInt (uiScale, 3.0f), 0);
    const auto depthWidth = depthArea.getWidth() / 2;
    layoutKnob (depth, depthArea.removeFromLeft (depthWidth).reduced (scaleInt (uiScale, 3.0f), 0), scaleInt (uiScale, 84.0f));
    layoutKnob (depthDistance, depthArea.reduced (scaleInt (uiScale, 3.0f), 0), scaleInt (uiScale, 84.0f));
    auto modArea = spaceContent.reduced (scaleInt (uiScale, 4.0f), 0);
    auto tabs = modArea.removeFromTop (scaleInt (uiScale, 30.0f));
    const auto tabWidth = tabs.getWidth() / 3;
    auto tab1 = tabs.removeFromLeft (tabWidth).reduced (scaleInt (uiScale, 3.0f));
    auto tab2 = tabs.removeFromLeft (tabWidth).reduced (scaleInt (uiScale, 3.0f));
    auto tab3 = tabs.reduced (scaleInt (uiScale, 3.0f));
    haasButton.setBounds (tab1);
    flangerButton.setBounds (tab2);
    phaserButton.setBounds (tab3);
    const auto modWidth = modArea.getWidth() / 3;
    layoutKnob (wide, modArea.removeFromLeft (modWidth).reduced (scaleInt (uiScale, 4.0f), 0), scaleInt (uiScale, 84.0f));
    layoutKnob (wideFreq, modArea.removeFromLeft (modWidth).reduced (scaleInt (uiScale, 4.0f), 0), scaleInt (uiScale, 84.0f));
    layoutKnob (wideRate, modArea.reduced (scaleInt (uiScale, 4.0f), 0), scaleInt (uiScale, 84.0f));

    auto masterArea = moduleRows[2];
    const auto masterIsTop = order[0] == 2;
    const auto masterScale = masterIsTop ? 0.94f : 1.0f;
    auto masterPowerArea = masterArea.reduced (0, scaleInt (uiScale, 2.0f)).removeFromLeft (scaleInt (uiScale, 86.0f));
    masterPower.button.setBounds (masterPowerArea.removeFromTop (scaleInt (uiScale, 30.0f)));
    auto masterContent = contentArea (moduleRows[2], 2).reduced (scaleInt (uiScale, 2.0f), 0);
    const auto masterGap = scaleInt (uiScale, 6.0f);
    const auto availableMasterWidth = masterContent.getWidth();
    const auto controlWidth = juce::jmax (scaleInt (uiScale, 1.0f), availableMasterWidth - masterGap * 3);
    const auto compTargetWidth = scaleInt (uiScale, 322.0f);
    const auto eqTargetWidth = scaleInt (uiScale, 308.0f);
    const auto transientTargetWidth = scaleInt (uiScale, 246.0f);
    const auto clipTargetWidth = scaleInt (uiScale, 116.0f);
    const auto targetTotal = compTargetWidth + eqTargetWidth + transientTargetWidth + clipTargetWidth;
    const auto widthScale = juce::jmin (1.0f, static_cast<float> (controlWidth) / static_cast<float> (juce::jmax (1, targetTotal)));
    const auto compWidthTotal = juce::roundToInt (static_cast<float> (compTargetWidth) * widthScale);
    const auto eqWidthProtected = juce::roundToInt (static_cast<float> (eqTargetWidth) * widthScale);
    const auto transientWidth = juce::roundToInt (static_cast<float> (transientTargetWidth) * widthScale);
    const auto clipWidth = juce::jmax (scaleInt (uiScale, 1.0f), controlWidth - compWidthTotal - eqWidthProtected - transientWidth);

    auto compArea = masterContent.removeFromLeft (compWidthTotal).reduced (scaleInt (uiScale, 3.0f), 0);
    masterContent.removeFromLeft (masterGap);
    auto eqArea = masterContent.removeFromLeft (eqWidthProtected).reduced (scaleInt (uiScale, 2.0f), 0);
    masterContent.removeFromLeft (masterGap);
    auto transientArea = masterContent.removeFromLeft (transientWidth).reduced (scaleInt (uiScale, 3.0f), 0);
    masterContent.removeFromLeft (masterGap);
    auto clipArea = masterContent.withWidth (clipWidth).reduced (scaleInt (uiScale, 3.0f), 0);

    const auto compWidth = compArea.getWidth() / 4;
    layoutKnob (comp, compArea.removeFromLeft (compWidth).reduced (scaleInt (uiScale, 3.0f), 0),
                scaleInt (uiScale * masterScale, 86.0f));
    layoutKnob (compThreshold, compArea.removeFromLeft (compWidth).reduced (scaleInt (uiScale, 3.0f), 0),
                scaleInt (uiScale * masterScale, 86.0f));
    layoutKnob (compAttack, compArea.removeFromLeft (compWidth).reduced (scaleInt (uiScale, 3.0f), 0),
                scaleInt (uiScale * masterScale, 86.0f));
    layoutKnob (compRelease, compArea.reduced (scaleInt (uiScale, 3.0f), 0),
                scaleInt (uiScale * masterScale, 86.0f));

    const auto eqKnobMax = scaleInt (uiScale * masterScale, 68.0f);
    const int eqCellWidth = juce::jmax (scaleInt (uiScale, 42.0f), eqArea.getWidth() / 5);
    layoutKnob (eqLow, eqArea.removeFromLeft (eqCellWidth).reduced (scaleInt (uiScale, 1.0f), 0), eqKnobMax);
    layoutKnob (eqLowMid, eqArea.removeFromLeft (eqCellWidth).reduced (scaleInt (uiScale, 1.0f), 0), eqKnobMax);
    layoutKnob (eqMid, eqArea.removeFromLeft (eqCellWidth).reduced (scaleInt (uiScale, 1.0f), 0), eqKnobMax);
    layoutKnob (eqHighMid, eqArea.removeFromLeft (eqCellWidth).reduced (scaleInt (uiScale, 1.0f), 0), eqKnobMax);
    layoutKnob (eqHigh, eqArea.reduced (scaleInt (uiScale, 1.0f), 0), eqKnobMax);

    const auto transWidth = transientArea.getWidth() / 3;
    layoutKnob (transientMix, transientArea.removeFromLeft (transWidth).reduced (scaleInt (uiScale, 3.0f), 0),
                scaleInt (uiScale * masterScale, 86.0f));
    layoutKnob (transientAttack, transientArea.removeFromLeft (transWidth).reduced (scaleInt (uiScale, 3.0f), 0),
                scaleInt (uiScale * masterScale, 86.0f));
    layoutKnob (transientRelease, transientArea.reduced (scaleInt (uiScale, 3.0f), 0),
                scaleInt (uiScale * masterScale, 86.0f));
    layoutKnob (clipper, clipArea, scaleInt (uiScale * masterScale, 86.0f));
    updateScreenVisibility();
}

void SuperBassAudioProcessorEditor::layoutKnob (Knob& knob, juce::Rectangle<int> bounds)
{
    layoutKnob (knob, bounds, 0);
}

void SuperBassAudioProcessorEditor::layoutKnob (Knob& knob, juce::Rectangle<int> bounds, int maxSliderSize)
{
    if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0)
    {
        knob.label.setBounds ({});
        knob.slider.setBounds ({});
        return;
    }

    const auto labelHeight = juce::jlimit (scaleInt (uiScale, 12.0f),
                                           scaleInt (uiScale, 20.0f),
                                           juce::roundToInt (static_cast<float> (bounds.getHeight()) * 0.22f));
    knob.label.setBounds (bounds.removeFromTop (labelHeight));
    auto sliderArea = bounds.reduced (0, juce::jmax (0, scaleInt (uiScale, 1.0f)));
    if (maxSliderSize > 0)
        sliderArea = sliderArea.withSizeKeepingCentre (juce::jmin (maxSliderSize, sliderArea.getWidth()),
                                                       juce::jmin (maxSliderSize + scaleInt (uiScale, 20.0f), sliderArea.getHeight()));
    knob.slider.setBounds (sliderArea);
}

void SuperBassAudioProcessorEditor::layoutChoice (Choice& choice, juce::Rectangle<int> bounds)
{
    choice.label.setBounds (bounds.removeFromTop (scaleInt (uiScale, 18.0f)));
    choice.box.setBounds (bounds.reduced (scaleInt (uiScale, 2.0f), scaleInt (uiScale, 4.0f)));
}

SuperBassAudioProcessorEditor::MeterStrip::MeterStrip (SuperBassAudioProcessor& p)
    : processor (p)
{
}

void SuperBassAudioProcessorEditor::MeterStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (4.0f, 3.0f);
    const auto inDb = processor.getInputMeterDb();
    const auto outDb = processor.getOutputMeterDb();
    const auto grDb = processor.getGainReductionMeterDb();
    displayInDb += (inDb - displayInDb) * (inDb > displayInDb ? 0.55f : 0.08f);
    displayOutDb += (outDb - displayOutDb) * (outDb > displayOutDb ? 0.55f : 0.08f);
    displayGrDb += (grDb - displayGrDb) * (grDb > displayGrDb ? 0.4f : 0.1f);

    const auto themeText = findColour (meterTextColourId, true);
    const auto themeMuted = findColour (meterMutedColourId, true);
    const auto themeAccent = findColour (meterAccentColourId, true);
    const auto themeBackground = findColour (meterBackgroundColourId, true);

    auto drawMeter = [&g, themeText, themeMuted, themeBackground] (juce::Rectangle<float> area,
                                                                   const juce::String& label,
                                                                   float db,
                                                                   juce::Colour accent)
    {
        const auto clipped = db >= -0.05f;
        g.setColour (shadow.withAlpha (0.12f));
        g.fillRoundedRectangle (area.translated (0.0f, 1.5f), 7.0f);
        g.setGradientFill (juce::ColourGradient (themeBackground.brighter (0.08f), area.getX(), area.getY(),
                                                 themeBackground.darker (0.05f), area.getX(), area.getBottom(), false));
        g.fillRoundedRectangle (area, 7.0f);
        g.setColour ((clipped ? juce::Colours::red : panelOutline).withAlpha (0.85f));
        g.drawRoundedRectangle (area.reduced (0.5f), 7.0f, clipped ? 1.6f : 1.0f);

        auto labelArea = area.removeFromLeft (46.0f).reduced (4.0f, 0.0f);
        g.setColour (clipped ? juce::Colours::red : themeMuted);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawFittedText (label, labelArea.toNearestInt(), juce::Justification::centred, 1);

        auto valueArea = area.removeFromRight (72.0f).reduced (4.0f, 0.0f);
        g.setColour (clipped ? juce::Colours::red : themeText);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawFittedText (juce::String (db, 1) + " dB", valueArea.toNearestInt(), juce::Justification::centredRight, 1);

        const auto norm = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
        auto track = area.reduced (4.0f, 10.0f);
        g.setColour (themeMuted.withAlpha (0.26f));
        g.fillRoundedRectangle (track, track.getHeight() * 0.5f);
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.5f), track.getX(), track.getY(),
                                                 accent.brighter (0.55f), track.getRight(), track.getY(), false));
        g.fillRoundedRectangle (track.withWidth (track.getWidth() * norm), track.getHeight() * 0.5f);
    };

    const auto half = r.getWidth() * 0.5f;
    drawMeter (r.removeFromLeft (half).reduced (3.0f, 0.0f), "IN", displayInDb, themeAccent);
    drawMeter (r.reduced (3.0f, 0.0f), "OUT", displayOutDb, displayOutDb >= -0.05f ? juce::Colours::red : themeAccent.brighter (0.28f));
    (void) displayGrDb;
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
    const char* names[] { "OPEN", "DIMENSION", "MASTER" };
    const juce::Colour accents[] { openAccent, spaceAccent, masterAccent };
    const auto smileAmount = processor.apvts.getRawParameterValue ("smile") != nullptr
        ? juce::jlimit (0.0f, 1.0f, processor.apvts.getRawParameterValue ("smile")->load() / 100.0f)
        : 1.0f;
    const auto dawn = juce::jlimit (0.0f, 1.0f, smileAmount * 2.0f);
    const auto day = juce::jlimit (0.0f, 1.0f, smileAmount * 2.0f - 1.0f);
    const auto dynamicAccent = lerpColour (lerpColour (juce::Colour (91, 143, 255), juce::Colour (255, 150, 74), dawn),
                                           gold, day);
    const auto dynamicMuted = lerpColour (juce::Colour (142, 164, 205), muted, smileAmount);
    const auto panelFill = lerpColour (lerpColour (juce::Colour (12, 22, 46), juce::Colour (45, 56, 78), dawn),
                                       juce::Colours::white, day);
    const auto slotFill = lerpColour (lerpColour (juce::Colour (18, 31, 61), juce::Colour (68, 76, 92), dawn),
                                      juce::Colour (240, 247, 253), day);
    const auto slotText = lerpColour (juce::Colour (231, 239, 255), text, day);

    const auto r = getLocalBounds().toFloat();
    g.setColour (panelFill.withAlpha (0.2f + day * 0.28f));
    g.fillRoundedRectangle (r.reduced (2.0f), 10.0f);
    g.setGradientFill (juce::ColourGradient (dynamicAccent.withAlpha (0.08f + 0.04f * smileAmount), r.getX(), r.getY(),
                                             juce::Colours::transparentWhite, r.getRight(), r.getBottom(), false));
    g.fillRoundedRectangle (r.reduced (3.0f), 9.0f);
    g.setColour (lerpColour (dynamicAccent.withAlpha (0.54f), panelOutline.withAlpha (0.5f), day));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    g.setColour (dynamicMuted.withAlpha (0.86f));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("TOP", getLocalBounds().removeFromTop (18), juce::Justification::centred);

    for (int i = 0; i < 3; ++i)
    {
        auto slot = slotBounds (i);
        const auto active = i == dragging;
        const auto module = order[static_cast<size_t> (i)];
        const auto accent = lerpColour (dynamicAccent, accents[module], 0.48f + 0.25f * day);
        const auto slotFloat = slot.toFloat();
        g.setColour (shadow.withAlpha (0.07f + 0.08f * day));
        g.fillRoundedRectangle (slotFloat.translated (0.0f, 1.5f), 7.0f);
        g.setGradientFill (juce::ColourGradient ((active ? accent.withAlpha (0.28f) : slotFill.withAlpha (0.95f)).brighter (0.05f),
                                                 slotFloat.getX(), slotFloat.getY(),
                                                 (active ? accent.withAlpha (0.16f) : slotFill.withAlpha (0.9f)).darker (0.05f),
                                                 slotFloat.getX(), slotFloat.getBottom(), false));
        g.fillRoundedRectangle (slotFloat, 7.0f);
        g.setColour (active ? accent : lerpColour (accent.withAlpha (0.5f), panelOutline.withAlpha (0.72f), day));
        g.drawRoundedRectangle (slotFloat, 7.0f, 1.4f);
        g.setColour (accent.withAlpha (0.28f + 0.1f * (1.0f - day)));
        g.fillRoundedRectangle (slot.removeFromLeft (6).toFloat(), 4.0f);
        g.setColour (active ? lerpColour (juce::Colours::white, accent.darker (0.25f), day) : slotText);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawFittedText (names[module], slot.expanded (-2, 0), juce::Justification::centred, 1, 0.72f);

        if (i < 2)
        {
            auto from = slotBounds (i).getBottom() + 5;
            auto x = getWidth() / 2;
            auto to = slotBounds (i + 1).getY() - 5;
            g.setColour (dynamicAccent.withAlpha (0.42f + 0.1f * smileAmount));
            g.drawLine (static_cast<float> (x), static_cast<float> (from), static_cast<float> (x), static_cast<float> (to), 2.0f);
            juce::Path arrow;
            arrow.addTriangle (static_cast<float> (x), static_cast<float> (to),
                               static_cast<float> (x - 5), static_cast<float> (to - 8),
                               static_cast<float> (x + 5), static_cast<float> (to - 8));
            g.fillPath (arrow);
        }
    }

    g.setColour (dynamicMuted.withAlpha (0.86f));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("BOTTOM", getLocalBounds().removeFromBottom (18), juce::Justification::centred);
}

void SuperBassAudioProcessorEditor::RouteStrip::mouseDown (const juce::MouseEvent& e)
{
    dragging = slotForPosition (e.y);
    repaint();
}

void SuperBassAudioProcessorEditor::RouteStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;

    const auto target = slotForPosition (e.y);
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
        if (slotBounds (i).contains (getWidth() / 2, x))
            return i;

    return juce::jlimit (0, 2, x * 3 / juce::jmax (1, getHeight()));
}

juce::Rectangle<int> SuperBassAudioProcessorEditor::RouteStrip::slotBounds (int index) const
{
    const auto localScale = juce::jlimit (0.5f, 2.0f, static_cast<float> (getWidth()) / 128.0f);
    const auto gap = juce::roundToInt (16.0f * localScale);
    auto area = getLocalBounds().reduced (juce::roundToInt (14.0f * localScale), juce::roundToInt (28.0f * localScale));
    const auto slotHeight = (area.getHeight() - gap * 2) / 3;
    return { area.getX(), area.getY() + index * (slotHeight + gap), area.getWidth(), slotHeight };
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
