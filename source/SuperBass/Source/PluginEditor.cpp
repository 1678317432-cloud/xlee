#include "PluginEditor.h"

namespace
{
const juce::Colour background { 18, 18, 20 };
const juce::Colour panel { 34, 33, 31 };
const juce::Colour panelOutline { 78, 73, 65 };
const juce::Colour gold { 214, 168, 88 };
const juce::Colour text { 230, 226, 216 };
const juce::Colour muted { 158, 151, 139 };
}

SuperBassAudioProcessorEditor::SuperBassAudioProcessorEditor (SuperBassAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), routeStrip (p)
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
    setupKnob (wide, "Wide", "spaceWide");
    setupKnob (wideFreq, "Wide Hz", "spaceWideFreq");
    setupKnob (clipper, "Clipper", "masterClipper");
    setupKnob (comp, "Comp", "masterCompression");
    setupKnob (compAttack, "C Atk", "masterCompAttack");
    setupKnob (compRelease, "C Rel", "masterCompRelease");
    setupKnob (transient, "Trans", "masterTransient");
    setupKnob (transientAttack, "T Atk", "masterTransientAttack");
    setupKnob (transientRelease, "T Rel", "masterTransientRelease");
    setupKnob (eqLow, "60", "eqLow");
    setupKnob (eqLowMid, "160", "eqLowMid");
    setupKnob (eqMid, "500", "eqMid");
    setupKnob (eqHighMid, "2k", "eqHighMid");
    setupKnob (eqHigh, "8k", "eqHigh");

    setupToggle (openPower, "Open", "openEnabled");
    setupToggle (spacePower, "Space", "spaceEnabled");
    setupToggle (masterPower, "Master", "masterEnabled");
    setupToggle (autoGain, "Auto Gain", "masterAutoGain");

    setupChoice (depthPosition, "Depth", "spaceDepthPosition");
    setupChoice (color, "Color", "colorMode");

    setupModeButton (haasButton, "HASS", 0);
    setupModeButton (flangerButton, "FLANGER", 1);
    setupModeButton (phaserButton, "PHASER", 2);
    addAndMakeVisible (routeStrip);
    updateWideModeButtons();
    startTimerHz (12);
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
    button.setColour (juce::TextButton::buttonColourId, juce::Colour (29, 29, 30));
    button.setColour (juce::TextButton::buttonOnColourId, gold);
    button.setColour (juce::TextButton::textColourOffId, text);
    button.setColour (juce::TextButton::textColourOnId, background);
    button.onClick = [this, mode] { setWideMode (mode); };
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

void SuperBassAudioProcessorEditor::timerCallback()
{
    updateWideModeButtons();
    routeStrip.repaint();
}

void SuperBassAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    auto bounds = getLocalBounds().reduced (18);
    bounds.removeFromTop (54);
    bounds.removeFromTop (74);

    auto smileArea = bounds.removeFromLeft (190);
    g.setColour (juce::Colour (25, 24, 23));
    g.fillRoundedRectangle (smileArea.toFloat(), 8.0f);
    g.setColour (panelOutline);
    g.drawRoundedRectangle (smileArea.toFloat(), 8.0f, 1.0f);

    auto modules = bounds.reduced (12, 0);
    auto open = modules.removeFromLeft (modules.getWidth() / 3).reduced (6);
    auto space = modules.removeFromLeft (modules.getWidth() / 2).reduced (6);
    auto master = modules.reduced (6);

    for (auto area : { open, space, master })
    {
        g.setColour (panel);
        g.fillRoundedRectangle (area.toFloat(), 8.0f);
        g.setColour (panelOutline);
        g.drawRoundedRectangle (area.toFloat(), 8.0f, 1.0f);
    }

}

void SuperBassAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (18);
    title.setBounds (bounds.removeFromTop (44));

    auto content = bounds.removeFromTop (570);
    auto smileArea = content.removeFromLeft (210).reduced (18);
    layoutKnob (smile, smileArea.removeFromTop (180));
    layoutChoice (color, smileArea.removeFromTop (62));
    layoutKnob (outputGain, smileArea.removeFromTop (150));

    routeStrip.setBounds (content.removeFromTop (70).reduced (12, 6));

    auto modules = content.reduced (12, 0);
    auto openArea = modules.removeFromLeft (modules.getWidth() / 3).reduced (16);
    auto spaceArea = modules.removeFromLeft (modules.getWidth() / 2).reduced (16);
    auto masterArea = modules.reduced (16);

    openPower.button.setBounds (openArea.removeFromTop (28));
    auto openKnobs = openArea.removeFromTop (180);
    layoutKnob (sub, openKnobs.removeFromLeft (openKnobs.getWidth() / 2).reduced (5));
    layoutKnob (subFreq, openKnobs.reduced (5));
    auto bodyKnobs = openArea.removeFromTop (180);
    layoutKnob (body, bodyKnobs.removeFromLeft (bodyKnobs.getWidth() / 2).reduced (5));
    layoutKnob (bodyFreq, bodyKnobs.reduced (5));

    spacePower.button.setBounds (spaceArea.removeFromTop (28));
    auto spaceKnobs = spaceArea.removeFromTop (170);
    layoutKnob (depth, spaceKnobs.removeFromLeft (spaceKnobs.getWidth() / 2).reduced (5));
    layoutKnob (wide, spaceKnobs.reduced (5));
    auto wideKnobs = spaceArea.removeFromTop (142);
    layoutKnob (wideFreq, wideKnobs.reduced (5));
    layoutChoice (depthPosition, spaceArea.removeFromTop (58));
    auto buttons = spaceArea.removeFromTop (44).reduced (2, 4);
    const auto buttonWidth = buttons.getWidth() / 3;
    haasButton.setBounds (buttons.removeFromLeft (buttonWidth).reduced (3));
    flangerButton.setBounds (buttons.removeFromLeft (buttonWidth).reduced (3));
    phaserButton.setBounds (buttons.reduced (3));

    masterPower.button.setBounds (masterArea.removeFromTop (28));
    auto masterTop = masterArea.removeFromTop (124);
    layoutKnob (clipper, masterTop.removeFromLeft (masterTop.getWidth() / 3).reduced (4));
    layoutKnob (comp, masterTop.removeFromLeft (masterTop.getWidth() / 2).reduced (4));
    auto autoArea = masterTop.reduced (4);
    autoGain.button.setBounds (autoArea.removeFromTop (34));

    auto compArea = masterArea.removeFromTop (126);
    layoutKnob (compAttack, compArea.removeFromLeft (compArea.getWidth() / 2).reduced (4));
    layoutKnob (compRelease, compArea.reduced (4));

    auto transArea = masterArea.removeFromTop (144);
    layoutKnob (transient, transArea.removeFromLeft (transArea.getWidth() / 3).reduced (4));
    layoutKnob (transientAttack, transArea.removeFromLeft (transArea.getWidth() / 2).reduced (4));
    layoutKnob (transientRelease, transArea.reduced (4));

    auto eqArea = masterArea.removeFromTop (106);
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
