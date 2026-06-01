#pragma once

#include <atomic>
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

    float getInputMeterDb() const noexcept { return inputMeterDb.load(); }
    float getOutputMeterDb() const noexcept { return outputMeterDb.load(); }
    float getGainReductionMeterDb() const noexcept { return gainReductionMeterDb.load(); }

private:
    enum class ModuleId
    {
        open,
        space,
        master
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
        void setCurrentAndTarget (float newValue);
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
    void resetRealtimeState();
    void processOpen (juce::AudioBuffer<float>& buffer);
    void processSpace (juce::AudioBuffer<float>& buffer);
    void processMaster (juce::AudioBuffer<float>& buffer);
    void processSmile (juce::AudioBuffer<float>& buffer);
    void processModule (ModuleId module, juce::AudioBuffer<float>& buffer);
    void processRouting (juce::AudioBuffer<float>& buffer, const std::array<ModuleId, 3>& order);
    std::array<ModuleId, 3> getRoutingOrder() const;
    float measureRms (const juce::AudioBuffer<float>& buffer) const;
    float measurePerceivedLevel (const juce::AudioBuffer<float>& buffer) const;
    void updateLevelMeter (const juce::AudioBuffer<float>& buffer, float& smoother, std::atomic<float>& target);
    void updateGainReductionMeter();
    void applyModuleLevelMatch (juce::AudioBuffer<float>& buffer, float beforeRms, SmoothValue& smoother,
                                float minGain = 0.72f, float maxGain = 1.14f);
    void applyWetLevelMatch (juce::AudioBuffer<float>& buffer);
    void applyFinalLevelMatch (juce::AudioBuffer<float>& buffer);
    void applyOutputGain (juce::AudioBuffer<float>& buffer);

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
    std::array<OnePole, 2> openSideLowPass;
    std::array<OnePole, 2> openCenterLowPass;
    std::array<OnePole, 2> openCenterHighPass;
    std::array<OnePole, 2> openCenterBodyLowPass;
    std::array<OnePole, 2> openCenterBodyHighPass;
    std::array<OnePole, 2> depthHighPass;
    std::array<OnePole, 2> sideHighPass;
    std::array<AllPass, 2> diffusorAllPass;
    std::array<AllPass, 2> diffusorWarmAllPass;
    std::array<AllPass, 2> phaserAllPass;
    std::array<AllPass, 2> phaserWarmAllPass;
    std::array<OnePole, 2> masterDetector;

    std::array<Biquad, 2> eqLow;
    std::array<Biquad, 2> eqLowMid;
    std::array<Biquad, 2> eqMid;
    std::array<Biquad, 2> eqHighMid;
    std::array<Biquad, 2> eqHigh;

    SmoothValue smileSmooth;
    SmoothValue routingWetSmooth;
    SmoothValue wetLevelSmooth;
    SmoothValue openLevelSmooth;
    SmoothValue spaceLevelSmooth;
    SmoothValue masterLevelSmooth;
    SmoothValue finalLevelSmooth;
    SmoothValue outputGainSmooth;
    SmoothValue depthSmooth;
    SmoothValue depthDistanceSmooth;
    SmoothValue wideSmooth;
    SmoothValue wideFreqSmooth;
    SmoothValue wideRateSmooth;
    SmoothValue diffusorFreqSmooth;
    SmoothValue wideDelaySmooth;

    double currentSampleRate = 44100.0;
    int delayWritePosition = 0;
    int wideDelayWritePosition = 0;
    std::array<ModuleId, 3> lastRoutingOrder { ModuleId::open, ModuleId::space, ModuleId::master };
    std::array<float, 2> compressorEnv {};
    std::array<float, 2> transientFastEnv {};
    std::array<float, 2> transientSlowEnv {};
    std::atomic<float> inputMeterDb { -60.0f };
    std::atomic<float> outputMeterDb { -60.0f };
    std::atomic<float> gainReductionMeterDb { 0.0f };
    float inputMeterSmooth = -60.0f;
    float outputMeterSmooth = -60.0f;
    float gainReductionMeterSmooth = 0.0f;
    float blockGainReductionDb = 0.0f;
    float wideModPhase = 0.0f;
    int silentBlockCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuperBassAudioProcessor)
};
