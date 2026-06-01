#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float pi = juce::MathConstants<float>::pi;
constexpr float internalHeadroomDb = -9.0f;

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
    auto trimRange = juce::NormalisableRange<float> (-9.0f, 9.0f, 0.1f);
    auto subFreqRange = juce::NormalisableRange<float> (30.0f, 120.0f, 0.1f, 0.55f);
    auto bodyFreqRange = juce::NormalisableRange<float> (60.0f, 800.0f, 0.1f, 0.42f);
    auto wideFreqRange = juce::NormalisableRange<float> (20.0f, 20000.0f, 0.1f, 0.28f);
    auto rateRange = juce::NormalisableRange<float> (0.05f, 2.5f, 0.001f, 0.42f);
    auto diffuserFreqRange = juce::NormalisableRange<float> (80.0f, 12000.0f, 0.1f, 0.35f);

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("smile", "Smile", percentRange, 35.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> ("openEnabled", "Open", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("openSub", "Sub", percentRange, 38.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("openSubFreq", "Sub Hz", subFreqRange, 58.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("openBody", "Body", percentRange, 34.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("openBodyFreq", "Body Hz", bodyFreqRange, 190.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> ("spaceEnabled", "Space", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spaceDepth", "Depth", percentRange, 18.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spaceDepthDistance", "Depth Distance", percentRange, 45.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spaceWide", "Wide", percentRange, 12.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spaceWideFreq", "Wide Hz", wideFreqRange, 480.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spaceWideRate", "Rate", rateRange, 0.55f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("spaceWideMode", "Wide Mode", juce::StringArray { "Hass", "Flanger", "Phaser" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("spaceAllPassEnabled", "Diffusor", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spaceDiffusorHz", "Diffusor Hz", diffuserFreqRange, 950.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> ("masterEnabled", "Master", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterClipper", "Clipper", percentRange, 8.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterCompression", "Comp", percentRange, 10.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterCompAttack", "Comp Attack", juce::NormalisableRange<float> (1.0f, 80.0f, 0.1f, 0.45f), 30.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterCompRelease", "Comp Release", juce::NormalisableRange<float> (30.0f, 500.0f, 1.0f, 0.55f), 120.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterTransientAttack", "Trans Attack", juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterTransientRelease", "Trans Release", juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterTransientMix", "Trans Mix", percentRange, 35.0f));
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

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output", "Gain", trimRange, 0.0f));

    return { params.begin(), params.end() };
}

void SuperBassAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    const auto channels = juce::jmax (1, getTotalNumOutputChannels());

    dryBuffer.setSize (channels, samplesPerBlock);
    scratchBuffer.setSize (channels, samplesPerBlock);
    delayBuffer.setSize (channels, static_cast<int> (sampleRate * 0.08));
    wideDelayBuffer.setSize (channels, static_cast<int> (sampleRate * 0.04));
    delayBuffer.clear();
    wideDelayBuffer.clear();
    delayWritePosition = 0;
    wideDelayWritePosition = 0;

    for (auto* collection : { &subLowPass, &subHarmonicHighPass, &bodyBandLowPass, &bodyBandHighPass, &openSideLowPass, &openCenterLowPass, &openCenterHighPass, &openCenterBodyLowPass, &openCenterBodyHighPass, &depthHighPass, &sideHighPass, &masterDetector })
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
    outputGainSmooth.reset (sampleRate, 0.025f);
    depthSmooth.reset (sampleRate, 0.025f);
    depthDistanceSmooth.reset (sampleRate, 0.04f);
    wideSmooth.reset (sampleRate, 0.035f);
    wideFreqSmooth.reset (sampleRate, 0.045f);
    wideRateSmooth.reset (sampleRate, 0.08f);
    diffusorFreqSmooth.reset (sampleRate, 0.05f);
    wideDelaySmooth.reset (sampleRate, 0.02f);
    compressorEnv = {};
    transientFastEnv = {};
    transientSlowEnv = {};
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
    depthSmooth.setCurrentAndTarget (getFloatParam (apvts, "spaceDepth") / 100.0f);
    depthDistanceSmooth.setCurrentAndTarget (getFloatParam (apvts, "spaceDepthDistance") / 100.0f);
    wideSmooth.setCurrentAndTarget (getFloatParam (apvts, "spaceWide") / 100.0f);
    wideFreqSmooth.setCurrentAndTarget (getFloatParam (apvts, "spaceWideFreq"));
    wideRateSmooth.setCurrentAndTarget (getFloatParam (apvts, "spaceWideRate"));
    diffusorFreqSmooth.setCurrentAndTarget (getFloatParam (apvts, "spaceDiffusorHz"));
    wideDelaySmooth.setCurrentAndTarget (static_cast<float> (sampleRate * 0.008));
    updateFilters();
}

void SuperBassAudioProcessor::releaseResources()
{
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

    updateFilters();

    const auto smileAmount = juce::jlimit (0.0f, 1.0f, getFloatParam (apvts, "smile") / 100.0f);
    smileSmooth.setTarget (smileAmount);
    blockGainReductionDb = 0.0f;

    buffer.applyGain (dbToGain (internalHeadroomDb));

    const auto routingOrder = getRoutingOrder();
    if (routingOrder != lastRoutingOrder)
    {
        routingWetSmooth.setCurrentAndTarget (0.0f);
        lastRoutingOrder = routingOrder;
    }

    processRouting (buffer, routingOrder);

    routingWetSmooth.setTarget (1.0f);
    for (int i = 0; i < numSamples; ++i)
    {
        const auto gain = routingWetSmooth.getNext();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* wet = buffer.getWritePointer (ch);
            const auto internalDry = dryBuffer.getSample (ch, i) * dbToGain (internalHeadroomDb);
            wet[i] = internalDry + (wet[i] - internalDry) * gain;
        }
    }

    buffer.applyGain (dbToGain (-internalHeadroomDb));
    applyWetLevelMatch (buffer);
    processSmile (buffer);
    applyFinalLevelMatch (buffer);
    applyOutputGain (buffer);
    updateLevelMeter (buffer, outputMeterSmooth, outputMeterDb);
    updateGainReductionMeter();
}

void SuperBassAudioProcessor::processRouting (juce::AudioBuffer<float>& buffer, const std::array<ModuleId, 3>& order)
{
    for (auto module : order)
        processModule (module, buffer);
}

void SuperBassAudioProcessor::processModule (ModuleId module, juce::AudioBuffer<float>& buffer)
{
    if (module == ModuleId::open && getBoolParam (apvts, "openEnabled"))
    {
        const auto beforeRms = measurePerceivedLevel (buffer);
        processOpen (buffer);
        applyModuleLevelMatch (buffer, beforeRms, openLevelSmooth, 0.64f, 1.16f);
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
        applyModuleLevelMatch (buffer, beforeRms, masterLevelSmooth, 0.68f, 1.1f);
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

void SuperBassAudioProcessor::processOpen (juce::AudioBuffer<float>& buffer)
{
    const auto subAmount = getFloatParam (apvts, "openSub") / 100.0f;
    const auto bodyAmount = getFloatParam (apvts, "openBody") / 100.0f;
    const auto subFreq = getFloatParam (apvts, "openSubFreq");
    const auto bodyFreq = getFloatParam (apvts, "openBodyFreq");
    const auto bodyDrive = 1.035f + bodyAmount * 2.05f;
    const auto subDrive = 1.018f + subAmount * 1.95f;
    const auto openDensity = juce::jlimit (0.0f, 1.0f, subAmount * 0.48f + bodyAmount * 0.62f);

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
            const auto anchor = juce::jlimit (0.0f, 0.92f, 0.78f * subAmount + 0.42f * bodyAmount);
            const auto anchoredSide = side - lowSide * anchor;
            left[i] = mid + anchoredSide;
            right[i] = mid - anchoredSide;
        }

        auto& subMidLp = openCenterLowPass[0];
        auto& subHarmHp = openCenterHighPass[0];
        auto& bodyMidLp = openCenterBodyLowPass[0];
        auto& bodyMidHp = openCenterBodyHighPass[0];
        subMidLp.setLowPass (subFreq * 1.45f);
        subHarmHp.setHighPass (juce::jmax (70.0f, subFreq * 1.32f));
        bodyMidLp.setLowPass (bodyFreq * 3.2f);
        bodyMidHp.setHighPass (juce::jmax (30.0f, bodyFreq * 0.32f));

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto mid = 0.5f * (left[i] + right[i]);
            const auto side = 0.5f * (left[i] - right[i]);
            const auto subBand = subMidLp.processLowPass (mid * 0.5f);
            const auto subHarmRaw = subHarmHp.processHighPass (softSaturate (subBand * 1.16f, subDrive));
            const auto subHarm = subHarmRaw + (subHarmRaw * subHarmRaw * std::copysign (1.0f, subHarmRaw)) * (0.18f * subAmount);
            const auto bodyBand = bodyMidHp.processHighPass (bodyMidLp.processLowPass (mid * 0.5f));
            const auto bodyEven = softSaturate (bodyBand * (1.0f + bodyAmount * 0.85f), bodyDrive) - bodyBand * 0.12f;
            const auto bodyOdd = softSaturate (bodyBand * (1.0f + bodyAmount * 1.62f), 1.1f + bodyAmount * 2.18f) - bodyBand;
            const auto subPath = mid * 0.5f
                               + subBand * (0.34f * subAmount)
                               + subHarm * (0.22f * subAmount);
            const auto bodyPath = mid * 0.5f
                                + bodyBand * (0.44f * bodyAmount)
                                + bodyEven * (0.21f * bodyAmount)
                                + bodyOdd * (0.16f * bodyAmount);
            const auto centerLift = (subPath + bodyPath) - mid;
            const auto focusedMid = softSaturate ((mid + centerLift) * dbToGain (-(subAmount + bodyAmount) * 0.18f),
                                                  1.02f + bodyAmount * 0.24f);
            left[i] = focusedMid + side;
            right[i] = focusedMid - side;
        }
    }

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& subLp = subLowPass[static_cast<size_t> (ch % 2)];
        auto& subHp = subHarmonicHighPass[static_cast<size_t> (ch % 2)];
        auto& bodyLp = bodyBandLowPass[static_cast<size_t> (ch % 2)];
        auto& bodyHp = bodyBandHighPass[static_cast<size_t> (ch % 2)];

        subLp.setLowPass (subFreq * 1.35f);
        subHp.setHighPass (juce::jmax (70.0f, subFreq * 1.25f));
        bodyLp.setLowPass (bodyFreq * 3.4f);
        bodyHp.setHighPass (juce::jmax (28.0f, bodyFreq * 0.3f));

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto x = data[i];
            const auto subInput = x * 0.5f;
            const auto bodyInput = x * 0.5f;
            const auto subBand = subLp.processLowPass (subInput);
            const auto subSaturated = softSaturate (subBand * 1.06f, subDrive);
            const auto subHarmRaw = subHp.processHighPass (subSaturated);
            const auto subHarm = subHarmRaw + (subHarmRaw * subHarmRaw * std::copysign (1.0f, subHarmRaw)) * (0.16f * subAmount);
            const auto bodyBand = bodyHp.processHighPass (bodyLp.processLowPass (bodyInput));
            const auto bodyThick = softSaturate (bodyBand * (1.06f + bodyAmount * 0.82f), bodyDrive);
            const auto bodyOdd = softSaturate (bodyBand * (1.0f + bodyAmount * 1.65f), 1.06f + bodyAmount * 2.1f) - bodyBand;
            const auto bodyHarm = bodyThick - bodyBand * 0.08f + bodyOdd * 0.3f;

            const auto subPath = subInput
                               + subBand * (0.36f * subAmount)
                               + subHarm * (0.16f * subAmount);
            const auto bodyPath = bodyInput
                                + bodyBand * (0.48f * bodyAmount)
                                + bodyHarm * (0.22f * bodyAmount);
            const auto polished = subPath + bodyPath + bodyBand * bodyAmount * 0.045f;
            const auto compressed = softSaturate (polished * dbToGain (openDensity * 0.34f), 1.04f + bodyAmount * 0.34f);
            data[i] = juce::jlimit (-3.6f, 3.6f, compressed * dbToGain (-openDensity * 0.58f));
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
        const auto distanceMs = 4.5f + distance * 26.0f;
        const auto feedback = juce::jlimit (0.025f, 0.19f, 0.035f + distance * 0.14f + depth * 0.035f);
        const auto wetGain = depth * (0.38f + distance * 0.58f);
        const auto dryDistance = 1.0f - depth * (0.045f + distance * 0.22f);
        const auto reflectionHp = 150.0f + distance * 130.0f;
        const int delaySamples = juce::jlimit (1, delayBuffer.getNumSamples() - 1,
            static_cast<int> (currentSampleRate * distanceMs / 1000.0f));
        const int readPosition = (delayWritePosition + delayBuffer.getNumSamples() - delaySamples) % delayBuffer.getNumSamples();

        for (int ch = 0; ch < channels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            auto& hp = depthHighPass[static_cast<size_t> (ch % 2)];
            hp.setHighPass (reflectionHp);
            auto filteredSend = hp.processHighPass (data[i]) * (1.0f - depth * (0.1f + distance * 0.16f));
            const auto delayed = delayBuffer.getSample (ch, readPosition);
            float early = 0.0f;
            for (int tap = 0; tap < 8; ++tap)
            {
                const auto tapBaseMs = 1.1f + static_cast<float> (tap) * (1.55f + distance * 2.35f);
                const auto offset = juce::jlimit (1, delayBuffer.getNumSamples() - 1,
                    static_cast<int> (currentSampleRate * tapBaseMs * 0.001f));
                const auto pos = (delayWritePosition + delayBuffer.getNumSamples() - offset) % delayBuffer.getNumSamples();
                const auto tapSign = ((tap + ch) % 2 == 0) ? 1.0f : -1.0f;
                const auto weight = (0.28f - static_cast<float> (tap) * 0.018f) * (0.78f + distance * 0.34f);
                early += delayBuffer.getSample (ch, pos) * tapSign * weight;
            }

            delayBuffer.setSample (ch, delayWritePosition, filteredSend + delayed * feedback);
            const auto depthTone = softSaturate ((delayed * (0.10f + distance * 0.08f) + early) * (0.82f + distance * 0.16f),
                                                 1.01f + depth * 0.08f);
            const auto distanceDarkening = 1.0f - depth * distance * 0.1f;
            data[i] = data[i] * dryDistance * distanceDarkening + depthTone * wetGain;
        }

        delayWritePosition = (delayWritePosition + 1) % delayBuffer.getNumSamples();
    }

    if (diffusorEnabled)
    {
        const auto depthContribution = getFloatParam (apvts, "spaceDepth") / 100.0f;
        const auto wideContribution = getFloatParam (apvts, "spaceWide") / 100.0f;
        const auto diffusorMix = juce::jlimit (0.14f, 0.4f, 0.2f + depthContribution * 0.13f + wideContribution * 0.07f);

        for (int i = 0; i < samples; ++i)
        {
            const auto freq = diffusorFreqSmooth.getNext();
            for (int ch = 0; ch < channels; ++ch)
            {
                auto* data = buffer.getWritePointer (ch);
                const auto idx = static_cast<size_t> (ch % 2);
                auto& bright = diffusorAllPass[idx];
                auto& warm = diffusorWarmAllPass[idx];
                bright.setFrequency (juce::jlimit (90.0f, 18000.0f, freq * (ch % 2 == 0 ? 0.9f : 1.14f)));
                warm.setFrequency (juce::jlimit (70.0f, 12000.0f, freq * (ch % 2 == 0 ? 0.36f : 0.43f)));
                const auto stageA = bright.process (data[i]);
                const auto stageB = warm.process (stageA);
                const auto diffused = stageB * 0.74f + stageA * 0.26f;
                data[i] += (softSaturate (diffused, 1.01f + depthContribution * 0.06f) - data[i]) * diffusorMix;
            }
        }
    }

    if (channels >= 2)
    {
        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);
        const auto modeDelayMs = wideMode == 0 ? 11.2f : (wideMode == 1 ? 3.25f : 1.65f);
        wideDelaySmooth.setTarget (static_cast<float> (currentSampleRate * modeDelayMs / 1000.0));

        for (int i = 0; i < samples; ++i)
        {
            const auto wide = wideSmooth.getNext();
            const auto wideFreq = wideFreqSmooth.getNext();
            const auto rate = wideRateSmooth.getNext();
            const auto sideCutoff = juce::jlimit (70.0f, 8000.0f, wideFreq);
            sideHighPass[0].setHighPass (sideCutoff);
            sideHighPass[1].setHighPass (sideCutoff * 1.08f);

            const auto lfo = std::sin (wideModPhase);
            const auto lfo2 = std::sin (wideModPhase * 0.63f + 1.4f);
            const auto organicLfo = lfo * 0.82f + lfo2 * 0.18f;
            const auto baseRate = wideMode == 1 ? 0.115f : (wideMode == 2 ? 0.067f : 0.032f);
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
            const auto movementDepth = wideMode == 0 ? (0.72f + rate * 0.1f)
                                                     : (0.74f + rate * 0.18f);
            const auto sweep = (wideMode == 0 ? 0.0f : (wideMode == 1 ? 3.55f : 1.18f)) * movementDepth * (organicLfo + 1.0f);
            const auto delayedL = interpolateDelay (wideDelayBuffer, 0, delayBase + sweep, wideDelayWritePosition);
            const auto delayedR = interpolateDelay (wideDelayBuffer, 1, delayBase + sweep * 1.29f + 1.12f, wideDelayWritePosition);

            const auto phaserLHz = juce::jlimit (80.0f, 12000.0f, wideFreq * (0.72f + 0.18f * (organicLfo + 1.0f)));
            const auto phaserRHz = juce::jlimit (80.0f, 12000.0f, wideFreq * (1.2f - 0.11f * (organicLfo + 1.0f)));
            phaserAllPass[0].setFrequency (phaserLHz);
            phaserAllPass[1].setFrequency (phaserRHz);
            phaserWarmAllPass[0].setFrequency (juce::jlimit (70.0f, 10000.0f, phaserLHz * 0.42f));
            phaserWarmAllPass[1].setFrequency (juce::jlimit (70.0f, 10000.0f, phaserRHz * 0.47f));
            const auto phaseL = phaserWarmAllPass[0].process (phaserAllPass[0].process (mid));
            const auto phaseR = phaserWarmAllPass[1].process (phaserAllPass[1].process (mid));

            float decorrelated = 0.0f;
            if (wideMode == 0)
                decorrelated = (delayedR - delayedL) * 0.52f;
            else if (wideMode == 1)
                decorrelated = ((delayedL - inputR) - (delayedR - inputL)) * 0.46f + (phaseL - phaseR) * 0.14f;
            else
                decorrelated = (phaseL - phaseR) * 0.7f + (delayedR - delayedL) * 0.11f;

            const auto highSide = sideHighPass[0].processHighPass (sideRaw);
            const auto lowSide = sideRaw - highSide;
            const auto highDecorrelated = sideHighPass[1].processHighPass (decorrelated);
            const auto side = juce::jlimit (-0.78f, 0.78f,
                lowSide
                + highSide * (1.0f + wide * (wideMode == 0 ? 0.78f : 0.62f))
                + highDecorrelated * wide * (wideMode == 0 ? 0.78f : (wideMode == 1 ? 0.86f : 0.82f)));
            const auto monoGuard = 1.0f - wide * (wideMode == 0 ? 0.025f : 0.055f);
            const auto newL = mid * monoGuard + side;
            const auto newR = mid * monoGuard - side;
            const auto mix = juce::jlimit (0.0f, 0.95f, wide * 1.12f);
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
    const auto clipper = getFloatParam (apvts, "masterClipper") / 100.0f;
    const auto compression = getFloatParam (apvts, "masterCompression") / 100.0f;
    const auto compAttackMs = getFloatParam (apvts, "masterCompAttack");
    const auto compReleaseMs = getFloatParam (apvts, "masterCompRelease");
    const auto transientAttack = getFloatParam (apvts, "masterTransientAttack") / 100.0f;
    const auto transientRelease = getFloatParam (apvts, "masterTransientRelease") / 100.0f;
    const auto transientMix = getFloatParam (apvts, "masterTransientMix") / 100.0f;
    const auto ratio = 6.0f + compression * 4.0f;
    const auto thresholdDb = -24.5f + compression * 8.5f;
    const auto adjustedAttackMs = juce::jlimit (0.38f, 42.0f, compAttackMs * 0.34f + 0.48f);
    const auto adjustedReleaseMs = juce::jlimit (22.0f, 310.0f, compReleaseMs * 0.64f);
    const auto compAttackCoeff = std::exp (-1.0f / static_cast<float> (currentSampleRate * adjustedAttackMs * 0.001f));
    const auto compReleaseCoeff = std::exp (-1.0f / static_cast<float> (currentSampleRate * adjustedReleaseMs * 0.001f));
    const auto transFastCoeff = std::exp (-1.0f / static_cast<float> (currentSampleRate * 0.0032f));
    const auto sustainMs = 58.0f + (transientRelease + 1.0f) * 86.0f;
    const auto transSlowCoeff = std::exp (-1.0f / static_cast<float> (currentSampleRate * sustainMs * 0.001f));
    const auto preClipGain = dbToGain (clipper * 1.55f);
    const auto postClipGain = dbToGain (compression * 0.14f - clipper * 0.78f);
    const auto threshold = 0.99f - clipper * 0.055f;
    const auto softness = 0.74f + (1.0f - clipper) * 0.3f;
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
            float gainReductionDb = 0.0f;
            if (envDb > thresholdDb)
                gainReductionDb = (thresholdDb + (envDb - thresholdDb) / ratio) - envDb;
            blockGainReductionDb = juce::jmin (blockGainReductionDb, gainReductionDb * compression);
            const auto punchyCompression = gainReductionDb * compression * 0.52f;
            const auto compBlend = juce::jlimit (0.0f, 0.8f, compression * 0.88f);
            const auto compressed = x * dbToGain (punchyCompression);
            x += (compressed - x) * compBlend;
            x += (softSaturate (x, 1.035f + compression * 0.28f) - x) * (compression * 0.038f);

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
            x += (softSaturate (x, 1.012f + eqCharacter * 0.2f) - x) * (0.022f + eqCharacter * 0.045f);

            const auto lowBeforeClip = masterDetector[channel].processLowPass (x);
            const auto highBeforeClip = x - lowBeforeClip;
            const auto clipInput = x * preClipGain;
            const auto lowInput = lowBeforeClip * preClipGain;
            const auto highInput = highBeforeClip * preClipGain;
            const auto clippedHighA = softClip (highInput, threshold, softness);
            const auto clippedHighB = softClip (clippedHighA, 0.988f - clipper * 0.022f, 0.9f);
            const auto clippedFull = softClip (clipInput, threshold + clipper * 0.02f, softness * 1.08f);
            const auto transientBlend = juce::jlimit (0.0f, 0.46f, 0.26f + clipper * 0.2f);
            const auto lowPreserve = (lowInput - softClip (lowInput, 1.08f, 1.2f)) * clipper * 0.08f;
            x = (lowInput + clippedHighB) * (1.0f - transientBlend)
                + clippedFull * 0.32f
                + clipInput * (transientBlend * 0.72f)
                + lowPreserve;
            x *= postClipGain;
            data[i] = juce::jlimit (-1.35f, 1.35f, x);
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
            const auto* dry = dryBuffer.getReadPointer (ch);
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
        const auto partialCompensation = 1.0f + (rawGain - 1.0f) * 0.92f;
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
        targetGain = juce::jlimit (0.58f, 1.22f, dryRms / wetRms);

    const auto smile = getFloatParam (apvts, "smile") / 100.0f;
    const auto openDrive = (getFloatParam (apvts, "openSub") + getFloatParam (apvts, "openBody")) / 200.0f;
    const auto masterDrive = (getFloatParam (apvts, "masterCompression") + getFloatParam (apvts, "masterClipper")) / 200.0f;
    const auto enhancementTrim = dbToGain (-(openDrive * 0.75f + masterDrive * 0.45f) * smile);
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
        const auto correctionDepth = 0.62f + smile * 0.28f;
        targetGain = 1.0f + (rawGain - 1.0f) * correctionDepth;
        targetGain = juce::jlimit (0.64f, 1.16f, targetGain);
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
        eqLow[static_cast<size_t> (ch)].setLowShelf (68.0f, 0.58f, low * 0.78f);
        eqLowMid[static_cast<size_t> (ch)].setPeak (170.0f, 0.5f, lowMid * 0.72f);
        eqMid[static_cast<size_t> (ch)].setPeak (680.0f, 0.46f, mid * 0.68f);
        eqHighMid[static_cast<size_t> (ch)].setPeak (2550.0f, 0.48f, highMid * 0.66f);
        eqHigh[static_cast<size_t> (ch)].setHighShelf (9800.0f, 0.62f, high * 0.7f);
    }
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

float SuperBassAudioProcessor::SmoothValue::getNext()
{
    current += coefficient * (target - current);
    return current;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SuperBassAudioProcessor();
}
