#include "PluginEditor.h"

namespace
{
const juce::Colour background { 18, 18, 20 };
const juce::Colour panel { 34, 33, 31 };
const juce::Colour panelOutline { 78, 73, 65 };
const juce::Colour gold { 214, 168, 88 };
const juce::Colour text { 230, 226, 216 };
const juce::Colour muted { 158, 151, 139 };
const juce::Colour openAccent { 222, 154, 79 };
const juce::Colour spaceAccent { 104, 164, 178 };
const juce::Colour masterAccent { 178, 137, 212 };

void drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& name)
{
    g.setColour (muted);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawFittedText (name, area.removeFromTop (18), juce::Justification::centred, 1);
}

void drawPanel (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour accent)
{
    g.setColour (panel);
    g.fillRoundedRectangle (area.toFloat(), 8.0f);
    g.setColour (panelOutline);
    g.drawRoundedRectangle (area.toFloat(), 8.0f, 1.0f);
    g.setColour (accent.withAlpha (0.55f));
    g.drawRoundedRectangle (area.toFloat().reduced (1.5f), 7.0f, 1.4f);
    g.setColour (accent.withAlpha (0.2f));
    g.fillRoundedRectangle (area.removeFromTop (5).toFloat(), 4.0f);
}

void drawSubGroup (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour colour)
{
    g.setColour (colour.withAlpha (0.22f));
    g.fillRoundedRectangle (area.toFloat(), 7.0f);
    g.setColour (colour.withAlpha (0.38f));
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 7.0f, 1.0f);
}
}

SuperBassAudioProcessorEditor::SuperBassAudioProcessorEditor (SuperBassAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), routeStrip (p), vuMeter (p)
{
    setSize (1240, 680);

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
    setupKnob (eqLow, "60", "eqLow");
    setupKnob (eqLowMid, "160", "eqLowMid");
    setupKnob (eqMid, "500", "eqMid");
    setupKnob (eqHighMid, "2k", "eqHighMid");
    setupKnob (eqHigh, "8k", "eqHigh");

    setupToggle (openPower, "Open", "openEnabled");
    setupToggle (spacePower, "Space", "spaceEnabled");
    setupToggle (masterPower, "Master", "masterEnabled");
    setupToggle (allPassPower, "Diffusor", "spaceAllPassEnabled");

    setupOpenModeButton (punchButton, "PUNCH", 0);
    setupOpenModeButton (bassHeadButton, "BASSHEAD", 1);
    setupOpenModeButton (boomButton, "BOOM", 2);
    setupModeButton (haasButton, "HASS", 0);
    setupModeButton (flangerButton, "FLANGER", 1);
    setupModeButton (phaserButton, "PHASER", 2);
    setupMeterButton (meterInButton, "IN", 0);
    setupMeterButton (meterOutButton, "OUT", 1);
    setupMeterButton (meterGrButton, "GR", 2);
    addAndMakeVisible (vuMeter);
    addAndMakeVisible (routeStrip);
    updateOpenModeButtons();
    updateWideModeButtons();
    updateMeterButtons();
    startTimerHz (48);
}

void SuperBassAudioProcessorEditor::setupKnob (Knob& knob, const juce::String& labelText, const juce::String& paramId)
{
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
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

    choice.box.setColour (juce::ComboBox::backgroundColourId, juce::Colour (27, 27, 28));
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
    button.setColour (juce::TextButton::buttonColourId, juce::Colour (25, 32, 34));
    button.setColour (juce::TextButton::buttonOnColourId, spaceAccent);
    button.setColour (juce::TextButton::textColourOffId, text);
    button.setColour (juce::TextButton::textColourOnId, background);
    button.onClick = [this, mode] { setWideMode (mode); };
    addAndMakeVisible (button);
}

