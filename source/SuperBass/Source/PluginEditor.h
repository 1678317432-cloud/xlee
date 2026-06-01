#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class SuperBassAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit SuperBassAudioProcessorEditor (SuperBassAudioProcessor&);
    ~SuperBassAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<ButtonAttachment> attachment;
    };

    struct Choice
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<ComboBoxAttachment> attachment;
    };

    void setupKnob (Knob& knob, const juce::String& label, const juce::String& paramId);
    void setupToggle (Toggle& toggle, const juce::String& label, const juce::String& paramId);
    void setupChoice (Choice& choice, const juce::String& label, const juce::String& paramId);
    void layoutKnob (Knob& knob, juce::Rectangle<int> bounds);
    void layoutChoice (Choice& choice, juce::Rectangle<int> bounds);
    void setupModeButton (juce::TextButton& button, const juce::String& text, int mode);
    void setupOpenModeButton (juce::TextButton& button, const juce::String& text, int mode);
    void setupMeterButton (juce::TextButton& button, const juce::String& text, int mode);
    void setOpenMode (int mode);
    void updateOpenModeButtons();
    void setWideMode (int mode);
    void updateWideModeButtons();
    void setMeterMode (int mode);
    void updateMeterButtons();
    void timerCallback() override;

    class VuMeter final : public juce::Component
    {
    public:
        explicit VuMeter (SuperBassAudioProcessor&);
        void setMode (int newMode);
        void paint (juce::Graphics&) override;

    private:
        SuperBassAudioProcessor& processor;
        int mode = 1;
        float displayDb = -60.0f;
    };

    class RouteStrip final : public juce::Component
    {
    public:
        explicit RouteStrip (SuperBassAudioProcessor&);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

    private:
        void syncFromParameter();
        void publishOrder();
        int slotForPosition (int x) const;
        juce::Rectangle<int> slotBounds (int index) const;
        static int orderToChoice (const std::array<int, 3>& order);

        SuperBassAudioProcessor& processor;
        std::array<int, 3> order { 0, 1, 2 };
        int dragging = -1;
    };

    SuperBassAudioProcessor& processor;
    RouteStrip routeStrip;

    Knob smile;
    Knob outputGain;
    Knob sub;
    Knob subFreq;
    Knob body;
    Knob bodyFreq;
    Knob depth;
    Knob depthDistance;
    Knob wide;
    Knob wideFreq;
    Knob wideRate;
    Knob diffusorFreq;
    Knob clipper;
    Knob comp;
    Knob compThreshold;
    Knob compAttack;
    Knob compRelease;
    Knob transientAttack;
    Knob transientRelease;
    Knob transientMix;
    Knob eqLow;
    Knob eqLowMid;
    Knob eqMid;
    Knob eqHighMid;
    Knob eqHigh;

    Toggle openPower;
    Toggle spacePower;
    Toggle masterPower;
    Toggle allPassPower;

    juce::TextButton punchButton;
    juce::TextButton bassHeadButton;
    juce::TextButton boomButton;
    juce::TextButton haasButton;
    juce::TextButton flangerButton;
    juce::TextButton phaserButton;
    juce::TextButton meterInButton;
    juce::TextButton meterOutButton;
    juce::TextButton meterGrButton;

    VuMeter vuMeter;

    juce::Label title;
    int meterMode = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuperBassAudioProcessorEditor)
};
