#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

class SuperBassAudioProcessor final : public juce::AudioProcessor
{
public:
    SuperBassAudioProcessor();
    ~SuperBassAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    enum class ModuleId
    {
        open,
        space,
        master,
        smile
    };

    struct OnePole
    {
        void prepare (double newSampleRate);
        void setLowPass (float frequencyHz);
        void setHighPass (float frequencyHz);
        float processLowPass (float x);
        float processHighPass (float x);
        void reset();

        double sampleRate = 44100.0;
        float a = 0.0f;
        float z = 0.0f;
    };

    struct Biquad
    {
        void prepare (double newSampleRate);
        void reset();
        void setPeak (float freqHz, float q, float gainDb);
        void setLowShelf (float freqHz, float q, float gainDb);
        void setHighShelf (float freqHz, float q, float gainDb);
        float process (float x);

        double sampleRate = 44100.0;
        float b0 = 1.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float z1 = 0.0f;
        float z2 = 0.0f;
    };

    struct AllPass
    {
        void prepare (double newSampleRate);
        void reset();
        void setFrequency (float frequencyHz);
        float process (float x);

        double sampleRate = 44100.0;
        float a = 0.0f;
        float x1 = 0.0f;
        float y1 = 0.0f;
    };

    struct SmoothValue
    {
        void reset (double sampleRate, float seconds);
        void setTarget (float newTarget);
        float getNext();

        float current = 0.0f;
        float target = 0.0f;
        float coefficient = 0.001f;
    };

    static float getFloatParam (juce::AudioProcessorValueTreeState& state, const juce::String& id);
    static int getChoiceParam (juce::AudioProcessorValueTreeState& state, const juce::String& id);
    static bool getBoolParam (juce::AudioProcessorValueTreeState& state, const juce::String& id);

    void updateFilters();
    void processOpen (juce::AudioBuffer<float>& buffer, float smile);
    void processSpace (juce::AudioBuffer<float>& buffer, float smile);
    void processMaster (juce::AudioBuffer<float>& buffer, float smile);
    void processSmile (juce::AudioBuffer<float>& buffer, float smile);
    void processModule (ModuleId module, juce::AudioBuffer<float>& buffer, float smile);
    std::array<ModuleId, 3> getRoutingOrder() const;

    static float softSaturate (float x, float drive);
    static float softClip (float x, float threshold, float softness);

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> scratchBuffer;
    juce::AudioBuffer<float> delayBuffer;
    juce::AudioBuffer<float> wideDelayBuffer;

    std::array<OnePole, 2> subLowPass;
    std::array<OnePole, 2> subHarmonicHighPass;
    std::array<OnePole, 2> bodyBandLowPass;
    std::array<OnePole, 2> bodyBandHighPass;
    std::array<OnePole, 2> depthHighPass;
    std::array<OnePole, 2> sideHighPass;
    std::array<AllPass, 2> wideAllPass;
    std::array<OnePole, 2> masterDetector;
    std::array<OnePole, 2> transientSlow;
    std::array<OnePole, 2> smileLowPass;
    std::array<OnePole, 2> smileHighPass;

    std::array<Biquad, 2> eqLow;
    std::array<Biquad, 2> eqLowMid;
    std::array<Biquad, 2> eqMid;
    std::array<Biquad, 2> eqHighMid;
    std::array<Biquad, 2> eqHigh;

    SmoothValue smileSmooth;
    SmoothValue depthSmooth;
    SmoothValue wideSmooth;
    SmoothValue wideFreqSmooth;
    SmoothValue wideDelaySmooth;

    double currentSampleRate = 44100.0;
    int delayWritePosition = 0;
    int wideDelayWritePosition = 0;
    std::array<float, 2> compressorEnv {};
    std::array<float, 2> smileGlueEnv {};
    std::array<float, 2> transientFastEnv {};
    std::array<float, 2> transientSlowEnv {};
    float wideModPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuperBassAudioProcessor)
};
