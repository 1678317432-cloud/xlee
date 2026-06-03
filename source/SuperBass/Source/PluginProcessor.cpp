#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float pi = juce::MathConstants<float>::pi;
constexpr float internalHeadroomDb = -9.0f;
constexpr int maxOversamplingLatencySamples = 512;

float dbToGain (float db)
{
    return juce::Decibels::decibelsToGain (db);
}

float smoothStep (float x)
{
    const auto clamped = juce::jlimit (0.0f, 1.0f, x);
    return clamped * clamped * (3.0f - 2.0f * clamped);
}

float interpolateDelay (const juce::AudioBuffer<float>& buffer, int channel, float delaySamples, int writePosition)
{
    const auto size = buffer.getNumSamples();
    auto read = static_cast<float> (writePosition) - juce::jlimit (1.0f, static_cast<float> (size - 2), delaySamples);
    while (read < 0.0f)
        read += static_cast<float> (size);

    const auto index0 = static_cast<int> (read) % size;
    const auto index1 = (index0 + 1) % size;
    const auto frac = read - static_cast<float> (index0);
    return buffer.getSample (channel, index0) + (buffer.getSample (channel, index1) - buffer.getSample (channel, index0)) * frac;
}
}

SuperBassAudioProcessor::SuperBassAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout SuperBassAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto percentRange = juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f, 0.75f);
    auto gainRange = juce::NormalisableRange<float> (-8.0f, 8.0f, 0.1f);
    auto trimRange = juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f);
    auto subFreqRange = juce::NormalisableRange<float> (30.0f, 256.0f, 0.1f, 0.48f);
    auto bodyFreqRange = juce::NormalisableRange<float> (60.0f, 800.0f, 0.1f, 0.42f);
    auto wideFreqRange = juce::NormalisableRange<float> (20.0f, 20000.0f, 0.1f, 0.28f);
    auto rateRange = juce::NormalisableRange<float> (0.05f, 2.5f, 0.001f, 0.42f);
    auto diffuserFreqRange = juce::NormalisableRange<float> (80.0f, 12000.0f, 0.1f, 0.35f);
    auto thresholdRange = juce::NormalisableRange<float> (-60.0f, 12.0f, 0.1f);
    auto compAttackRange = juce::NormalisableRange<float> (1.0f, 80.0f, 0.1f, 0.45f);
    auto compReleaseRange = juce::NormalisableRange<float> (30.0f, 2000.0f, 1.0f, 0.48f);

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("smile", "Smile", percentRange, 100.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> ("openEnabled", "Open", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("openSub", "Sub", percentRange, 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("openSubFreq", "Sub Hz", subFreqRange, 61.1f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("openBody", "Body", percentRange, 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("openBodyFreq", "Body Hz", bodyFreqRange, 93.2f));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("openFreqLink", "Open Link", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("openMode", "Open Mode", juce::StringArray { "Punch", "BassHead", "Boom" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterBool> ("spaceEnabled", "Space", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spaceDepth", "Depth", percentRange, 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spaceDepthDistance", "Depth Distance", percentRange, 80.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spaceWide", "Wide", percentRange, 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spaceWideFreq", "Wide Hz", wideFreqRange, 300.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spaceWideRate", "Rate", rateRange, 0.7f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("spaceWideMode", "Wide Mode", juce::StringArray { "Hass", "Flanger", "Phaser" }, 1));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("spaceAllPassEnabled", "Diffusor", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spaceDiffusorHz", "Diffusor Hz", diffuserFreqRange, 200.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> ("masterEnabled", "Master", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterClipper", "Clipper", percentRange, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterCompression", "Comp", percentRange, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterCompThreshold", "Comp Threshold", thresholdRange, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterCompAttack", "Comp Attack", compAttackRange, 80.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterCompRelease", "Comp Release", compReleaseRange, 30.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterTransientAttack", "Trans Attack", juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterTransientRelease", "Trans Release", juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterTransientMix", "Trans Mix", percentRange, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("eqLow", "EQ Low", gainRange, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("eqLowMid", "EQ Low Mid", gainRange, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("eqMid", "EQ Mid", gainRange, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("eqHighMid", "EQ High Mid", gainRange, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("eqHigh", "EQ High", gainRange, 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> ("routingOrder", "Routing", juce::StringArray {
        "Open > Space > Master",
        "Open > Master > Space",
        "Space > Open > Master",
        "Space > Master > Open",
        "Master > Open > Space",
        "Master > Space > Open" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterChoice> ("oversampling", "Oversampling",
        juce::StringArray { "1x", "2x", "4x", "8x" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output", "Gain", trimRange, 0.0f));

    return { params.begin(), params.end() };
}

void SuperBassAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    const auto channels = juce::jmax (1, getTotalNumOutputChannels());

    dryBuffer.setSize (channels, samplesPerBlock);
    alignedDryBuffer.setSize (channels, samplesPerBlock);
    scratchBuffer.setSize (channels, samplesPerBlock);
    oversampledDryBuffer.setSize (channels, samplesPerBlock * 8);
    dryLatencyBuffer.setSize (channels, samplesPerBlock + maxOversamplingLatencySamples + 8);
    delayBuffer.setSize (channels, static_cast<int> (sampleRate * 0.08 * 8.0));
    wideDelayBuffer.setSize (channels, static_cast<int> (sampleRate * 0.04 * 8.0));
    alignedDryBuffer.clear();
    oversampledDryBuffer.clear();
    dryLatencyBuffer.clear();
    delayBuffer.clear();
    wideDelayBuffer.clear();
    prepareOversampling (channels, samplesPerBlock);
    dryLatencyWritePosition = 0;
    delayWritePosition = 0;
    wideDelayWritePosition = 0;

    for (auto* collection : { &subLowPass, &subHarmonicHighPass, &bodyBandLowPass, &bodyBandHighPass, &openSideLowPass, &openCenterLowPass, &openCenterHighPass, &openCenterBodyLowPass, &openCenterBodyHighPass, &openDeltaPolishLowPass, &openCenterDeltaPolishLowPass, &openSubPolishLowPass, &openCenterSubPolishLowPass, &openSubEvenPolishLowPass, &openCenterSubEvenPolishLowPass, &openSubOddPolishLowPass, &openCenterSubOddPolishLowPass, &openBodyPolishLowPass, &openCenterBodyPolishLowPass, &openBodyEvenPolishLowPass, &openCenterBodyEvenPolishLowPass, &openBodyOddPolishLowPass, &openCenterBodyOddPolishLowPass, &openMidPolishLowPass, &openCenterMidPolishLowPass, &openMidDrivePolishLowPass, &openCenterMidDrivePolishLowPass, &openBoomPolishLowPass, &openCenterBoomPolishLowPass, &openPsychoHarmonicHighPass, &openPsychoHarmonicLowPass, &openPsychoCenterHarmonicHighPass, &openPsychoCenterHarmonicLowPass, &depthHighPass, &sideHighPass, &masterDetector })
        for (auto& filter : *collection)
            filter.prepare (sampleRate);

    for (auto* collection : { &diffusorAllPass, &diffusorWarmAllPass, &phaserAllPass, &phaserWarmAllPass })
        for (auto& filter : *collection)
            filter.prepare (sampleRate);

    for (auto* collection : { &eqLow, &eqLowMid, &eqMid, &eqHighMid, &eqHigh })
        for (auto& filter : *collection)
            filter.prepare (sampleRate);

    smileSmooth.reset (sampleRate, 0.04f);
    routingWetSmooth.reset (sampleRate, 0.012f);
    wetLevelSmooth.reset (sampleRate, 0.12f);
    openLevelSmooth.reset (sampleRate, 0.09f);
    spaceLevelSmooth.reset (sampleRate, 0.09f);
    masterLevelSmooth.reset (sampleRate, 0.09f);
    finalLevelSmooth.reset (sampleRate, 0.16f);
    outputGainSmooth.reset (sampleRate, 0.04f);
    openSubSmooth.reset (sampleRate, 0.045f);
    openBodySmooth.reset (sampleRate, 0.045f);
    openSubFreqSmooth.reset (sampleRate, 0.075f);
    openBodyFreqSmooth.reset (sampleRate, 0.075f);
    depthSmooth.reset (sampleRate, 0.025f);
    depthDistanceSmooth.reset (sampleRate, 0.04f);
    wideSmooth.reset (sampleRate, 0.035f);
    wideFreqSmooth.reset (sampleRate, 0.045f);
    wideRateSmooth.reset (sampleRate, 0.08f);
    diffusorFreqSmooth.reset (sampleRate, 0.05f);
    wideDelaySmooth.reset (sampleRate, 0.02f);
    masterClipperSmooth.reset (sampleRate, 0.035f);
    masterCompressionSmooth.reset (sampleRate, 0.04f);
    masterThresholdSmooth.reset (sampleRate, 0.045f);
    masterAttackSmooth.reset (sampleRate, 0.045f);
    masterReleaseSmooth.reset (sampleRate, 0.045f);
    transientAttackSmooth.reset (sampleRate, 0.035f);
    transientReleaseSmooth.reset (sampleRate, 0.035f);
    transientMixSmooth.reset (sampleRate, 0.035f);
    compressorEnv = {};
    transientFastEnv = {};
    transientSlowEnv = {};
    openPsychoFastEnv = {};
    openPsychoSlowEnv = {};
    openPsychoCenterFastEnv = {};
    openPsychoCenterSlowEnv = {};
    openStabilityEnv = {};
    openSubControlEnv = {};
    openBodyControlEnv = {};
    openMidControlEnv = {};
    openCenterStabilityEnv = 0.0f;
    openCenterSubControlEnv = 0.0f;
    openCenterBodyControlEnv = 0.0f;
    openCenterMidControlEnv = 0.0f;
    masterClipPeakEnv = {};
    masterClipBodyEnv = {};
    silentBlockCount = 0;
    inputMeterSmooth = -60.0f;
    outputMeterSmooth = -60.0f;
    gainReductionMeterSmooth = 0.0f;
    blockGainReductionDb = 0.0f;
    inputMeterDb.store (-60.0f);
    outputMeterDb.store (-60.0f);
    gainReductionMeterDb.store (0.0f);
    smileSmooth.setCurrentAndTarget (getFloatParam (apvts, "smile") / 100.0f);
    routingWetSmooth.setCurrentAndTarget (1.0f);
    wetLevelSmooth.setCurrentAndTarget (1.0f);
    openLevelSmooth.setCurrentAndTarget (1.0f);
    spaceLevelSmooth.setCurrentAndTarget (1.0f);
    masterLevelSmooth.setCurrentAndTarget (1.0f);
    finalLevelSmooth.setCurrentAndTarget (1.0f);
    outputGainSmooth.setCurrentAndTarget (dbToGain (getFloatParam (apvts, "output")));
    openSubSmooth.setCurrentAndTarget (getFloatParam (apvts, "openSub") / 100.0f);
    openBodySmooth.setCurrentAndTarget (getFloatParam (apvts, "openBody") / 100.0f);
    openSubFreqSmooth.setCurrentAndTarget (getFloatParam (apvts, "openSubFreq"));
    openBodyFreqSmooth.setCurrentAndTarget (getFloatParam (apvts, "openBodyFreq"));
    depthSmooth.setCurrentAndTarget (getFloatParam (apvts, "spaceDepth") / 100.0f);
    depthDistanceSmooth.setCurrentAndTarget (getFloatParam (apvts, "spaceDepthDistance") / 100.0f);
    wideSmooth.setCurrentAndTarget (getFloatParam (apvts, "spaceWide") / 100.0f);
    wideFreqSmooth.setCurrentAndTarget (getFloatParam (apvts, "spaceWideFreq"));
    wideRateSmooth.setCurrentAndTarget (getFloatParam (apvts, "spaceWideRate"));
    diffusorFreqSmooth.setCurrentAndTarget (getFloatParam (apvts, "spaceDiffusorHz"));
    wideDelaySmooth.setCurrentAndTarget (static_cast<float> (sampleRate * 0.008));
    masterClipperSmooth.setCurrentAndTarget (getFloatParam (apvts, "masterClipper") / 100.0f);
    masterCompressionSmooth.setCurrentAndTarget (getFloatParam (apvts, "masterCompression") / 100.0f);
    masterThresholdSmooth.setCurrentAndTarget (getFloatParam (apvts, "masterCompThreshold"));
    masterAttackSmooth.setCurrentAndTarget (getFloatParam (apvts, "masterCompAttack"));
    masterReleaseSmooth.setCurrentAndTarget (getFloatParam (apvts, "masterCompRelease"));
    transientAttackSmooth.setCurrentAndTarget (getFloatParam (apvts, "masterTransientAttack") / 100.0f);
    transientReleaseSmooth.setCurrentAndTarget (getFloatParam (apvts, "masterTransientRelease") / 100.0f);
    transientMixSmooth.setCurrentAndTarget (getFloatParam (apvts, "masterTransientMix") / 100.0f);
    updateFilters();
    updateOversamplingState();
}

void SuperBassAudioProcessor::releaseResources()
{
    resetRealtimeState();
}

bool SuperBassAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    return mainOut == juce::AudioChannelSet::mono() || mainOut == juce::AudioChannelSet::stereo();
}

void SuperBassAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, numSamples);

    dryBuffer.makeCopyOf (buffer, true);
    updateLevelMeter (dryBuffer, inputMeterSmooth, inputMeterDb);

    if (measureRms (dryBuffer) < 0.000003f)
    {
        ++silentBlockCount;
        if (silentBlockCount == 2)
            resetRealtimeState();
    }
    else
    {
        if (silentBlockCount > 1)
        {
            startupRampSamplesTotal = juce::jmax (1, static_cast<int> (currentSampleRate * 0.012));
            startupRampSamplesRemaining = startupRampSamplesTotal;
        }

        silentBlockCount = 0;
    }

    updateFilters();

    const auto smileAmount = juce::jlimit (0.0f, 1.0f, getFloatParam (apvts, "smile") / 100.0f);
    smileSmooth.setTarget (smileAmount);
    blockGainReductionDb = 0.0f;
    updateOversamplingState();
    updateLatencyCompensatedDryBuffer (oversamplingLatencySamples, numChannels, numSamples);

    buffer.applyGain (dbToGain (internalHeadroomDb));

    const auto routingOrder = getRoutingOrder();
    if (routingOrder != lastRoutingOrder)
    {
        routingWetSmooth.setCurrentAndTarget (0.0f);
        lastRoutingOrder = routingOrder;
    }

    if (auto* oversampler = getActiveOversampler())
        processRoutingOversampled (buffer, routingOrder, *oversampler, getOversamplingFactor());
    else
        processRouting (buffer, routingOrder);

    routingWetSmooth.setTarget (1.0f);
    for (int i = 0; i < numSamples; ++i)
    {
        const auto gain = routingWetSmooth.getNext();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* wet = buffer.getWritePointer (ch);
            const auto internalDry = alignedDryBuffer.getSample (ch, i) * dbToGain (internalHeadroomDb);
            wet[i] = internalDry + (wet[i] - internalDry) * gain;
        }
    }

    buffer.applyGain (dbToGain (-internalHeadroomDb));
    applyWetLevelMatch (buffer);
    processSmile (buffer);
    applyFinalLevelMatch (buffer);
    applyOversamplingTransitionFade (buffer);
    applyStartupRamp (buffer);
    applyOutputGain (buffer);
    updateLevelMeter (buffer, outputMeterSmooth, outputMeterDb);
    updateGainReductionMeter();
}

void SuperBassAudioProcessor::processRouting (juce::AudioBuffer<float>& buffer, const std::array<ModuleId, 3>& order)
{
    for (auto module : order)
        processModule (module, buffer);
}

void SuperBassAudioProcessor::processRoutingOversampled (juce::AudioBuffer<float>& buffer,
                                                         const std::array<ModuleId, 3>& order,
                                                         juce::dsp::Oversampling<float>& oversampler,
                                                         int factor)
{
    juce::dsp::AudioBlock<float> block (buffer);
    const auto upsampledBlock = oversampler.processSamplesUp (block);
    const auto upsampledChannels = static_cast<int> (upsampledBlock.getNumChannels());
    const auto upsampledSamples = static_cast<int> (upsampledBlock.getNumSamples());

    oversampledDryBuffer.setSize (upsampledChannels, upsampledSamples, false, false, true);
    for (int ch = 0; ch < upsampledChannels; ++ch)
    {
        oversampledChannelPointers[static_cast<size_t> (ch)] = upsampledBlock.getChannelPointer (static_cast<size_t> (ch));
        auto* dry = oversampledDryBuffer.getWritePointer (ch);
        const auto* source = alignedDryBuffer.getReadPointer (ch);
        for (int i = 0; i < upsampledSamples; ++i)
            dry[i] = source[i / factor] * dbToGain (internalHeadroomDb);
    }

    juce::AudioBuffer<float> upsampledBuffer (oversampledChannelPointers.data(), upsampledChannels, upsampledSamples);
    const auto baseRate = currentSampleRate;
    setRealtimeProcessingSampleRate (baseRate * static_cast<double> (factor));
    updateFilters();

    for (auto module : order)
        processModule (module, upsampledBuffer);

    setRealtimeProcessingSampleRate (baseRate);
    updateFilters();
    oversampler.processSamplesDown (block);
}

void SuperBassAudioProcessor::processModule (ModuleId module, juce::AudioBuffer<float>& buffer)
{
    if (module == ModuleId::open && getBoolParam (apvts, "openEnabled"))
    {
        const auto beforeRms = measurePerceivedLevel (buffer);
        processOpen (buffer);
        applyModuleLevelMatch (buffer, beforeRms, openLevelSmooth, 0.6f, 1.12f);
    }

    if (module == ModuleId::space && getBoolParam (apvts, "spaceEnabled"))
    {
        const auto beforeRms = measurePerceivedLevel (buffer);
        processSpace (buffer);
        applyModuleLevelMatch (buffer, beforeRms, spaceLevelSmooth, 0.72f, 1.12f);
    }

    if (module == ModuleId::master && getBoolParam (apvts, "masterEnabled"))
    {
        const auto beforeRms = measurePerceivedLevel (buffer);
        processMaster (buffer);
        applyModuleLevelMatch (buffer, beforeRms, masterLevelSmooth, 0.66f, 1.08f);
    }
}

int SuperBassAudioProcessor::getOversamplingIndex() const
{
    return juce::jlimit (0, 3, getChoiceParam (const_cast<juce::AudioProcessorValueTreeState&> (apvts), "oversampling"));
}

int SuperBassAudioProcessor::getOversamplingFactor() const
{
    return 1 << getOversamplingIndex();
}

juce::dsp::Oversampling<float>* SuperBassAudioProcessor::getActiveOversampler() const
{
    const auto index = getOversamplingIndex();
    if (index <= 0)
        return nullptr;

    return oversamplers[static_cast<size_t> (index - 1)].get();
}

int SuperBassAudioProcessor::getActiveOversamplingLatency() const
{
    if (auto* oversampler = getActiveOversampler())
        return static_cast<int> (std::ceil (oversampler->getLatencyInSamples()));

    return 0;
}

void SuperBassAudioProcessor::prepareOversampling (int channels, int samplesPerBlock)
{
    for (size_t i = 0; i < oversamplers.size(); ++i)
    {
        oversamplers[i] = std::make_unique<juce::dsp::Oversampling<float>> (
            static_cast<size_t> (channels),
            i + 1,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true,
            true);
        oversamplers[i]->initProcessing (static_cast<size_t> (samplesPerBlock));
        oversamplers[i]->reset();
    }
}

void SuperBassAudioProcessor::updateOversamplingState()
{
    const auto newIndex = getOversamplingIndex();
    const auto newLatency = getActiveOversamplingLatency();

    if (newIndex == activeOversamplingIndex && newLatency == oversamplingLatencySamples)
        return;

    activeOversamplingIndex = newIndex;
    oversamplingLatencySamples = newLatency;
    setLatencySamples (oversamplingLatencySamples);

    if (auto* oversampler = getActiveOversampler())
        oversampler->reset();

    resetRealtimeState();
    oversamplingFadeSamplesTotal = juce::jmax (1, static_cast<int> (currentSampleRate * 0.018));
    oversamplingFadeSamplesRemaining = oversamplingFadeSamplesTotal;
}

void SuperBassAudioProcessor::setRealtimeProcessingSampleRate (double sampleRate)
{
    currentSampleRate = sampleRate;

    for (auto* collection : { &subLowPass, &subHarmonicHighPass, &bodyBandLowPass, &bodyBandHighPass, &openSideLowPass, &openCenterLowPass, &openCenterHighPass, &openCenterBodyLowPass, &openCenterBodyHighPass, &openDeltaPolishLowPass, &openCenterDeltaPolishLowPass, &openSubPolishLowPass, &openCenterSubPolishLowPass, &openSubEvenPolishLowPass, &openCenterSubEvenPolishLowPass, &openSubOddPolishLowPass, &openCenterSubOddPolishLowPass, &openBodyPolishLowPass, &openCenterBodyPolishLowPass, &openBodyEvenPolishLowPass, &openCenterBodyEvenPolishLowPass, &openBodyOddPolishLowPass, &openCenterBodyOddPolishLowPass, &openMidPolishLowPass, &openCenterMidPolishLowPass, &openMidDrivePolishLowPass, &openCenterMidDrivePolishLowPass, &openBoomPolishLowPass, &openCenterBoomPolishLowPass, &openPsychoHarmonicHighPass, &openPsychoHarmonicLowPass, &openPsychoCenterHarmonicHighPass, &openPsychoCenterHarmonicLowPass, &depthHighPass, &sideHighPass, &masterDetector })
        for (auto& filter : *collection)
            filter.sampleRate = sampleRate;

    for (auto* collection : { &diffusorAllPass, &diffusorWarmAllPass, &phaserAllPass, &phaserWarmAllPass })
        for (auto& filter : *collection)
            filter.sampleRate = sampleRate;

    for (auto* collection : { &eqLow, &eqLowMid, &eqMid, &eqHighMid, &eqHigh })
        for (auto& filter : *collection)
            filter.sampleRate = sampleRate;

    for (auto* smoother : { &smileSmooth, &routingWetSmooth, &wetLevelSmooth, &openLevelSmooth, &spaceLevelSmooth,
                            &masterLevelSmooth, &finalLevelSmooth, &outputGainSmooth, &openSubSmooth, &openBodySmooth,
                            &openSubFreqSmooth, &openBodyFreqSmooth, &depthSmooth, &depthDistanceSmooth, &wideSmooth,
                            &wideFreqSmooth, &wideRateSmooth, &diffusorFreqSmooth, &wideDelaySmooth, &masterClipperSmooth,
                            &masterCompressionSmooth, &masterThresholdSmooth, &masterAttackSmooth, &masterReleaseSmooth,
                            &transientAttackSmooth, &transientReleaseSmooth, &transientMixSmooth })
        smoother->updateSampleRate (sampleRate);
}

void SuperBassAudioProcessor::updateLatencyCompensatedDryBuffer (int latencySamples, int numChannels, int numSamples)
{
    alignedDryBuffer.setSize (numChannels, numSamples, false, false, true);

    if (latencySamples <= 0 || dryLatencyBuffer.getNumSamples() <= latencySamples + numSamples)
    {
        alignedDryBuffer.makeCopyOf (dryBuffer, true);
        return;
    }

    const auto latencySize = dryLatencyBuffer.getNumSamples();
    for (int i = 0; i < numSamples; ++i)
    {
        const auto readPosition = (dryLatencyWritePosition + latencySize - latencySamples) % latencySize;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            alignedDryBuffer.setSample (ch, i, dryLatencyBuffer.getSample (ch, readPosition));
            dryLatencyBuffer.setSample (ch, dryLatencyWritePosition, dryBuffer.getSample (ch, i));
        }

        dryLatencyWritePosition = (dryLatencyWritePosition + 1) % latencySize;
    }
}

void SuperBassAudioProcessor::applyOversamplingTransitionFade (juce::AudioBuffer<float>& buffer)
{
    if (oversamplingFadeSamplesRemaining <= 0)
        return;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto fade = 1.0f - static_cast<float> (oversamplingFadeSamplesRemaining)
                               / static_cast<float> (juce::jmax (1, oversamplingFadeSamplesTotal));
        --oversamplingFadeSamplesRemaining;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* wet = buffer.getWritePointer (ch);
            wet[i] = alignedDryBuffer.getSample (ch, i) + (wet[i] - alignedDryBuffer.getSample (ch, i)) * fade;
        }

        if (oversamplingFadeSamplesRemaining <= 0)
            break;
    }
}

void SuperBassAudioProcessor::applyStartupRamp (juce::AudioBuffer<float>& buffer)
{
    if (startupRampSamplesRemaining <= 0)
        return;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto fade = smoothStep (1.0f - static_cast<float> (startupRampSamplesRemaining)
                                           / static_cast<float> (juce::jmax (1, startupRampSamplesTotal)));
        --startupRampSamplesRemaining;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* wet = buffer.getWritePointer (ch);
            const auto dry = alignedDryBuffer.getSample (ch, i);
            wet[i] = dry + (wet[i] - dry) * fade;
        }

        if (startupRampSamplesRemaining <= 0)
            break;
    }
}

std::array<SuperBassAudioProcessor::ModuleId, 3> SuperBassAudioProcessor::getRoutingOrder() const
{
    switch (getChoiceParam (const_cast<juce::AudioProcessorValueTreeState&> (apvts), "routingOrder"))
    {
        case 1: return { ModuleId::open, ModuleId::master, ModuleId::space };
        case 2: return { ModuleId::space, ModuleId::open, ModuleId::master };
        case 3: return { ModuleId::space, ModuleId::master, ModuleId::open };
        case 4: return { ModuleId::master, ModuleId::open, ModuleId::space };
        case 5: return { ModuleId::master, ModuleId::space, ModuleId::open };
        default: return { ModuleId::open, ModuleId::space, ModuleId::master };
    }
}

float SuperBassAudioProcessor::generateOpenPsychoHarmonics (float subBand, OnePole& harmonicHighPass,
                                                            OnePole& harmonicLowPass, float& fastEnv,
                                                            float& slowEnv, float amount)
{
    const auto amountClamped = juce::jlimit (0.0f, 1.8f, amount);
    if (amountClamped <= 0.0001f)
        return 0.0f;

    const auto magnitude = std::abs (subBand);
    const auto fastCoeff = 1.0f - std::exp (-1.0f / static_cast<float> (currentSampleRate * 0.0045));
    const auto slowCoeff = 1.0f - std::exp (-1.0f / static_cast<float> (currentSampleRate * 0.075));
    fastEnv += fastCoeff * (magnitude - fastEnv);
    slowEnv += slowCoeff * (magnitude - slowEnv);

    const auto noiseGate = smoothStep (juce::jlimit (0.0f, 1.0f, (slowEnv - 0.00082f) * 560.0f));
    const auto dynamicLift = smoothStep (juce::jlimit (0.0f, 1.0f, (slowEnv - 0.00092f) * 58.0f));
    const auto transientRatio = (fastEnv - slowEnv) / (slowEnv + 0.0007f);
    const auto transientGuard = 1.0f - 0.38f * smoothStep (juce::jlimit (0.0f, 1.0f, transientRatio));
    const auto follow = (0.06f + dynamicLift * 0.94f) * transientGuard * noiseGate;

    const auto shaped = premiumSaturate (subBand * (1.34f + amountClamped * 1.28f), 1.03f + amountClamped * 0.82f, 0.68f);
    const auto signedEven = (shaped * shaped * std::copysign (1.0f, shaped)) * (0.22f + amountClamped * 0.2f);
    const auto rectifiedMagnitude = juce::jmax (0.0f, std::abs (shaped) - slowEnv * 0.62f);
    const auto rectifiedEven = rectifiedMagnitude * std::copysign (1.0f, shaped) * (0.07f + amountClamped * 0.075f);
    const auto oddHarmonic = (softSaturate (shaped * (1.12f + amountClamped * 0.54f), 1.12f + amountClamped * 0.86f) - shaped)
                           * (0.18f + amountClamped * 0.11f);
    const auto controlled = softClip (signedEven + rectifiedEven + oddHarmonic, 1.04f - amountClamped * 0.035f, 1.12f);
    const auto translated = harmonicLowPass.processLowPass (harmonicHighPass.processHighPass (controlled));

    return juce::jlimit (-1.05f, 1.05f, translated * follow * amountClamped);
}

void SuperBassAudioProcessor::processOpen (juce::AudioBuffer<float>& buffer)
{
    openSubSmooth.setTarget (getFloatParam (apvts, "openSub") / 100.0f);
    openBodySmooth.setTarget (getFloatParam (apvts, "openBody") / 100.0f);
    openSubFreqSmooth.setTarget (getFloatParam (apvts, "openSubFreq"));
    openBodyFreqSmooth.setTarget (getFloatParam (apvts, "openBodyFreq"));
    const auto subAmountBase = juce::jlimit (0.0f, 1.0f, openSubSmooth.getNext (buffer.getNumSamples()));
    const auto bodyAmountBase = juce::jlimit (0.0f, 1.0f, openBodySmooth.getNext (buffer.getNumSamples()));
    const auto subFreq = openSubFreqSmooth.getNext (buffer.getNumSamples());
    const auto bodyFreq = openBodyFreqSmooth.getNext (buffer.getNumSamples());
    const auto openMode = getChoiceParam (apvts, "openMode");
    const auto bassHead = openMode == 1 ? 1.0f : 0.0f;
    const auto boom = openMode == 2 ? 1.0f : 0.0f;
    const auto punch = openMode == 0 ? 1.0f : 0.0f;
    const auto subPriority = 1.02f * punch + 1.74f * bassHead + 0.72f * boom;
    const auto upperBassPriority = 1.48f * punch + 0.78f * bassHead + 1.24f * boom;
    const auto lowMidPriority = 1.44f * punch + 0.44f * bassHead + 1.9f * boom;
    const auto midPriority = 0.8f * punch + 0.22f * bassHead + 1.74f * boom;
    const auto psychoPriority = 0.98f * punch + 1.7f * bassHead + 0.66f * boom;
    const auto subCleanliness = 0.56f * punch + 1.0f * bassHead + 0.42f * boom;
    const auto boomDrivePriority = 0.28f * punch + 0.08f * bassHead + 1.0f * boom;
    const auto bodyDrive = 1.03f + bodyAmountBase * (1.74f + upperBassPriority * 0.42f + lowMidPriority * 0.33f + boomDrivePriority * 0.5f);
    const auto subDrive = 1.012f + subAmountBase * (1.46f + subPriority * 0.5f + psychoPriority * 0.24f + boomDrivePriority * 0.08f);
    const auto openDensity = juce::jlimit (0.0f, 1.0f,
        subAmountBase * (0.46f + subPriority * 0.22f + psychoPriority * 0.2f)
        + bodyAmountBase * (0.54f + upperBassPriority * 0.18f + lowMidPriority * 0.2f));
    const auto punchFocus = 1.0f + punch * 0.18f + boom * 0.06f;
    const auto subTargetFocus = 1.0f + smoothStep (juce::jlimit (0.0f, 1.0f, (116.0f - subFreq) / 86.0f)) * (0.1f + bassHead * 0.085f + punch * 0.04f);
    const auto bodyTargetFocus = 1.0f + smoothStep (juce::jlimit (0.0f, 1.0f, (238.0f - bodyFreq) / 178.0f)) * (0.085f + punch * 0.078f + boom * 0.095f);
    const auto subWeight = (0.94f + subPriority * 0.56f + psychoPriority * 0.18f) * subTargetFocus;
    const auto bodyWeight = (0.92f + upperBassPriority * 0.48f + lowMidPriority * 0.26f) * bodyTargetFocus;
    const auto midExcitement = 0.88f + lowMidPriority * 0.26f + midPriority * 0.56f;
    const auto lowBodyFocus = boom * smoothStep (juce::jlimit (0.0f, 1.0f, (145.0f - bodyFreq) / 70.0f));
    const auto harmonicControl = 0.17f + bassHead * 0.2f + boom * 0.34f;
    const auto psychoAmount = juce::jlimit (0.0f, 1.46f,
        subAmountBase * (0.52f + psychoPriority * 0.52f)
        + bodyAmountBase * (0.08f + punch * 0.22f + boom * 0.22f));
    const auto psychoMix = 0.36f * punch + 0.82f * bassHead + 0.24f * boom;
    const auto psychoHighPassHz = juce::jlimit (70.0f, 310.0f, subFreq * (1.36f + punch * 0.08f + bassHead * 0.03f + boom * 0.34f));
    const auto psychoLowPassHz = juce::jlimit (175.0f, 980.0f, subFreq * (4.45f + punch * 0.82f - bassHead * 0.18f + boom * 1.72f)
                                                      + bodyFreq * (0.14f + punch * 0.08f + boom * 0.38f));
    const auto subPolishHz = juce::jlimit (115.0f, 620.0f,
        subFreq * (3.05f + punch * 0.72f - bassHead * 0.16f + boom * 1.22f)
        + bodyFreq * (0.08f + punch * 0.08f + boom * 0.22f));
    const auto bodyPolishHz = juce::jlimit (420.0f, 2850.0f,
        bodyFreq * (3.82f + punch * 1.1f - bassHead * 1.08f + boom * 1.5f - lowBodyFocus * 1.36f)
        + subFreq * (2.08f + punch * 0.4f + boom * 0.78f));
    const auto midPolishHz = juce::jlimit (650.0f, 3900.0f,
        bodyFreq * (4.58f + punch * 0.76f - bassHead * 1.42f + boom * 1.86f - lowBodyFocus * 1.88f)
        + subFreq * (2.18f + boom * 2.05f));
    const auto harmonicPolishHz = juce::jlimit (1280.0f, 6600.0f,
        bodyFreq * (5.15f + punch * 1.18f + boom * 0.22f - lowBodyFocus * 0.62f) + subFreq * (2.8f + bassHead * 0.64f));
    const auto boomPolishHz = juce::jlimit (680.0f, 2950.0f,
        bodyFreq * (3.24f + boom * 0.72f - lowBodyFocus * 1.58f)
        + subFreq * (2.72f + boom * 1.05f));
    const auto subRawBleed = juce::jlimit (0.0015f, 0.052f, 0.021f + punch * 0.018f + boom * 0.014f - bassHead * 0.024f - lowBodyFocus * 0.012f);
    const auto bodyRawBleed = juce::jlimit (0.004f, 0.062f, 0.028f + punch * 0.021f + boom * 0.014f - bassHead * 0.022f - lowBodyFocus * 0.032f);
    const auto midRawBleed = juce::jlimit (0.0025f, 0.052f, 0.017f + punch * 0.014f + boom * 0.014f - bassHead * 0.02f - lowBodyFocus * 0.032f);
    const auto punchAuthority = 0.92f + upperBassPriority * 0.16f + punch * 0.06f;
    const auto bassHeadDepth = 1.0f + bassHead * 0.34f;
    const auto antiFizz = 1.0f - boom * (0.28f + lowBodyFocus * 0.14f) - bassHead * 0.16f;
    const auto stabilityDepth = 0.72f + punch * 0.21f + bassHead * 0.52f + boom * 0.36f;
    const auto subStabilityWeight = 0.84f + subPriority * 0.29f + bassHead * 0.32f;
    const auto bodyStabilityWeight = 0.48f + upperBassPriority * 0.18f + lowMidPriority * 0.22f + lowBodyFocus * 0.2f;
    const auto openAttackCoeff = std::exp (-1.0f / static_cast<float> (currentSampleRate * (0.0028f + bassHead * 0.001f)));
    const auto openReleaseCoeff = std::exp (-1.0f / static_cast<float> (currentSampleRate * (0.112f + bassHead * 0.058f + boom * 0.038f)));

    if (buffer.getNumChannels() >= 2)
    {
        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);
        auto& sideLpL = openSideLowPass[0];
        auto& sideLpR = openSideLowPass[1];
        sideLpL.setLowPass (juce::jlimit (90.0f, 420.0f, bodyFreq * 1.15f));
        sideLpR.setLowPass (juce::jlimit (90.0f, 420.0f, bodyFreq * 1.15f));

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto mid = 0.5f * (left[i] + right[i]);
            const auto side = 0.5f * (left[i] - right[i]);
            const auto lowSide = 0.5f * (sideLpL.processLowPass (left[i]) - sideLpR.processLowPass (right[i]));
            const auto anchor = juce::jlimit (0.0f, 0.995f,
                (0.94f + punch * 0.08f + bassHead * 0.2f + boom * 0.1f) * subAmountBase
                + (0.54f + punch * 0.03f + bassHead * 0.08f + boom * 0.04f) * bodyAmountBase);
            const auto anchoredSide = side - lowSide * anchor;
            left[i] = mid + anchoredSide;
            right[i] = mid - anchoredSide;
        }

        auto& subMidLp = openCenterLowPass[0];
        auto& subHarmHp = openCenterHighPass[0];
        auto& bodyMidLp = openCenterBodyLowPass[0];
        auto& bodyMidHp = openCenterBodyHighPass[0];
        auto& deltaPolish = openCenterDeltaPolishLowPass[0];
        auto& subPolish = openCenterSubPolishLowPass[0];
        auto& subEvenPolish = openCenterSubEvenPolishLowPass[0];
        auto& subOddPolish = openCenterSubOddPolishLowPass[0];
        auto& bodyPolish = openCenterBodyPolishLowPass[0];
        auto& bodyEvenPolish = openCenterBodyEvenPolishLowPass[0];
        auto& bodyOddPolish = openCenterBodyOddPolishLowPass[0];
        auto& midPolish = openCenterMidPolishLowPass[0];
        auto& midDrivePolish = openCenterMidDrivePolishLowPass[0];
        auto& boomPolish = openCenterBoomPolishLowPass[0];
        auto& psychoHp = openPsychoCenterHarmonicHighPass[0];
        auto& psychoLp = openPsychoCenterHarmonicLowPass[0];
        subMidLp.setLowPass (juce::jlimit (48.0f, 230.0f, subFreq * (1.2f + bassHead * 0.16f + punch * 0.07f + boom * 0.02f)));
        subHarmHp.setHighPass (juce::jmax (70.0f, subFreq * 1.32f));
        bodyMidLp.setLowPass (juce::jlimit (150.0f, 2800.0f, bodyFreq * (2.55f + upperBassPriority * 0.2f + lowMidPriority * 0.25f + midPriority * 0.08f - lowBodyFocus * 0.24f)));
        bodyMidHp.setHighPass (juce::jmax (34.0f, bodyFreq * (0.32f + bassHead * 0.08f + boom * 0.04f)));
        deltaPolish.setLowPass (harmonicPolishHz);
        subPolish.setLowPass (subPolishHz);
        subEvenPolish.setLowPass (subPolishHz * (0.82f + bassHead * 0.04f));
        subOddPolish.setLowPass (subPolishHz * (1.08f + boom * 0.22f));
        bodyPolish.setLowPass (bodyPolishHz);
        bodyEvenPolish.setLowPass (bodyPolishHz * (0.84f + punch * 0.05f));
        bodyOddPolish.setLowPass (bodyPolishHz * (1.04f + boom * 0.12f));
        midPolish.setLowPass (midPolishHz);
        midDrivePolish.setLowPass (midPolishHz * (0.88f + boom * 0.18f));
        boomPolish.setLowPass (boomPolishHz);
        psychoHp.setHighPass (psychoHighPassHz);
        psychoLp.setLowPass (psychoLowPassHz);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto mid = 0.5f * (left[i] + right[i]);
            const auto side = 0.5f * (left[i] - right[i]);
            const auto subBand = subMidLp.processLowPass (mid * 0.5f);
            const auto subDriveInput = premiumSaturate (subBand * (1.04f + subPriority * 0.078f + boomDrivePriority * 0.04f),
                                                        subDrive, 0.66f + bassHead * 0.14f);
            const auto subHarmRaw = subHarmHp.processHighPass (subDriveInput);
            const auto subEvenRaw = (subHarmRaw * subHarmRaw * std::copysign (1.0f, subHarmRaw))
                                  * (0.09f + subPriority * 0.048f + psychoPriority * 0.05f + boomDrivePriority * 0.026f)
                                  * subAmountBase;
            const auto subEven = subEvenPolish.processLowPass (subEvenRaw) * (0.96f - subCleanliness * 0.08f)
                               + subEvenRaw * subRawBleed;
            const auto subOddRaw = (premiumSaturate (subHarmRaw * (1.0f + punch * 0.42f + boomDrivePriority * 0.92f),
                                                     1.0f + punch * 0.52f + boomDrivePriority * 1.12f,
                                                     0.58f + bassHead * 0.16f) - subHarmRaw)
                                 * (punch * 0.22f + boomDrivePriority * 0.38f);
            const auto subOdd = subOddPolish.processLowPass (subOddRaw) * antiFizz
                              + subOddRaw * subRawBleed;
            const auto subHarm = softClip (subPolish.processLowPass (subHarmRaw + subEven + subOdd * (0.34f + boom * 0.11f)),
                                           1.19f - boom * 0.03f - bassHead * 0.004f,
                                           1.12f + bassHead * 0.12f);
            const auto psychoHarmRaw = generateOpenPsychoHarmonics (subBand, psychoHp, psychoLp,
                                                                    openPsychoCenterFastEnv[0],
                                                                    openPsychoCenterSlowEnv[0],
                                                                    psychoAmount);
            const auto psychoHarm = subPolish.processLowPass (psychoHarmRaw);
            const auto bodyBand = bodyMidHp.processHighPass (bodyMidLp.processLowPass (mid * 0.5f));
            const auto centerDetector = std::abs (subBand) * subStabilityWeight
                                      + std::abs (bodyBand) * bodyStabilityWeight
                                      + std::abs (psychoHarm) * (0.18f + bassHead * 0.12f + boom * 0.08f);
            const auto centerCoeff = centerDetector > openCenterStabilityEnv ? openAttackCoeff : openReleaseCoeff;
            openCenterStabilityEnv = centerCoeff * openCenterStabilityEnv + (1.0f - centerCoeff) * centerDetector;
            const auto centerOver = smoothStep (juce::jlimit (0.0f, 1.0f,
                (openCenterStabilityEnv - (0.084f + bassHead * 0.014f)) * (5.16f + boom * 0.98f)));
            const auto centerStabilityGain = dbToGain (-centerOver * (1.08f + stabilityDepth * 1.06f));
            const auto centerHarmonicHold = 1.0f - centerOver * (0.09f + bassHead * 0.088f + boom * 0.11f);
            const auto centerSubDetector = std::abs (subBand) * (0.82f + subPriority * 0.2f + bassHead * 0.22f)
                                         + std::abs (psychoHarm) * (0.24f + psychoPriority * 0.16f);
            const auto centerBodyDetector = std::abs (bodyBand) * (0.56f + upperBassPriority * 0.14f + lowMidPriority * 0.1f)
                                          + std::abs (subHarm) * (0.08f + subPriority * 0.05f);
            const auto centerMidDetector = std::abs (bodyBand) * (0.22f + lowMidPriority * 0.12f + midPriority * 0.22f)
                                         + std::abs (subOdd) * (0.08f + boomDrivePriority * 0.24f);
            const auto centerSubCoeff = centerSubDetector > openCenterSubControlEnv ? openAttackCoeff : openReleaseCoeff;
            const auto centerBodyCoeff = centerBodyDetector > openCenterBodyControlEnv ? openAttackCoeff : openReleaseCoeff;
            const auto centerMidCoeff = centerMidDetector > openCenterMidControlEnv ? openAttackCoeff : openReleaseCoeff;
            openCenterSubControlEnv = centerSubCoeff * openCenterSubControlEnv + (1.0f - centerSubCoeff) * centerSubDetector;
            openCenterBodyControlEnv = centerBodyCoeff * openCenterBodyControlEnv + (1.0f - centerBodyCoeff) * centerBodyDetector;
            openCenterMidControlEnv = centerMidCoeff * openCenterMidControlEnv + (1.0f - centerMidCoeff) * centerMidDetector;
            const auto centerSubOver = smoothStep (juce::jlimit (0.0f, 1.0f,
                (openCenterSubControlEnv - (0.052f + bassHead * 0.008f + boom * 0.005f)) * (7.92f + bassHead * 1.92f)));
            const auto centerBodyOver = smoothStep (juce::jlimit (0.0f, 1.0f,
                (openCenterBodyControlEnv - (0.065f + punch * 0.007f + bassHead * 0.003f - lowBodyFocus * 0.014f)) * (6.18f + boom * 1.05f + lowBodyFocus * 0.86f)));
            const auto centerMidOver = smoothStep (juce::jlimit (0.0f, 1.0f,
                (openCenterMidControlEnv - (0.079f + boom * 0.012f + bassHead * 0.009f - lowBodyFocus * 0.012f)) * (4.82f + boom * 1.08f + lowBodyFocus * 0.9f)));
            const auto centerSubHold = 1.0f - centerSubOver * (0.09f + bassHead * 0.225f + boom * 0.025f);
            const auto centerBodyHold = 1.0f - centerBodyOver * (0.08f + punch * 0.098f + boom * 0.068f + lowBodyFocus * 0.08f);
            const auto centerMidHold = 1.0f - centerMidOver * (0.052f + boom * 0.1f + lowBodyFocus * 0.086f);
            const auto centerLayerGain = dbToGain (-(centerSubOver * (0.36f + bassHead * 0.34f)
                                                    + centerBodyOver * (0.23f + punch * 0.125f + boom * 0.07f)
                                                    + centerMidOver * (0.09f + boom * 0.08f)));
            const auto bodyEvenRaw = premiumSaturate (bodyBand * (1.0f + bodyAmountBase * (0.68f + upperBassPriority * 0.2f + lowMidPriority * 0.09f)), bodyDrive, 0.58f) - bodyBand * 0.09f;
            const auto bodyEven = bodyEvenPolish.processLowPass (bodyEvenRaw) * 0.86f
                                + bodyEvenRaw * bodyRawBleed;
            const auto bodyOddRaw = premiumSaturate (bodyBand * (1.0f + bodyAmountBase * (0.82f + lowMidPriority * 0.46f + boomDrivePriority * 0.32f)),
                                                     1.06f + bodyAmountBase * (1.04f + lowMidPriority * 0.45f + boomDrivePriority * 0.38f - lowBodyFocus * 0.22f),
                                                     0.5f + boomDrivePriority * 0.12f + lowBodyFocus * 0.06f) - bodyBand;
            const auto bodyOdd = bodyOddPolish.processLowPass (bodyOddRaw) * (0.84f + punch * 0.04f + boom * 0.04f)
                               + bodyOddRaw * bodyRawBleed;
            const auto midPushRaw = premiumSaturate (bodyBand * (1.0f + bodyAmountBase * (0.42f + lowMidPriority * 0.36f + midPriority * 0.34f)),
                                                     1.035f + punch * 0.22f + boomDrivePriority * 0.52f + midPriority * 0.13f - lowBodyFocus * 0.18f,
                                                     0.52f + boomDrivePriority * 0.08f + lowBodyFocus * 0.05f) - bodyBand;
            const auto midPush = midDrivePolish.processLowPass (midPushRaw) * 0.9f
                               + midPushRaw * midRawBleed;
            const auto punchMass = softClip ((softSaturate (bodyBand + psychoHarm * 0.28f + subHarm * 0.18f,
                                                            1.06f + bodyAmountBase * 0.96f)
                                              - bodyBand * 0.46f) * punch,
                                             0.94f, 0.84f);
            const auto bassHeadBloom = softClip ((psychoHarm * 0.98f + subHarm * 0.08f + subBand * 0.1f) * bassHead, 1.08f, 1.16f);
            const auto boomExciteSource = bodyBand * (0.98f + lowMidPriority * 0.14f + lowBodyFocus * 0.16f)
                                        + bodyOdd * (0.17f + boomDrivePriority * 0.11f + lowBodyFocus * 0.08f)
                                        + midPush * (0.12f + midPriority * 0.06f + lowBodyFocus * 0.04f)
                                        + subHarm * (0.034f + boomDrivePriority * 0.026f)
                                        + psychoHarm * 0.034f;
            const auto boomExciteRaw = (premiumSaturate (boomExciteSource * (1.0f + bodyAmountBase * (0.82f + lowBodyFocus * 0.08f)),
                                                        1.06f + bodyAmountBase * 0.44f + boomDrivePriority * 0.72f - lowBodyFocus * 0.24f,
                                                        0.62f + lowBodyFocus * 0.05f)
                                      - boomExciteSource * 0.56f) * boom;
            const auto boomExcite = softClip (boomPolish.processLowPass (boomExciteRaw) * 0.99f + boomExciteRaw * midRawBleed,
                                              1.19f, 1.42f);
            const auto midSparkRaw = bodyOdd * midExcitement + bodyEven * 0.46f + midPush * (0.54f * boom + 0.14f * punch) + boomExcite * (0.68f + lowBodyFocus * 0.08f);
            const auto midSpark = softClip (midPolish.processLowPass (midSparkRaw) * 0.98f + midSparkRaw * midRawBleed,
                                            1.14f - boom * 0.01f, 1.22f);
            const auto subPath = mid * 0.5f
                               + subBand * ((0.2f + subPriority * 0.16f) * subAmountBase * subWeight * bassHeadDepth)
                               + subHarm * ((0.15f + punch * 0.08f + bassHead * 0.1f + boom * 0.08f) * subAmountBase * centerHarmonicHold * centerSubHold)
                               + psychoHarm * (psychoMix * subAmountBase * bassHeadDepth * centerHarmonicHold * centerSubHold)
                               + bassHeadBloom * ((0.1f + bassHead * 0.105f) * subAmountBase * centerSubHold);
            const auto bodyPath = mid * 0.5f
                                + bodyBand * ((0.27f + upperBassPriority * 0.13f + lowMidPriority * 0.09f) * bodyAmountBase * bodyWeight * punchAuthority)
                                + bodyEven * ((0.14f + upperBassPriority * 0.055f) * bodyAmountBase * centerBodyHold)
                                + punchMass * ((0.095f + punch * 0.085f) * bodyAmountBase * centerHarmonicHold * centerBodyHold)
                                + boomExcite * ((0.095f + boomDrivePriority * 0.13f) * bodyAmountBase * centerHarmonicHold * centerMidHold)
                                + midSpark * ((0.115f + punch * 0.12f + boomDrivePriority * 0.46f) * bodyAmountBase * centerHarmonicHold * centerBodyHold * centerMidHold);
            const auto anchoredLift = (subBand * (0.054f + bassHead * 0.072f + punch * 0.032f)
                                    + bodyBand * (0.058f + punch * 0.068f + boom * 0.07f)
                                    + psychoHarm * (0.016f + bassHead * 0.03f)
                                    + midSpark * boom * 0.03f)
                                    * (subAmountBase * 0.52f + bodyAmountBase * 0.62f)
                                    * (1.0f - centerOver * 0.26f);
            const auto centerLift = ((subPath + bodyPath) - mid + anchoredLift) * centerStabilityGain * centerLayerGain;
            const auto focusedMid = thickSaturate ((mid + centerLift) * dbToGain (-(subAmountBase + bodyAmountBase) * (0.13f + boom * 0.045f)),
                                                   1.035f + bodyAmountBase * 0.32f + punch * 0.18f + boom * 0.1f,
                                                   0.16f + boom * 0.16f);
            const auto controlledMid = softClip (focusedMid, 1.25f - boom * 0.018f - bassHead * 0.026f, 1.12f + bassHead * 0.09f + boom * 0.09f);
            const auto openSideFocus = punchFocus * (1.0f - centerOver * (0.04f + bassHead * 0.07f + boom * 0.05f));
            left[i] = controlledMid + side * openSideFocus;
            right[i] = controlledMid - side * openSideFocus;
        }
    }

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& subLp = subLowPass[static_cast<size_t> (ch % 2)];
        auto& subHp = subHarmonicHighPass[static_cast<size_t> (ch % 2)];
        auto& bodyLp = bodyBandLowPass[static_cast<size_t> (ch % 2)];
        auto& bodyHp = bodyBandHighPass[static_cast<size_t> (ch % 2)];
        auto& deltaPolish = openDeltaPolishLowPass[static_cast<size_t> (ch % 2)];
        auto& subPolish = openSubPolishLowPass[static_cast<size_t> (ch % 2)];
        auto& subEvenPolish = openSubEvenPolishLowPass[static_cast<size_t> (ch % 2)];
        auto& subOddPolish = openSubOddPolishLowPass[static_cast<size_t> (ch % 2)];
        auto& bodyPolish = openBodyPolishLowPass[static_cast<size_t> (ch % 2)];
        auto& bodyEvenPolish = openBodyEvenPolishLowPass[static_cast<size_t> (ch % 2)];
        auto& bodyOddPolish = openBodyOddPolishLowPass[static_cast<size_t> (ch % 2)];
        auto& midPolish = openMidPolishLowPass[static_cast<size_t> (ch % 2)];
        auto& midDrivePolish = openMidDrivePolishLowPass[static_cast<size_t> (ch % 2)];
        auto& boomPolish = openBoomPolishLowPass[static_cast<size_t> (ch % 2)];
        auto& psychoHp = openPsychoHarmonicHighPass[static_cast<size_t> (ch % 2)];
        auto& psychoLp = openPsychoHarmonicLowPass[static_cast<size_t> (ch % 2)];

        subLp.setLowPass (juce::jlimit (45.0f, 220.0f, subFreq * (1.18f + bassHead * 0.17f + punch * 0.06f + boom * 0.02f)));
        subHp.setHighPass (juce::jmax (70.0f, subFreq * 1.25f));
        bodyLp.setLowPass (juce::jlimit (145.0f, 2900.0f, bodyFreq * (2.62f + upperBassPriority * 0.18f + lowMidPriority * 0.26f + midPriority * 0.08f - lowBodyFocus * 0.26f)));
        bodyHp.setHighPass (juce::jmax (32.0f, bodyFreq * (0.31f + bassHead * 0.08f + boom * 0.04f)));
        deltaPolish.setLowPass (harmonicPolishHz * (ch % 2 == 0 ? 0.98f : 1.015f));
        subPolish.setLowPass (subPolishHz * (ch % 2 == 0 ? 0.99f : 1.01f));
        subEvenPolish.setLowPass (subPolishHz * (ch % 2 == 0 ? 0.84f : 0.87f));
        subOddPolish.setLowPass (subPolishHz * (ch % 2 == 0 ? 1.04f : 1.09f));
        bodyPolish.setLowPass (bodyPolishHz * (ch % 2 == 0 ? 0.98f : 1.02f));
        bodyEvenPolish.setLowPass (bodyPolishHz * (ch % 2 == 0 ? 0.85f : 0.9f));
        bodyOddPolish.setLowPass (bodyPolishHz * (ch % 2 == 0 ? 1.01f : 1.07f));
        midPolish.setLowPass (midPolishHz * (ch % 2 == 0 ? 0.97f : 1.035f));
        midDrivePolish.setLowPass (midPolishHz * (ch % 2 == 0 ? 0.92f : 1.015f));
        boomPolish.setLowPass (boomPolishHz * (ch % 2 == 0 ? 0.95f : 1.045f));
        psychoHp.setHighPass (psychoHighPassHz);
        psychoLp.setLowPass (psychoLowPassHz);
        const auto stereoPsychoTrim = buffer.getNumChannels() >= 2 ? 0.58f : 1.0f;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto x = data[i];
            const auto subInput = x * 0.5f;
            const auto bodyInput = x * 0.5f;
            const auto subBand = subLp.processLowPass (subInput);
            const auto subSaturated = premiumSaturate (subBand * (1.025f + subPriority * 0.074f + boomDrivePriority * 0.038f),
                                                       subDrive, 0.66f + bassHead * 0.14f);
            const auto subHarmRaw = subHp.processHighPass (subSaturated);
            const auto subEvenRaw = (subHarmRaw * subHarmRaw * std::copysign (1.0f, subHarmRaw))
                                  * ((0.086f + subPriority * 0.048f + psychoPriority * 0.05f + boomDrivePriority * 0.026f) * subAmountBase);
            const auto subEven = subEvenPolish.processLowPass (subEvenRaw) * (0.96f - subCleanliness * 0.08f)
                               + subEvenRaw * subRawBleed;
            const auto subOddRaw = (premiumSaturate (subHarmRaw * (1.0f + punch * 0.4f + boomDrivePriority * 0.88f),
                                                     1.0f + punch * 0.5f + boomDrivePriority * 1.08f,
                                                     0.58f + bassHead * 0.16f) - subHarmRaw)
                                 * (punch * 0.2f + boomDrivePriority * 0.36f);
            const auto subOdd = subOddPolish.processLowPass (subOddRaw) * antiFizz
                              + subOddRaw * subRawBleed;
            const auto subHarm = softClip (subPolish.processLowPass (subHarmRaw + subEven + subOdd * (0.32f + boom * 0.11f)),
                                           1.2f - boom * 0.03f - bassHead * 0.004f,
                                           1.12f + bassHead * 0.12f);
            const auto psychoHarmRaw = generateOpenPsychoHarmonics (subBand, psychoHp, psychoLp,
                                                                    openPsychoFastEnv[static_cast<size_t> (ch % 2)],
                                                                    openPsychoSlowEnv[static_cast<size_t> (ch % 2)],
                                                                    psychoAmount);
            const auto psychoHarm = subPolish.processLowPass (psychoHarmRaw);
            const auto bodyBand = bodyHp.processHighPass (bodyLp.processLowPass (bodyInput));
            auto& stabilityEnv = openStabilityEnv[static_cast<size_t> (ch % 2)];
            const auto openDetector = std::abs (subBand) * subStabilityWeight
                                    + std::abs (bodyBand) * bodyStabilityWeight
                                    + std::abs (psychoHarm) * (0.2f + bassHead * 0.14f + boom * 0.08f);
            const auto envCoeff = openDetector > stabilityEnv ? openAttackCoeff : openReleaseCoeff;
            stabilityEnv = envCoeff * stabilityEnv + (1.0f - envCoeff) * openDetector;
            const auto openOver = smoothStep (juce::jlimit (0.0f, 1.0f,
                (stabilityEnv - (0.078f + bassHead * 0.014f)) * (5.58f + boom * 0.92f)));
            const auto stabilityGain = dbToGain (-openOver * (0.96f + stabilityDepth * 1.16f));
            const auto harmonicHold = 1.0f - openOver * (0.096f + bassHead * 0.104f + boom * 0.118f);
            const auto driveHold = 1.0f - openOver * (0.068f + boom * 0.062f);
            auto& subControlEnv = openSubControlEnv[static_cast<size_t> (ch % 2)];
            auto& bodyControlEnv = openBodyControlEnv[static_cast<size_t> (ch % 2)];
            auto& midControlEnv = openMidControlEnv[static_cast<size_t> (ch % 2)];
            const auto subLayerDetector = std::abs (subBand) * (0.82f + subPriority * 0.22f + bassHead * 0.22f)
                                        + std::abs (psychoHarm) * (0.22f + psychoPriority * 0.16f)
                                        + std::abs (subHarm) * (0.08f + subPriority * 0.05f);
            const auto bodyLayerDetector = std::abs (bodyBand) * (0.56f + upperBassPriority * 0.14f + lowMidPriority * 0.11f)
                                         + std::abs (subHarm) * (0.075f + subPriority * 0.045f);
            const auto midLayerDetector = std::abs (bodyBand) * (0.22f + lowMidPriority * 0.12f + midPriority * 0.23f)
                                        + std::abs (subOdd) * (0.08f + boomDrivePriority * 0.24f);
            const auto subLayerCoeff = subLayerDetector > subControlEnv ? openAttackCoeff : openReleaseCoeff;
            const auto bodyLayerCoeff = bodyLayerDetector > bodyControlEnv ? openAttackCoeff : openReleaseCoeff;
            const auto midLayerCoeff = midLayerDetector > midControlEnv ? openAttackCoeff : openReleaseCoeff;
            subControlEnv = subLayerCoeff * subControlEnv + (1.0f - subLayerCoeff) * subLayerDetector;
            bodyControlEnv = bodyLayerCoeff * bodyControlEnv + (1.0f - bodyLayerCoeff) * bodyLayerDetector;
            midControlEnv = midLayerCoeff * midControlEnv + (1.0f - midLayerCoeff) * midLayerDetector;
            const auto subLayerOver = smoothStep (juce::jlimit (0.0f, 1.0f,
                (subControlEnv - (0.05f + bassHead * 0.008f + boom * 0.005f)) * (8.06f + bassHead * 2.04f)));
            const auto bodyLayerOver = smoothStep (juce::jlimit (0.0f, 1.0f,
                (bodyControlEnv - (0.064f + punch * 0.007f + bassHead * 0.003f - lowBodyFocus * 0.014f)) * (6.24f + boom * 1.08f + lowBodyFocus * 0.88f)));
            const auto midLayerOver = smoothStep (juce::jlimit (0.0f, 1.0f,
                (midControlEnv - (0.079f + boom * 0.012f + bassHead * 0.009f - lowBodyFocus * 0.012f)) * (4.86f + boom * 1.1f + lowBodyFocus * 0.92f)));
            const auto subLayerHold = 1.0f - subLayerOver * (0.094f + bassHead * 0.23f + boom * 0.024f);
            const auto bodyLayerHold = 1.0f - bodyLayerOver * (0.082f + punch * 0.1f + boom * 0.07f + lowBodyFocus * 0.084f);
            const auto midLayerHold = 1.0f - midLayerOver * (0.054f + boom * 0.104f + lowBodyFocus * 0.088f);
            const auto layeredDriveHold = driveHold * (1.0f - midLayerOver * (0.018f + boom * 0.03f));
            const auto layerTrim = dbToGain (-(subLayerOver * (0.34f + bassHead * 0.36f)
                                              + bodyLayerOver * (0.23f + punch * 0.125f + boom * 0.07f)
                                              + midLayerOver * (0.09f + boom * 0.08f)));
            const auto bodyThick = premiumSaturate (bodyBand * (1.035f + bodyAmountBase * (0.64f + upperBassPriority * 0.2f + lowMidPriority * 0.09f)),
                                                    bodyDrive * (1.0f - boom * 0.1f) * layeredDriveHold,
                                                    0.56f + boom * 0.08f + lowBodyFocus * 0.04f);
            const auto bodyOddRaw = premiumSaturate (bodyBand * (1.0f + bodyAmountBase * (0.8f + lowMidPriority * 0.46f + boomDrivePriority * 0.31f)),
                                                    1.05f + bodyAmountBase * (1.02f + lowMidPriority * 0.46f + boomDrivePriority * 0.37f - lowBodyFocus * 0.22f),
                                                    0.5f + boomDrivePriority * 0.12f + lowBodyFocus * 0.06f) - bodyBand;
            const auto bodyOdd = bodyOddPolish.processLowPass (bodyOddRaw) * (0.84f + punch * 0.04f + boom * 0.04f)
                               + bodyOddRaw * bodyRawBleed;
            const auto midGrowlRaw = premiumSaturate (bodyBand * (1.0f + boomDrivePriority * 0.88f + punch * 0.62f + lowBodyFocus * 0.06f),
                                                     1.045f + boomDrivePriority * 0.66f + punch * 0.46f - lowBodyFocus * 0.18f,
                                                     0.56f + boomDrivePriority * 0.08f + lowBodyFocus * 0.04f) - bodyBand;
            const auto midGrowl = midDrivePolish.processLowPass (midGrowlRaw) * (0.9f + boom * 0.04f)
                                + midGrowlRaw * midRawBleed;
            const auto punchMass = softClip ((softSaturate (bodyBand + psychoHarm * 0.3f + subHarm * 0.2f,
                                                            1.06f + bodyAmountBase * 1.0f)
                                              - bodyBand * 0.46f) * punch,
                                             0.94f, 0.84f);
            const auto bassHeadBloom = softClip ((psychoHarm * 0.98f + subHarm * 0.08f + subBand * 0.1f) * bassHead, 1.08f, 1.16f);
            const auto boomClipSource = bodyBand * (0.98f + lowMidPriority * 0.14f + lowBodyFocus * 0.16f)
                                      + bodyOdd * (0.2f + boomDrivePriority * 0.12f + lowBodyFocus * 0.08f)
                                      + midGrowl * (0.14f + midPriority * 0.06f + lowBodyFocus * 0.04f)
                                      + subHarm * (0.034f + boomDrivePriority * 0.026f)
                                      + psychoHarm * 0.034f;
            const auto boomClipRaw = (premiumSaturate (boomClipSource * (1.0f + bodyAmountBase * (0.84f + lowBodyFocus * 0.08f)),
                                                      1.065f + bodyAmountBase * 0.44f + boomDrivePriority * 0.72f - lowBodyFocus * 0.24f,
                                                      0.62f + lowBodyFocus * 0.05f)
                                    - boomClipSource * 0.56f) * boom;
            const auto boomClip = softClip (boomPolish.processLowPass (boomClipRaw) * 0.99f + boomClipRaw * midRawBleed,
                                            1.19f, 1.42f);
            const auto bodyHarmRaw = bodyThick - bodyBand * 0.05f
                                   + bodyOdd * (0.24f + upperBassPriority * 0.07f + lowMidPriority * 0.08f)
                                   + midGrowl * (0.12f * punch + 0.36f * boomDrivePriority)
                                   + punchMass * 0.22f + boomClip * (0.16f + boomDrivePriority * 0.1f);
            const auto bodyHarm = softClip (bodyPolish.processLowPass (bodyHarmRaw) * 0.93f + bodyHarmRaw * bodyRawBleed,
                                            1.27f - boom * 0.01f, 1.24f);

            const auto subPath = subInput
                               + subBand * ((0.19f + subPriority * 0.17f) * subAmountBase * subWeight * bassHeadDepth)
                               + subHarm * ((0.145f + punch * 0.08f + bassHead * 0.1f + boom * 0.08f) * subAmountBase * harmonicHold * subLayerHold)
                               + psychoHarm * (psychoMix * subAmountBase * stereoPsychoTrim * bassHeadDepth * harmonicHold * subLayerHold)
                               + bassHeadBloom * ((0.1f + bassHead * 0.105f) * subAmountBase * harmonicHold * subLayerHold);
            const auto bodyPath = bodyInput
                                + bodyBand * ((0.27f + upperBassPriority * 0.14f + lowMidPriority * 0.09f) * bodyAmountBase * bodyWeight * punchAuthority)
                                + bodyHarm * ((0.155f + punch * 0.14f + boomDrivePriority * 0.42f) * bodyAmountBase * harmonicHold * bodyLayerHold)
                                + midGrowl * ((0.05f + boomDrivePriority * 0.11f) * bodyAmountBase * midLayerHold);
            const auto transientForward = (x - softSaturate (x, 1.02f)) * (0.112f * punch + 0.064f * boom);
            const auto anchoredLift = (subBand * (0.048f + bassHead * 0.076f + punch * 0.034f)
                                    + bodyBand * (0.06f + punch * 0.074f + boom * 0.074f)
                                    + psychoHarm * (0.014f + bassHead * 0.032f)
                                    + boomClip * boom * 0.036f)
                                    * (subAmountBase * 0.52f + bodyAmountBase * 0.64f)
                                    * (1.0f - openOver * 0.25f);
            const auto polished = (subPath + bodyPath + anchoredLift
                               + bodyBand * bodyAmountBase * (0.058f + punch * 0.09f + boom * 0.082f)
                               + boomClip * (0.126f * boom * bodyAmountBase * harmonicHold + 0.032f * bodyAmountBase * harmonicHold)) * stabilityGain
                               * layerTrim
                               + transientForward * (1.0f - openOver * 0.18f) * (1.0f - midLayerOver * 0.08f);
            const auto driven = premiumSaturate (polished * dbToGain (openDensity * (0.34f + punch * 0.14f + boom * 0.16f)),
                                                 1.04f + bodyAmountBase * 0.34f + punch * 0.22f + boom * 0.16f,
                                                 0.54f + boom * 0.08f + bassHead * 0.08f);
            const auto controlled = softClip (driven, 1.3f - boom * 0.01f - bassHead * 0.045f,
                                              1.24f + bassHead * 0.08f + boom * 0.1f);
            data[i] = juce::jlimit (-3.2f, 3.2f, controlled * dbToGain (-openDensity * (0.44f + harmonicControl)));
        }
    }
}

void SuperBassAudioProcessor::processSpace (juce::AudioBuffer<float>& buffer)
{
    const auto channels = buffer.getNumChannels();
    const auto samples = buffer.getNumSamples();
    depthSmooth.setTarget (getFloatParam (apvts, "spaceDepth") / 100.0f);
    depthDistanceSmooth.setTarget (getFloatParam (apvts, "spaceDepthDistance") / 100.0f);
    wideSmooth.setTarget (getFloatParam (apvts, "spaceWide") / 100.0f);
    wideFreqSmooth.setTarget (getFloatParam (apvts, "spaceWideFreq"));
    wideRateSmooth.setTarget (getFloatParam (apvts, "spaceWideRate"));
    diffusorFreqSmooth.setTarget (getFloatParam (apvts, "spaceDiffusorHz"));
    const auto wideMode = getChoiceParam (apvts, "spaceWideMode");
    const auto diffusorEnabled = getBoolParam (apvts, "spaceAllPassEnabled");

    for (int i = 0; i < samples; ++i)
    {
        const auto depth = depthSmooth.getNext();
        const auto distance = smoothStep (depthDistanceSmooth.getNext());
        const auto distanceMs = 3.8f + distance * 30.0f;
        const auto feedback = juce::jlimit (0.025f, 0.198f, 0.032f + distance * 0.148f + depth * 0.034f);
        const auto earlyLayer = depth * (0.54f + distance * 0.22f);
        const auto diffusionLayer = depth * distance * 0.58f;
        const auto wetGain = depth * (0.35f + distance * 0.56f);
        const auto dryDistance = 1.0f - depth * (0.035f + distance * 0.2f);
        const auto reflectionHp = 135.0f + distance * 155.0f;
        const int delaySamples = juce::jlimit (1, delayBuffer.getNumSamples() - 1,
            static_cast<int> (currentSampleRate * distanceMs / 1000.0f));
        const int readPosition = (delayWritePosition + delayBuffer.getNumSamples() - delaySamples) % delayBuffer.getNumSamples();

        for (int ch = 0; ch < channels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            auto& hp = depthHighPass[static_cast<size_t> (ch % 2)];
            hp.setHighPass (reflectionHp);
            auto filteredSend = hp.processHighPass (data[i]) * (1.0f - depth * (0.09f + distance * 0.17f));
            const auto delayed = delayBuffer.getSample (ch, readPosition);
            float early = 0.0f;
            for (int tap = 0; tap < 8; ++tap)
            {
                const auto tapBaseMs = 0.85f + static_cast<float> (tap) * (1.42f + distance * 2.72f);
                const auto offset = juce::jlimit (1, delayBuffer.getNumSamples() - 1,
                    static_cast<int> (currentSampleRate * tapBaseMs * 0.001f));
                const auto pos = (delayWritePosition + delayBuffer.getNumSamples() - offset) % delayBuffer.getNumSamples();
                const auto tapSign = ((tap + ch) % 2 == 0) ? 1.0f : -1.0f;
                const auto earlyShape = tap < 3 ? (1.0f + earlyLayer * 0.08f)
                                                : (0.9f - static_cast<float> (tap - 3) * 0.052f);
                const auto spreadShape = 1.0f + std::sin (static_cast<float> (tap + 1) * 1.37f + static_cast<float> (ch) * 0.61f)
                                               * (0.025f + distance * 0.032f);
                const auto weight = (0.24f - static_cast<float> (tap) * 0.0115f)
                                  * earlyShape * spreadShape * (0.83f + distance * 0.32f);
                early += delayBuffer.getSample (ch, pos) * tapSign * weight;
            }

            const auto diffusedFeedback = delayed * feedback * (0.88f + diffusionLayer * 0.16f);
            delayBuffer.setSample (ch, delayWritePosition, filteredSend + diffusedFeedback);
            const auto reflectionBody = delayed * (0.075f + distance * 0.072f + diffusionLayer * 0.028f);
            const auto depthTone = softSaturate ((reflectionBody + early * (0.94f + earlyLayer * 0.08f)) * (0.82f + distance * 0.14f),
                                                 1.008f + depth * 0.052f);
            const auto distanceDarkening = 1.0f - depth * distance * 0.102f;
            data[i] = data[i] * dryDistance * distanceDarkening + depthTone * wetGain;
        }

        delayWritePosition = (delayWritePosition + 1) % delayBuffer.getNumSamples();
    }

    if (diffusorEnabled)
    {
        const auto depthContribution = getFloatParam (apvts, "spaceDepth") / 100.0f;
        const auto wideContribution = getFloatParam (apvts, "spaceWide") / 100.0f;
        const auto diffusorMix = juce::jlimit (0.1f, 0.31f, 0.15f + depthContribution * 0.1f + wideContribution * 0.045f);

        for (int i = 0; i < samples; ++i)
        {
            const auto freq = diffusorFreqSmooth.getNext();
            for (int ch = 0; ch < channels; ++ch)
            {
                auto* data = buffer.getWritePointer (ch);
                const auto idx = static_cast<size_t> (ch % 2);
                auto& bright = diffusorAllPass[idx];
                auto& warm = diffusorWarmAllPass[idx];
                bright.setFrequency (juce::jlimit (90.0f, 18000.0f, freq * (ch % 2 == 0 ? 0.86f : 1.18f)));
                warm.setFrequency (juce::jlimit (70.0f, 12000.0f, freq * (ch % 2 == 0 ? 0.3f : 0.48f)));
                const auto dry = data[i];
                const auto stageA = bright.process (dry);
                const auto stageB = warm.process (stageA * 0.74f + dry * 0.26f);
                const auto crossStage = warm.process (bright.process (stageB * 0.42f + dry * 0.58f));
                const auto denseStage = bright.process (crossStage * 0.34f + stageA * 0.31f + dry * 0.35f);
                const auto diffused = stageB * 0.5f + stageA * 0.2f + crossStage * 0.18f + denseStage * 0.12f;
                data[i] += (softSaturate (diffused, 1.005f + depthContribution * 0.027f) - dry) * diffusorMix;
            }
        }
    }

    if (channels >= 2)
    {
        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);
        const auto modeDelayMs = wideMode == 0 ? 12.4f : (wideMode == 1 ? 4.65f : 2.35f);
        wideDelaySmooth.setTarget (static_cast<float> (currentSampleRate * modeDelayMs / 1000.0));

        for (int i = 0; i < samples; ++i)
        {
            const auto wide = wideSmooth.getNext();
            const auto wideFreq = wideFreqSmooth.getNext();
            const auto rate = wideRateSmooth.getNext();
            const auto sideCutoff = juce::jlimit (105.0f, 9000.0f, wideFreq);
            sideHighPass[0].setHighPass (sideCutoff);
            sideHighPass[1].setHighPass (sideCutoff * 1.06f);

            const auto lfo = std::sin (wideModPhase);
            const auto lfo2 = std::sin (wideModPhase * 0.51f + 1.33f);
            const auto lfo3 = std::sin (wideModPhase * 1.37f + 0.55f);
            const auto organicLfo = lfo * 0.62f + lfo2 * 0.25f + lfo3 * 0.13f;
            const auto baseRate = wideMode == 1 ? 0.15f : (wideMode == 2 ? 0.083f : 0.038f);
            const auto rateScale = wideMode == 0 ? (0.55f + rate * 0.55f)
                                                  : (0.38f + rate * 0.82f);
            wideModPhase += 2.0f * pi * baseRate * rateScale / static_cast<float> (currentSampleRate);
            if (wideModPhase > 2.0f * pi)
                wideModPhase -= 2.0f * pi;

            const auto mid = 0.5f * (left[i] + right[i]);
            const auto sideRaw = 0.5f * (left[i] - right[i]);
            const auto inputL = left[i];
            const auto inputR = right[i];
            const auto delayBase = wideDelaySmooth.getNext();
            const auto movementDepth = wideMode == 0 ? (0.42f + rate * 0.06f)
                                                     : (wideMode == 1 ? (0.78f + rate * 0.18f) : (0.74f + rate * 0.14f));
            const auto sweep = (wideMode == 0 ? 0.14f : (wideMode == 1 ? 4.6f : 1.35f)) * movementDepth * (organicLfo + 1.0f);
            const auto delayedL = interpolateDelay (wideDelayBuffer, 0, delayBase + sweep, wideDelayWritePosition);
            const auto delayedR = interpolateDelay (wideDelayBuffer, 1, delayBase + sweep * 1.21f + 1.65f, wideDelayWritePosition);

            const auto phaseSweep = smoothStep (0.5f + 0.5f * organicLfo);
            const auto phaseSweepAlt = smoothStep (0.5f + 0.5f * std::sin (wideModPhase * 0.79f + 2.1f));
            const auto phaserLHz = juce::jlimit (105.0f, 11500.0f, wideFreq * (0.36f + 1.08f * phaseSweep));
            const auto phaserRHz = juce::jlimit (105.0f, 11500.0f, wideFreq * (0.54f + 0.92f * (1.0f - phaseSweepAlt)));
            phaserAllPass[0].setFrequency (phaserLHz);
            phaserAllPass[1].setFrequency (phaserRHz);
            phaserWarmAllPass[0].setFrequency (juce::jlimit (95.0f, 9000.0f, phaserLHz * (0.32f + phaseSweepAlt * 0.12f)));
            phaserWarmAllPass[1].setFrequency (juce::jlimit (95.0f, 9000.0f, phaserRHz * (0.36f + phaseSweep * 0.1f)));
            const auto phaseMid = softSaturate (mid, 1.006f + wide * 0.035f);
            const auto phaseL = phaserWarmAllPass[0].process (phaserAllPass[0].process (phaseMid));
            const auto phaseR = phaserWarmAllPass[1].process (phaserAllPass[1].process (phaseMid));
            const auto phaseTexture = (phaseL - phaseR) + (phaserAllPass[0].process (phaseR * 0.42f) - phaserAllPass[1].process (phaseL * 0.42f)) * 0.18f;

            float decorrelated = 0.0f;
            if (wideMode == 0)
                decorrelated = ((delayedR - delayedL) * 0.4f + (delayedR - inputL - delayedL + inputR) * 0.09f);
            else if (wideMode == 1)
                decorrelated = ((delayedL - inputR) - (delayedR - inputL)) * 0.52f + phaseTexture * 0.18f;
            else
                decorrelated = phaseTexture * 0.88f + (delayedR - delayedL) * 0.1f + sideRaw * 0.08f;

            const auto highSide = sideHighPass[0].processHighPass (sideRaw);
            const auto lowSide = sideRaw - highSide;
            const auto highDecorrelated = sideHighPass[1].processHighPass (decorrelated);
            const auto lowGuard = wideMode == 0 ? 0.82f : (wideMode == 1 ? 0.72f : 0.74f);
            const auto decorGain = wideMode == 0 ? 0.84f : (wideMode == 1 ? 0.96f : 1.02f);
            const auto side = juce::jlimit (-0.78f, 0.78f,
                lowSide * (1.0f - wide * (1.0f - lowGuard))
                + highSide * (1.0f + wide * (wideMode == 0 ? 0.84f : 0.72f))
                + highDecorrelated * wide * decorGain);
            const auto monoGuard = 1.0f - wide * (wideMode == 0 ? 0.018f : 0.04f);
            const auto newL = mid * monoGuard + side;
            const auto newR = mid * monoGuard - side;
            const auto mix = juce::jlimit (0.0f, 0.88f, wide * (wideMode == 0 ? 1.04f : 0.98f));
            left[i] = inputL + (juce::jlimit (-1.75f, 1.75f, newL) - inputL) * mix;
            right[i] = inputR + (juce::jlimit (-1.75f, 1.75f, newR) - inputR) * mix;

            wideDelayBuffer.setSample (0, wideDelayWritePosition, inputL);
            wideDelayBuffer.setSample (1, wideDelayWritePosition, inputR);
            wideDelayWritePosition = (wideDelayWritePosition + 1) % wideDelayBuffer.getNumSamples();
        }
    }
}

void SuperBassAudioProcessor::processMaster (juce::AudioBuffer<float>& buffer)
{
    const auto channels = buffer.getNumChannels();
    const auto samples = buffer.getNumSamples();
    blockGainReductionDb = 0.0f;
    masterClipperSmooth.setTarget (getFloatParam (apvts, "masterClipper") / 100.0f);
    masterCompressionSmooth.setTarget (getFloatParam (apvts, "masterCompression") / 100.0f);
    masterThresholdSmooth.setTarget (getFloatParam (apvts, "masterCompThreshold"));
    masterAttackSmooth.setTarget (getFloatParam (apvts, "masterCompAttack"));
    masterReleaseSmooth.setTarget (getFloatParam (apvts, "masterCompRelease"));
    transientAttackSmooth.setTarget (getFloatParam (apvts, "masterTransientAttack") / 100.0f);
    transientReleaseSmooth.setTarget (getFloatParam (apvts, "masterTransientRelease") / 100.0f);
    transientMixSmooth.setTarget (getFloatParam (apvts, "masterTransientMix") / 100.0f);
    const auto clipper = masterClipperSmooth.getNext (samples);
    const auto compression = masterCompressionSmooth.getNext (samples);
    const auto compThreshold = masterThresholdSmooth.getNext (samples);
    const auto compAttackMs = masterAttackSmooth.getNext (samples);
    const auto compReleaseMs = masterReleaseSmooth.getNext (samples);
    const auto transientAttack = transientAttackSmooth.getNext (samples);
    const auto transientRelease = transientReleaseSmooth.getNext (samples);
    const auto transientMix = transientMixSmooth.getNext (samples);
    const auto ratio = 5.2f + compression * 4.4f;
    const auto thresholdDb = -24.0f + compression * 8.2f + compThreshold;
    const auto adjustedAttackMs = juce::jlimit (0.28f, 42.0f, compAttackMs * 0.34f + 0.36f);
    const auto releaseNorm = juce::jlimit (0.0f, 1.0f, (compReleaseMs - 30.0f) / 1970.0f);
    const auto adjustedReleaseMs = juce::jlimit (22.0f, 2000.0f, 24.0f + std::pow (releaseNorm, 0.76f) * 1976.0f);
    const auto compAttackCoeff = std::exp (-1.0f / static_cast<float> (currentSampleRate * adjustedAttackMs * 0.001f));
    const auto compReleaseCoeff = std::exp (-1.0f / static_cast<float> (currentSampleRate * adjustedReleaseMs * 0.001f));
    const auto transFastCoeff = std::exp (-1.0f / static_cast<float> (currentSampleRate * 0.0032f));
    const auto sustainMs = 58.0f + (transientRelease + 1.0f) * 86.0f;
    const auto transSlowCoeff = std::exp (-1.0f / static_cast<float> (currentSampleRate * sustainMs * 0.001f));
    const auto clipPeakAttackCoeff = std::exp (-1.0f / static_cast<float> (currentSampleRate * 0.0014f));
    const auto clipPeakReleaseCoeff = std::exp (-1.0f / static_cast<float> (currentSampleRate * 0.045f));
    const auto clipBodyAttackCoeff = std::exp (-1.0f / static_cast<float> (currentSampleRate * 0.012f));
    const auto clipBodyReleaseCoeff = std::exp (-1.0f / static_cast<float> (currentSampleRate * 0.16f));
    const auto preClipGain = dbToGain (clipper * 1.5f);
    const auto postClipGain = dbToGain (compression * 0.1f - clipper * 0.72f);
    const auto threshold = 1.0f - clipper * 0.04f;
    const auto softness = 0.98f + (1.0f - clipper) * 0.34f;
    const auto eqCharacter = juce::jlimit (0.0f, 1.0f,
        (std::abs (getFloatParam (apvts, "eqLow"))
       + std::abs (getFloatParam (apvts, "eqLowMid"))
       + std::abs (getFloatParam (apvts, "eqMid"))
       + std::abs (getFloatParam (apvts, "eqHighMid"))
       + std::abs (getFloatParam (apvts, "eqHigh"))) / 40.0f);

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto channel = static_cast<size_t> (ch % 2);
        masterDetector[channel].setLowPass (145.0f);

        for (int i = 0; i < samples; ++i)
        {
            auto x = data[i];

            const auto detector = std::abs (x);
            const auto envCoeff = detector > compressorEnv[channel] ? compAttackCoeff : compReleaseCoeff;
            compressorEnv[channel] = envCoeff * compressorEnv[channel] + (1.0f - envCoeff) * detector;
            const auto envDb = juce::Decibels::gainToDecibels (juce::jmax (compressorEnv[channel], 0.000001f));
            const auto overDb = envDb - thresholdDb;
            float gainReductionDb = 0.0f;
            if (overDb > -5.0f)
            {
                const auto knee = 5.0f;
                const auto kneeOver = overDb < knee ? (overDb + knee) * (overDb + knee) / (4.0f * knee)
                                                    : overDb;
                gainReductionDb = (kneeOver / ratio) - kneeOver;
            }
            blockGainReductionDb = juce::jmin (blockGainReductionDb, gainReductionDb * compression);
            const auto transientLift = juce::jlimit (0.0f, 0.22f, (std::abs (x) - compressorEnv[channel]) * (1.9f + compression * 1.15f));
            const auto punchyCompression = gainReductionDb * compression * (0.43f - transientLift * 0.18f);
            const auto compBlend = juce::jlimit (0.0f, 0.74f, compression * (0.78f - releaseNorm * 0.1f));
            const auto compressed = x * dbToGain (punchyCompression);
            x += (compressed - x) * compBlend;
            x += (premiumSaturate (x, 1.025f + compression * 0.18f, 0.62f) - x) * (compression * 0.028f);

            const auto beforeTransient = x;
            const auto transientDetector = std::abs (x);
            transientFastEnv[channel] = transFastCoeff * transientFastEnv[channel] + (1.0f - transFastCoeff) * transientDetector;
            transientSlowEnv[channel] = transSlowCoeff * transientSlowEnv[channel] + (1.0f - transSlowCoeff) * transientDetector;
            const auto transientDelta = juce::jlimit (-0.42f, 0.42f, transientFastEnv[channel] - transientSlowEnv[channel]);
            const auto attackGain = juce::jlimit (0.72f, 1.32f, 1.0f + transientAttack * transientDelta * 2.25f);
            const auto sustainDelta = juce::jlimit (-0.35f, 0.35f, transientSlowEnv[channel] - transientFastEnv[channel]);
            const auto sustainGain = juce::jlimit (0.78f, 1.26f, 1.0f + transientRelease * sustainDelta * 1.95f);
            const auto transientShaped = x * attackGain * sustainGain;
            x = beforeTransient + (transientShaped - beforeTransient) * transientMix;

            x = eqLow[channel].process (x);
            x = eqLowMid[channel].process (x);
            x = eqMid[channel].process (x);
            x = eqHighMid[channel].process (x);
            x = eqHigh[channel].process (x);
            x += (premiumSaturate (x, 1.008f + eqCharacter * 0.13f, 0.58f) - x) * (0.016f + eqCharacter * 0.03f);

            const auto lowBeforeClip = masterDetector[channel].processLowPass (x);
            const auto highBeforeClip = x - lowBeforeClip;
            const auto transientEdge = juce::jlimit (-0.58f, 0.58f, highBeforeClip - softSaturate (highBeforeClip, 1.018f));
            const auto clipMagnitude = std::abs (x * preClipGain);
            const auto bodyMagnitude = std::abs (lowBeforeClip * preClipGain);
            const auto peakCoeff = clipMagnitude > masterClipPeakEnv[channel] ? clipPeakAttackCoeff : clipPeakReleaseCoeff;
            const auto bodyCoeff = bodyMagnitude > masterClipBodyEnv[channel] ? clipBodyAttackCoeff : clipBodyReleaseCoeff;
            masterClipPeakEnv[channel] = peakCoeff * masterClipPeakEnv[channel] + (1.0f - peakCoeff) * clipMagnitude;
            masterClipBodyEnv[channel] = bodyCoeff * masterClipBodyEnv[channel] + (1.0f - bodyCoeff) * bodyMagnitude;
            const auto peakProximity = smoothStep (juce::jlimit (0.0f, 1.0f, (masterClipPeakEnv[channel] - 0.46f) / 0.72f));
            const auto bodyProximity = smoothStep (juce::jlimit (0.0f, 1.0f, (masterClipBodyEnv[channel] - 0.31f) / 0.64f));
            const auto clipProximity = smoothStep (juce::jlimit (0.0f, 1.0f, (clipMagnitude - 0.43f) / 0.7f));
            const auto peakDriveTrim = dbToGain (-clipper * peakProximity * 0.5f);
            const auto bodyHold = dbToGain (-clipper * bodyProximity * 0.36f);
            const auto glueLift = dbToGain (clipper * 0.28f * (1.0f - peakProximity * 0.72f) * (1.0f - bodyProximity * 0.48f));
            const auto goldLift = dbToGain (clipper * 1.18f * (1.0f - clipProximity) * (1.0f - peakProximity * 0.42f));
            const auto alchemyTone = 1.0f - clipper * (0.07f + clipProximity * 0.12f + peakProximity * 0.044f);
            const auto goldInput = (lowBeforeClip * (1.0f + (goldLift - 1.0f) * 0.45f)
                                  + highBeforeClip * (1.0f + (goldLift - 1.0f) * 0.2f) * alchemyTone) * preClipGain * peakDriveTrim * glueLift;
            const auto clipInput = x * preClipGain * peakDriveTrim;
            const auto lowInput = lowBeforeClip * preClipGain * bodyHold * (1.0f + (goldLift - 1.0f) * 0.25f);
            const auto highInput = highBeforeClip * preClipGain * peakDriveTrim * alchemyTone;
            const auto clippedHighA = softClip (highInput, threshold, softness);
            const auto clippedHighB = softClip (clippedHighA, 1.012f - clipper * 0.012f, 1.18f);
            const auto clippedFullA = softClip (goldInput, threshold + clipper * 0.034f, softness * 1.34f);
            const auto clippedFullB = softClip (clippedFullA, 1.075f - clipper * 0.024f, 1.34f);
            const auto transientBlend = juce::jlimit (0.0f, 0.34f, 0.16f + clipper * 0.12f - peakProximity * 0.035f);
            const auto lowControl = softClip (lowInput, 1.17f - clipper * 0.026f, 1.58f + bodyProximity * 0.1f);
            const auto lowHold = softClip (lowControl, 1.28f - clipper * 0.028f, 1.72f);
            const auto lowPreserve = lowInput + (lowHold - lowInput) * (0.18f + clipper * 0.14f + bodyProximity * 0.06f);
            const auto transientRestore = transientEdge * clipper * (0.28f + clipProximity * 0.052f) * (1.0f - peakProximity * 0.18f);
            const auto dryPeakReturn = clipInput * clipper * (0.046f + (1.0f - clipProximity) * 0.022f) * (1.0f - peakProximity * 0.24f);
            const auto densityHold = (clippedFullB - goldInput) * clipper * (0.038f + clipProximity * 0.024f + bodyProximity * 0.016f);
            const auto glueStage = premiumSaturate (lowPreserve + clippedHighB + densityHold, 1.004f + clipper * 0.034f, 0.64f)
                                 - (lowPreserve + clippedHighB + densityHold);
            x = (lowPreserve + clippedHighB) * (1.0f - transientBlend)
                + clippedFullB * 0.24f
                + clipInput * (transientBlend * 0.76f)
                + dryPeakReturn
                + densityHold
                + glueStage * (clipper * 0.18f)
                + transientRestore;
            x *= postClipGain;
            data[i] = juce::jlimit (-1.3f, 1.3f, x);
        }
    }
}

