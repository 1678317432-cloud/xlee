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
    void mouseUp (const juce::MouseEvent&) override;

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
    void layoutKnob (Knob& knob, juce::Rectangle<int> bounds, int maxSliderSize);
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
    void syncPresetBoxToProcessorState();
    void timerCallback() override;
    void setActiveScreen (int screenIndex);
    void updateScreenVisibility();
    void updateDynamicUiColours();
    float getSmileNormalised() const;
    std::array<int, 3> getRoutingOrderForUi() const;
    void showScaleContextMenu();
    void applyScalePreset (float scale);
    void updateScaledFonts();
    void renderThemeLayer (juce::Graphics&, juce::Rectangle<int> canvas, float smileAmount, float loopPhase);
    void drawSmileWaveOrbit (juce::Graphics&, juce::Point<float> centre, juce::Colour accent,
                             juce::Colour textColour, float smileAmount);
    void invalidateThemeCache();

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
        void setUiScale (float newScale) noexcept;
        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                               juce::Slider&) override;
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        juce::Font getPopupMenuFont() override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                                   bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool shouldDrawButtonAsHighlighted,
                             bool shouldDrawButtonAsDown) override;
        void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;
        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH,
                           juce::ComboBox&) override;

    private:
        float uiScale = 1.0f;
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

    class MeterStrip final : public juce::Component
    {
    public:
        explicit MeterStrip (SuperBassAudioProcessor&);
        void paint (juce::Graphics&) override;

    private:
        SuperBassAudioProcessor& processor;
        float displayInDb = -60.0f;
        float displayOutDb = -60.0f;
        float displayGrDb = -60.0f;
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
    juce::TextButton nextViewButton;
    juce::TextButton backViewButton;

    MeterStrip meterStrip;
    VuMeter vuMeter;

    juce::Label title;
    std::vector<PresetEntry> presetEntries;
    bool suppressPresetChange = false;
    bool suppressOpenLinkSync = false;
    float visualMotionPhase = 0.0f;
    float smoothedSmileAmount = 1.0f;
    float themeSmileAmount = 1.0f;
    float lastRenderedThemeSmileAmount = -1.0f;
    float smileOrbitFastEnv = 0.0f;
    float smileOrbitGlowEnv = 0.0f;
    float smileOrbitEnv = 0.0f;
    float smileLiquidEnv = 0.0f;
    float smileRippleEnv = 0.0f;
    juce::Image themeCache;
    int themeCacheWidth = 0;
    int themeCacheHeight = 0;
    int themeCacheScreen = -1;
    float lastRenderedThemePhase = -1.0f;
    float uiScale = 1.0f;
    int meterMode = 1;
    int activeScreen = 0;
    int lastLayoutRoutingChoice = -1;
    float lastAdvancedRepaintSmile = -1.0f;
    int lastAdvancedRepaintRoutingChoice = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuperBassAudioProcessorEditor)
};