void SuperBassAudioProcessorEditor::setupOpenModeButton (juce::TextButton& button, const juce::String& buttonText, int mode)
{
    button.setButtonText (buttonText);
    button.setClickingTogglesState (false);
    button.setColour (juce::TextButton::buttonColourId, juce::Colour (35, 29, 24));
    button.setColour (juce::TextButton::buttonOnColourId, openAccent);
    button.setColour (juce::TextButton::textColourOffId, text);
    button.setColour (juce::TextButton::textColourOnId, background);
    button.onClick = [this, mode] { setOpenMode (mode); };
    addAndMakeVisible (button);
}

void SuperBassAudioProcessorEditor::setupMeterButton (juce::TextButton& button, const juce::String& buttonText, int mode)
{
    button.setButtonText (buttonText);
    button.setClickingTogglesState (false);
    button.setColour (juce::TextButton::buttonColourId, juce::Colour (29, 29, 30));
    button.setColour (juce::TextButton::buttonOnColourId, gold);
    button.setColour (juce::TextButton::textColourOffId, text);
    button.setColour (juce::TextButton::textColourOnId, background);
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

void SuperBassAudioProcessorEditor::timerCallback()
{
    updateOpenModeButtons();
    updateWideModeButtons();
    updateMeterButtons();
    vuMeter.repaint();
    routeStrip.repaint();
}

void SuperBassAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    auto bounds = getLocalBounds().reduced (18);
    bounds.removeFromTop (54);
    bounds.removeFromTop (74);

    auto smileArea = bounds.removeFromLeft (198);
    g.setColour (juce::Colour (25, 24, 23));
    g.fillRoundedRectangle (smileArea.toFloat(), 8.0f);
    g.setColour (panelOutline);
    g.drawRoundedRectangle (smileArea.toFloat(), 8.0f, 1.0f);

    auto modules = bounds.reduced (12, 0);
    auto open = modules.removeFromLeft (modules.getWidth() / 3).reduced (6);
    auto space = modules.removeFromLeft (modules.getWidth() / 2).reduced (6);
    auto master = modules.reduced (6);

    drawPanel (g, open, openAccent);
    drawPanel (g, space, spaceAccent);
    drawPanel (g, master, masterAccent);

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

    auto content = bounds.removeFromTop (570);
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
    smileArea.removeFromTop (4);
    layoutKnob (outputGain, smileArea.removeFromTop (128));

    routeStrip.setBounds (content.removeFromTop (70).reduced (12, 6));

    auto modules = content.reduced (12, 0);
    auto openArea = modules.removeFromLeft (modules.getWidth() / 3).reduced (16);
    auto spaceArea = modules.removeFromLeft (modules.getWidth() / 2).reduced (16);
    auto masterArea = modules.reduced (16);

    openArea.removeFromTop (18);
    openPower.button.setBounds (openArea.removeFromTop (28));
    auto openKnobs = openArea.removeFromTop (174);
    layoutKnob (sub, openKnobs.removeFromLeft (openKnobs.getWidth() / 2).reduced (5));
    layoutKnob (subFreq, openKnobs.reduced (5));
    auto bodyKnobs = openArea.removeFromTop (174);
    layoutKnob (body, bodyKnobs.removeFromLeft (bodyKnobs.getWidth() / 2).reduced (5));
    layoutKnob (bodyFreq, bodyKnobs.reduced (5));
    auto openButtons = openArea.removeFromTop (50).reduced (2, 7);
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

    auto eqArea = masterArea.removeFromTop (122);
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
    const auto face = juce::Colour (218, 200, 162);
    const auto ink = juce::Colour (61, 47, 35);

    g.setColour (juce::Colour (18, 17, 16));
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (panelOutline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 7.0f, 1.0f);

    const auto rawDb = mode == 0 ? processor.getInputMeterDb()
                     : mode == 1 ? processor.getOutputMeterDb()
                                 : processor.getGainReductionMeterDb();
    const auto attack = 0.55f;
    const auto release = mode == 2 ? 0.096f : 0.076f;
    displayDb += (rawDb - displayDb) * (rawDb > displayDb ? attack : release);

    auto meter = getLocalBounds().reduced (10, 8);
    g.setGradientFill (juce::ColourGradient (face.brighter (0.06f), 0.0f, static_cast<float> (meter.getY()),
                                             face.darker (0.16f), 0.0f, static_cast<float> (meter.getBottom()), false));
    g.fillRoundedRectangle (meter.toFloat(), 6.0f);
    g.setColour (juce::Colour (112, 89, 56).withAlpha (0.65f));
    g.drawRoundedRectangle (meter.toFloat().reduced (0.5f), 6.0f, 1.0f);

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

    g.setColour (ink.withAlpha (0.56f));
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.strokePath (arc, juce::PathStrokeType (1.3f));

    const std::array<float, 7> tickValues = mode == 2
        ? std::array<float, 7> { 0.0f, 0.05f, 0.15f, 0.3f, 0.5f, 0.75f, 1.0f }
        : std::array<float, 7> { 0.0f, 0.19f, 0.38f, 0.58f, 0.77f, 0.88f, 1.0f };

    for (auto tick : tickValues)
    {
        const auto angle = startAngle + (endAngle - startAngle) * tick;
        const auto important = tick >= 0.77f || tick <= 0.001f;
        const auto inner = centre + juce::Point<float> (std::cos (angle), std::sin (angle)) * (radius - (important ? 11.0f : 8.0f));
        const auto outer = centre + juce::Point<float> (std::cos (angle), std::sin (angle)) * (radius - 1.5f);
        g.setColour ((important ? ink : ink.withAlpha (0.62f)));
        g.drawLine (inner.x, inner.y, outer.x, outer.y, important ? 1.8f : 1.1f);
    }

    g.setFont (juce::FontOptions (9.4f, juce::Font::bold));
    g.setColour (ink.withAlpha (0.78f));
    g.drawText (mode == 2 ? "-20" : "-20", getLocalBounds().reduced (17, 18), juce::Justification::bottomLeft);
    g.drawText (mode == 2 ? "0" : "+6", getLocalBounds().reduced (17, 18), juce::Justification::bottomRight);

    if (mode != 2)
    {
        const auto markY = getLocalBounds().getY() + 39;
        g.setFont (juce::FontOptions (10.8f, juce::Font::bold));
        g.setColour (juce::Colour (132, 35, 24).withAlpha (0.9f));
        g.drawText ("0", getLocalBounds().withTrimmedLeft (98).withTrimmedRight (47).withY (markY).withHeight (14),
                    juce::Justification::centred);
        g.drawText ("+3", getLocalBounds().withTrimmedLeft (126).withTrimmedRight (16).withY (markY).withHeight (14),
                    juce::Justification::centred);
    }

    const auto needleAngle = startAngle + (endAngle - startAngle) * norm;
    const auto needleEnd = centre + juce::Point<float> (std::cos (needleAngle), std::sin (needleAngle)) * (radius - 11.0f);
    g.setColour (juce::Colour (142, 35, 24));
    g.drawLine (centre.x, centre.y, needleEnd.x, needleEnd.y, 2.0f);
    g.setColour (ink);
    g.fillEllipse (centre.x - 4.0f, centre.y - 4.0f, 8.0f, 8.0f);

    auto valueText = mode == 2
        ? juce::String (displayDb, 1) + " dB"
        : juce::String (vuValue, 1) + " VU";
    g.setColour (text);
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

    g.setColour (juce::Colour (22, 22, 23));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
    g.setColour (panelOutline);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);

    for (int i = 0; i < 3; ++i)
    {
        auto slot = slotBounds (i);
        g.setColour (i == dragging ? gold.withAlpha (0.36f) : juce::Colour (37, 36, 34));
        g.fillRoundedRectangle (slot.toFloat(), 7.0f);
        g.setColour (i == dragging ? gold : panelOutline);
        g.drawRoundedRectangle (slot.toFloat(), 7.0f, 1.4f);
        g.setColour (i == dragging ? background : text);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawFittedText (names[order[static_cast<size_t> (i)]], slot, juce::Justification::centred, 1);

        if (i < 2)
        {
            auto from = slot.getRight() + 8;
            auto y = slot.getCentreY();
            auto to = slotBounds (i + 1).getX() - 8;
            g.setColour (gold.withAlpha (0.82f));
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