void SuperBassAudioProcessor::processSmile (juce::AudioBuffer<float>& buffer)
{
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto amount = juce::jlimit (0.0f, 1.0f, smileSmooth.getNext());

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* wet = buffer.getWritePointer (ch);
            const auto* dry = alignedDryBuffer.getReadPointer (ch);
            wet[i] = dry[i] + (wet[i] - dry[i]) * amount;
        }
    }
}

float SuperBassAudioProcessor::measureRms (const juce::AudioBuffer<float>& buffer) const
{
    double sumSquares = 0.0;
    int count = 0;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const auto* data = buffer.getReadPointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto sample = static_cast<double> (data[i]);
            sumSquares += sample * sample;
            ++count;
        }
    }

    if (count == 0)
        return 0.0f;

    return static_cast<float> (std::sqrt (sumSquares / static_cast<double> (count)));
}

float SuperBassAudioProcessor::measurePerceivedLevel (const juce::AudioBuffer<float>& buffer) const
{
    double sumSquares = 0.0;
    double sumAbs = 0.0;
    double peak = 0.0;
    int count = 0;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const auto* data = buffer.getReadPointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto sample = static_cast<double> (data[i]);
            const auto absSample = std::abs (sample);
            sumSquares += sample * sample;
            sumAbs += absSample;
            peak = std::max (peak, absSample);
            ++count;
        }
    }

    if (count == 0)
        return 0.0f;

    const auto rms = std::sqrt (sumSquares / static_cast<double> (count));
    const auto averageAbs = sumAbs / static_cast<double> (count);
    return static_cast<float> (rms * 0.72 + averageAbs * 0.2 + peak * 0.08);
}

