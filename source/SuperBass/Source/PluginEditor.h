#pragma once

#include <vector>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class SuperBassAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit SuperBassAudioProcessorEditor (SuperBassAudioProcessor&);
    ~SuperBassAudioProcessorEditor() override;

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
    void setupOversamplingButton (juce::TextButton& button, const juce::String& text, int mode);
    void setupMeterButton (juce::TextButton& button, const juce::String& text, int mode);
    void setOpenMode (int mode);
    void updateOpenModeButtons();
    void updateOpenLinkButton();
    void setOpenLinkEnabled (bool shouldBeEnabled);
    void syncLinkedOpenFrequency (const juce::String& changedParamId);
    void setWideMode (int mode);
    void updateWideModeButtons();
    void setOversamplingMode (int mode);
    void updateOversamplingButtons();
    void setMeterMode (int mode);
    void updateMeterButtons();
    void setupPresetControls();
    void refreshPresetList();
    void loadSelectedPreset();
    void savePreset();
    void renamePreset();
    void browsePresets();
    void applyFactoryPreset (int factoryIndex);
    void setPresetParameter (const juce::String& paramId, float value);
    void writePresetFile (const juce::File& file);
    void loadPresetFile (const juce::File& file);
    juce::File getPresetDirectory() const;
    void timerCallback() override;

    struct PresetEntry
    {
        juce::String name;
        bool isFactory = false;
        bool isHeader = false;
        int factoryIndex = -1;
        juce::File file;
    };

    class TechLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                               juce::Slider&) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                                   bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
        void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;
        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH,
                           juce::ComboBox&) override;
    };

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
    TechLookAndFeel techLookAndFeel;
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

    juce::TextButton openFreqLinkButton;
    std::unique_ptr<ButtonAttachment> openFreqLinkAttachment;
    juce::TextButton punchButton;
    juce::TextButton bassHeadButton;
    juce::TextButton boomButton;
    juce::TextButton haasButton;
    juce::TextButton flangerButton;
    juce::TextButton phaserButton;
    juce::TextButton meterInButton;
    juce::TextButton meterOutButton;
    juce::TextButton meterGrButton;
    juce::TextButton oversampling1xButton;
    juce::TextButton oversampling2xButton;
    juce::TextButton oversampling4xButton;
    juce::TextButton oversampling8xButton;
    juce::Label oversamplingLabel;
    juce::Label presetLabel;
    juce::ComboBox presetBox;
    juce::TextButton presetLoadButton;
    juce::TextButton presetSaveButton;
    juce::TextButton presetRenameButton;
    juce::TextButton presetBrowseButton;

    VuMeter vuMeter;

    juce::Label title;
    std::vector<PresetEntry> presetEntries;
    bool suppressPresetChange = false;
    bool suppressOpenLinkSync = false;
    float visualMotionPhase = 0.0f;
    int meterMode = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuperBassAudioProcessorEditor)
};