void SuperBassAudioProcessor::updateLevelMeter (const juce::AudioBuffer<float>& buffer, float& smoother,
                                                std::atomic<float>& target)
{
    const auto rms = measureRms (buffer);
    const auto db = juce::jlimit (-60.0f, 6.0f, juce::Decibels::gainToDecibels (juce::jmax (rms, 0.000001f)));
    const auto coefficient = db > smoother ? 0.34f : 0.075f;
    smoother += (db - smoother) * coefficient;
    target.store (smoother);
}

void SuperBassAudioProcessor::updateGainReductionMeter()
{
    const auto db = juce::jlimit (-20.0f, 0.0f, blockGainReductionDb);
    const auto coefficient = db < gainReductionMeterSmooth ? 0.34f : 0.065f;
    gainReductionMeterSmooth += (db - gainReductionMeterSmooth) * coefficient;
    gainReductionMeterDb.store (gainReductionMeterSmooth);
}

void SuperBassAudioProcessor::applyModuleLevelMatch (juce::AudioBuffer<float>& buffer, float beforeRms,
                                                     SmoothValue& smoother, float minGain, float maxGain)
{
    const auto afterRms = measurePerceivedLevel (buffer);
    float targetGain = 1.0f;

    if (beforeRms > 0.00008f && afterRms > 0.00008f)
    {
        const auto rawGain = beforeRms / afterRms;
        const auto partialCompensation = 1.0f + (rawGain - 1.0f) * 0.86f;
        targetGain = juce::jlimit (minGain, maxGain, partialCompensation);
    }

    smoother.setTarget (targetGain);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto gain = smoother.getNext();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.getWritePointer (ch)[i] *= gain;
    }
}

void SuperBassAudioProcessor::applyWetLevelMatch (juce::AudioBuffer<float>& buffer)
{
    const auto dryRms = measurePerceivedLevel (dryBuffer);
    const auto wetRms = measurePerceivedLevel (buffer);

    float targetGain = 1.0f;
    if (dryRms > 0.00008f && wetRms > 0.00008f)
    {
        const auto rawGain = dryRms / wetRms;
        targetGain = juce::jlimit (0.58f, 1.2f, 1.0f + (rawGain - 1.0f) * 0.88f);
    }

    const auto smile = getFloatParam (apvts, "smile") / 100.0f;
    const auto openDrive = (getFloatParam (apvts, "openSub") + getFloatParam (apvts, "openBody")) / 200.0f;
    const auto openMode = getChoiceParam (apvts, "openMode");
    const auto modeDrive = openMode == 1 ? 0.14f : (openMode == 2 ? 0.28f : 0.08f);
    const auto masterDrive = (getFloatParam (apvts, "masterCompression") + getFloatParam (apvts, "masterClipper")) / 200.0f;
    const auto enhancementTrim = dbToGain (-(openDrive * (0.68f + modeDrive) + masterDrive * 0.42f) * smile);
    targetGain = juce::jlimit (0.58f, 1.18f, targetGain * enhancementTrim);

    wetLevelSmooth.setTarget (targetGain);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto gain = wetLevelSmooth.getNext();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.getWritePointer (ch)[i] *= gain;
    }
}

void SuperBassAudioProcessor::applyFinalLevelMatch (juce::AudioBuffer<float>& buffer)
{
    const auto dryLevel = measurePerceivedLevel (dryBuffer);
    const auto processedLevel = measurePerceivedLevel (buffer);

    float targetGain = 1.0f;
    if (dryLevel > 0.00008f && processedLevel > 0.00008f)
    {
        const auto rawGain = dryLevel / processedLevel;
        const auto smile = juce::jlimit (0.0f, 1.0f, getFloatParam (apvts, "smile") / 100.0f);
        const auto correctionDepth = 0.58f + smile * 0.3f;
        targetGain = 1.0f + (rawGain - 1.0f) * correctionDepth;
        targetGain = juce::jlimit (0.62f, 1.14f, targetGain);
    }

    finalLevelSmooth.setTarget (targetGain);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto gain = finalLevelSmooth.getNext();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.getWritePointer (ch)[i] *= gain;
    }
}

void SuperBassAudioProcessor::applyOutputGain (juce::AudioBuffer<float>& buffer)
{
    outputGainSmooth.setTarget (dbToGain (getFloatParam (apvts, "output")));
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto gain = outputGainSmooth.getNext();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.getWritePointer (ch)[i] *= gain;
    }
}

void SuperBassAudioProcessor::updateFilters()
{
    const auto low = getFloatParam (apvts, "eqLow");
    const auto lowMid = getFloatParam (apvts, "eqLowMid");
    const auto mid = getFloatParam (apvts, "eqMid");
    const auto highMid = getFloatParam (apvts, "eqHighMid");
    const auto high = getFloatParam (apvts, "eqHigh");

    for (int ch = 0; ch < 2; ++ch)
    {
        eqLow[static_cast<size_t> (ch)].setLowShelf (45.0f, 0.64f, low * 0.66f);
        eqLowMid[static_cast<size_t> (ch)].setPeak (72.0f, 0.72f, lowMid * 0.62f);
        eqMid[static_cast<size_t> (ch)].setPeak (108.0f, 0.68f, mid * 0.62f);
        eqHighMid[static_cast<size_t> (ch)].setPeak (185.0f, 0.54f, highMid * 0.64f);
        eqHigh[static_cast<size_t> (ch)].setPeak (520.0f, 0.42f, high * 0.6f);
    }
}

void SuperBassAudioProcessor::resetRealtimeState()
{
    dryLatencyBuffer.clear();
    delayBuffer.clear();
    wideDelayBuffer.clear();
    dryLatencyWritePosition = 0;
    delayWritePosition = 0;
    wideDelayWritePosition = 0;
    compressorEnv = {};
    transientFastEnv = {};
    transientSlowEnv = {};
    wideModPhase = 0.0f;
    blockGainReductionDb = 0.0f;
    openPsychoFastEnv = {};
    openPsychoSlowEnv = {};
    openPsychoCenterFastEnv = {};
    openPsychoCenterSlowEnv = {};
    openStabilityEnv = {};
    openSubControlEnv = {};
    openBodyControlEnv = {};
    openMidControlEnv = {};
    openCenterStabilityEnv = 0.0f;
    openCenterSubControlEnv = 0.0f;
    openCenterBodyControlEnv = 0.0f;
    openCenterMidControlEnv = 0.0f;
    masterClipPeakEnv = {};
    masterClipBodyEnv = {};

    for (auto* collection : { &subLowPass, &subHarmonicHighPass, &bodyBandLowPass, &bodyBandHighPass, &openSideLowPass, &openCenterLowPass, &openCenterHighPass, &openCenterBodyLowPass, &openCenterBodyHighPass, &openDeltaPolishLowPass, &openCenterDeltaPolishLowPass, &openSubPolishLowPass, &openCenterSubPolishLowPass, &openSubEvenPolishLowPass, &openCenterSubEvenPolishLowPass, &openSubOddPolishLowPass, &openCenterSubOddPolishLowPass, &openBodyPolishLowPass, &openCenterBodyPolishLowPass, &openBodyEvenPolishLowPass, &openCenterBodyEvenPolishLowPass, &openBodyOddPolishLowPass, &openCenterBodyOddPolishLowPass, &openMidPolishLowPass, &openCenterMidPolishLowPass, &openMidDrivePolishLowPass, &openCenterMidDrivePolishLowPass, &openBoomPolishLowPass, &openCenterBoomPolishLowPass, &openPsychoHarmonicHighPass, &openPsychoHarmonicLowPass, &openPsychoCenterHarmonicHighPass, &openPsychoCenterHarmonicLowPass, &depthHighPass, &sideHighPass, &masterDetector })
        for (auto& filter : *collection)
            filter.reset();

    for (auto* collection : { &diffusorAllPass, &diffusorWarmAllPass, &phaserAllPass, &phaserWarmAllPass })
        for (auto& filter : *collection)
            filter.reset();

    inputMeterSmooth = -60.0f;
    outputMeterSmooth = -60.0f;
    gainReductionMeterSmooth = 0.0f;
    inputMeterDb.store (-60.0f);
    outputMeterDb.store (-60.0f);
    gainReductionMeterDb.store (0.0f);
}

juce::AudioProcessorEditor* SuperBassAudioProcessor::createEditor()
{
    return new SuperBassAudioProcessorEditor (*this);
}

void SuperBassAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SuperBassAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

float SuperBassAudioProcessor::getFloatParam (juce::AudioProcessorValueTreeState& state, const juce::String& id)
{
    return state.getRawParameterValue (id)->load();
}

int SuperBassAudioProcessor::getChoiceParam (juce::AudioProcessorValueTreeState& state, const juce::String& id)
{
    return static_cast<int> (state.getRawParameterValue (id)->load());
}

bool SuperBassAudioProcessor::getBoolParam (juce::AudioProcessorValueTreeState& state, const juce::String& id)
{
    return state.getRawParameterValue (id)->load() > 0.5f;
}

float SuperBassAudioProcessor::softSaturate (float x, float drive)
{
    const auto safeDrive = juce::jmax (1.0f, drive);
    return std::tanh (juce::jlimit (-8.0f, 8.0f, x * safeDrive)) / std::tanh (safeDrive);
}

float SuperBassAudioProcessor::thickSaturate (float x, float drive, float tone)
{
    const auto safeDrive = juce::jmax (1.0f, drive);
    const auto clamped = juce::jlimit (-8.0f, 8.0f, x * safeDrive);
    const auto tanhShape = std::tanh (clamped) / std::tanh (safeDrive);
    const auto atanShape = (2.0f / pi) * std::atan (clamped * (0.72f + tone * 0.42f));
    const auto blend = juce::jlimit (0.0f, 0.72f, tone);
    return tanhShape * (1.0f - blend) + atanShape * blend;
}

float SuperBassAudioProcessor::premiumSaturate (float x, float drive, float warmth)
{
    const auto safeDrive = juce::jmax (1.0f, drive);
    const auto safeWarmth = juce::jlimit (0.0f, 1.0f, warmth);
    const auto clamped = juce::jlimit (-7.5f, 7.5f, x * safeDrive);
    const auto tanhShape = std::tanh (clamped * (0.82f + safeWarmth * 0.12f)) / std::tanh (safeDrive * (0.82f + safeWarmth * 0.12f));
    const auto atanShape = (2.0f / pi) * std::atan (clamped * (0.58f + safeWarmth * 0.24f));
    const auto sineShape = std::sin (juce::jlimit (-pi * 0.48f, pi * 0.48f, clamped * (0.36f + safeWarmth * 0.1f)))
                         / std::sin (pi * 0.48f);
    const auto body = tanhShape * (0.54f - safeWarmth * 0.12f)
                    + atanShape * (0.3f + safeWarmth * 0.08f)
                    + sineShape * (0.16f + safeWarmth * 0.04f);
    return juce::jlimit (-1.18f, 1.18f, body);
}

float SuperBassAudioProcessor::softClip (float x, float threshold, float softness)
{
    const auto ax = std::abs (x);
    if (ax <= threshold)
        return x;

    return std::copysign (threshold + softness * std::tanh ((ax - threshold) / softness), x);
}

void SuperBassAudioProcessor::OnePole::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    reset();
}

void SuperBassAudioProcessor::OnePole::setLowPass (float frequencyHz)
{
    const auto clamped = juce::jlimit (10.0f, static_cast<float> (sampleRate * 0.45), frequencyHz);
    a = 1.0f - std::exp (-2.0f * pi * clamped / static_cast<float> (sampleRate));
}

void SuperBassAudioProcessor::OnePole::setHighPass (float frequencyHz)
{
    setLowPass (frequencyHz);
}

float SuperBassAudioProcessor::OnePole::processLowPass (float x)
{
    z += a * (x - z);
    return z;
}

float SuperBassAudioProcessor::OnePole::processHighPass (float x)
{
    return x - processLowPass (x);
}

void SuperBassAudioProcessor::OnePole::reset()
{
    z = 0.0f;
}

void SuperBassAudioProcessor::Biquad::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    reset();
}

void SuperBassAudioProcessor::Biquad::reset()
{
    z1 = 0.0f;
    z2 = 0.0f;
}

void SuperBassAudioProcessor::Biquad::setPeak (float freqHz, float q, float gainDb)
{
    const auto a = std::pow (10.0f, gainDb / 40.0f);
    const auto w0 = 2.0f * pi * freqHz / static_cast<float> (sampleRate);
    const auto alpha = std::sin (w0) / (2.0f * q);
    const auto cosW0 = std::cos (w0);
    const auto invA0 = 1.0f / (1.0f + alpha / a);

    b0 = (1.0f + alpha * a) * invA0;
    b1 = (-2.0f * cosW0) * invA0;
    b2 = (1.0f - alpha * a) * invA0;
    a1 = (-2.0f * cosW0) * invA0;
    a2 = (1.0f - alpha / a) * invA0;
}

void SuperBassAudioProcessor::Biquad::setLowShelf (float freqHz, float q, float gainDb)
{
    const auto a = std::pow (10.0f, gainDb / 40.0f);
    const auto w0 = 2.0f * pi * freqHz / static_cast<float> (sampleRate);
    const auto sinW0 = std::sin (w0);
    const auto cosW0 = std::cos (w0);
    const auto alpha = sinW0 / (2.0f * q);
    const auto beta = 2.0f * std::sqrt (a) * alpha;
    const auto invA0 = 1.0f / ((a + 1.0f) + (a - 1.0f) * cosW0 + beta);

    b0 = a * ((a + 1.0f) - (a - 1.0f) * cosW0 + beta) * invA0;
    b1 = 2.0f * a * ((a - 1.0f) - (a + 1.0f) * cosW0) * invA0;
    b2 = a * ((a + 1.0f) - (a - 1.0f) * cosW0 - beta) * invA0;
    a1 = -2.0f * ((a - 1.0f) + (a + 1.0f) * cosW0) * invA0;
    a2 = ((a + 1.0f) + (a - 1.0f) * cosW0 - beta) * invA0;
}

void SuperBassAudioProcessor::Biquad::setHighShelf (float freqHz, float q, float gainDb)
{
    const auto a = std::pow (10.0f, gainDb / 40.0f);
    const auto w0 = 2.0f * pi * freqHz / static_cast<float> (sampleRate);
    const auto sinW0 = std::sin (w0);
    const auto cosW0 = std::cos (w0);
    const auto alpha = sinW0 / (2.0f * q);
    const auto beta = 2.0f * std::sqrt (a) * alpha;
    const auto invA0 = 1.0f / ((a + 1.0f) - (a - 1.0f) * cosW0 + beta);

    b0 = a * ((a + 1.0f) + (a - 1.0f) * cosW0 + beta) * invA0;
    b1 = -2.0f * a * ((a - 1.0f) + (a + 1.0f) * cosW0) * invA0;
    b2 = a * ((a + 1.0f) + (a - 1.0f) * cosW0 - beta) * invA0;
    a1 = 2.0f * ((a - 1.0f) - (a + 1.0f) * cosW0) * invA0;
    a2 = ((a + 1.0f) - (a - 1.0f) * cosW0 - beta) * invA0;
}

float SuperBassAudioProcessor::Biquad::process (float x)
{
    const auto y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
}

void SuperBassAudioProcessor::AllPass::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    reset();
}

void SuperBassAudioProcessor::AllPass::reset()
{
    x1 = 0.0f;
    y1 = 0.0f;
}

void SuperBassAudioProcessor::AllPass::setFrequency (float frequencyHz)
{
    const auto clamped = juce::jlimit (20.0f, static_cast<float> (sampleRate * 0.45), frequencyHz);
    const auto t = std::tan (pi * clamped / static_cast<float> (sampleRate));
    a = (t - 1.0f) / (t + 1.0f);
}

float SuperBassAudioProcessor::AllPass::process (float x)
{
    const auto y = a * x + x1 - a * y1;
    x1 = x;
    y1 = y;
    return y;
}

void SuperBassAudioProcessor::SmoothValue::reset (double sampleRate, float seconds)
{
    timeSeconds = seconds;
    coefficient = 1.0f - std::exp (-1.0f / static_cast<float> (sampleRate * seconds));
    current = target;
}

void SuperBassAudioProcessor::SmoothValue::setCurrentAndTarget (float newValue)
{
    current = newValue;
    target = newValue;
}

void SuperBassAudioProcessor::SmoothValue::setTarget (float newTarget)
{
    target = newTarget;
}

void SuperBassAudioProcessor::SmoothValue::updateSampleRate (double sampleRate)
{
    coefficient = 1.0f - std::exp (-1.0f / static_cast<float> (sampleRate * timeSeconds));
}

float SuperBassAudioProcessor::SmoothValue::getNext()
{
    current += coefficient * (target - current);
    return current;
}

float SuperBassAudioProcessor::SmoothValue::getNext (int numSteps)
{
    const auto steps = juce::jmax (1, numSteps);
    for (int i = 0; i < steps; ++i)
        current += coefficient * (target - current);

    return current;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SuperBassAudioProcessor();
}
