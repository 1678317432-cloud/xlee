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

float getDepthDelayMs (int choice)
{
    static constexpr float values[] { 4.7f, 8.9f, 13.6f };
    return values[juce::jlimit (0, 2, choice)];
}

float getDepthHighPass (int choice)
{
    static constexpr float values[] { 190.0f, 145.0f, 115.0f };
    return values[juce::jlimit (0, 2, choice)];
}

float choiceToColor (int choice)
{
    static constexpr float values[] { 0.0f, 0.45f, 0.72f, 1.0f };
    return values[juce::jlimit (0, 3, choice)];
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
    auto subFreqRange = juce::NormalisableRange<float> (30.0f, 120.0f, 0.1f, 0.55f);
    auto bodyFreqRange = juce::NormalisableRange<float> (60.0f, 800.0f, 0.1f, 0.42f);
    auto wideFreqRange = juce::NormalisableRange<float> (20.0f, 20000.0f, 0.1f, 0.28f);

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("smile", "Smile", percentRange, 35.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> ("openEnabled", "Open", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("openSub", "Sub", percentRange, 38.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("openSubFreq", "Sub Hz", subFreqRange, 58.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("openBody", "Body", percentRange, 34.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("openBodyFreq", "Body Hz", bodyFreqRange, 190.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> ("spaceEnabled", "Space", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spaceDepth", "Depth", percentRange, 18.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("spaceDepthPosition", "Depth Position", juce::StringArray { "Near", "Mid", "Far" }, 1));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spaceWide", "Wide", percentRange, 12.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spaceWideFreq", "Wide Hz", wideFreqRange, 480.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("spaceWideMode", "Wide Mode", juce::StringArray { "Hass", "Flanger", "Phaser" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterBool> ("masterEnabled", "Master", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterClipper", "Clipper", percentRange, 8.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("masterAutoGain", "Auto Gain", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterCompression", "Comp", percentRange, 10.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterCompAttack", "Comp Attack", juce::NormalisableRange<float> (1.0f, 80.0f, 0.1f, 0.45f), 30.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterCompRelease", "Comp Release", juce::NormalisableRange<float> (30.0f, 500.0f, 1.0f, 0.55f), 120.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterTransient", "Transient", juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterTransientAttack", "Trans A", juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 15.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterTransientRelease", "Trans D", juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("colorMode", "Color", juce::StringArray { "Clean", "Analog", "Tape", "Tube" }, 1));
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

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("input", "Input", trimRange, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output", "Gain", trimRange, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix", "Mix", percentRange, 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("hq", "HQ", false));

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

    for (auto* collection : { &subLowPass, &subHarmonicHighPass, &bodyBandLowPass, &bodyBandHighPass, &depthHighPass, &sideHighPass, &masterDetector, &transientSlow, &smileLowPass, &smileHighPass })
        for (auto& filter : *collection)
            filter.prepare (sampleRate);

    for (auto& filter : wideAllPass)
        filter.prepare (sampleRate);

    for (auto* collection : { &eqLow, &eqLowMid, &eqMid, &eqHighMid, &eqHigh })
        for (auto& filter : *collection)
            filter.prepare (sampleRate);

    smileSmooth.reset (sampleRate, 0.04f);
    depthSmooth.reset (sampleRate, 0.025f);
    wideSmooth.reset (sampleRate, 0.035f);
    wideFreqSmooth.reset (sampleRate, 0.045f);
    wideDelaySmooth.reset (sampleRate, 0.02f);
    compressorEnv = {};
    smileGlueEnv = {};
    transientFastEnv = {};
    transientSlowEnv = {};
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

    updateFilters();

    buffer.applyGain (dbToGain (getFloatParam (apvts, "input")));
    buffer.applyGain (dbToGain (internalHeadroomDb));

    smileSmooth.setTarget (getFloatParam (apvts, "smile") / 100.0f);
    const auto smile = juce::jlimit (0.0f, 1.0f, smileSmooth.getNext());

    for (auto module : getRoutingOrder())
        processModule (module, buffer, 1.0f);

    buffer.applyGain (dbToGain (-internalHeadroomDb));
    processSmile (buffer, smile);

    buffer.applyGain (dbToGain (getFloatParam (apvts, "output")));

    const auto mix = getFloatParam (apvts, "mix") / 100.0f;
    if (mix < 0.999f)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* wet = buffer.getWritePointer (ch);
            const auto* dry = dryBuffer.getReadPointer (ch);

            for (int i = 0; i < numSamples; ++i)
                wet[i] = dry[i] + (wet[i] - dry[i]) * mix;
        }
    }
}

void SuperBassAudioProcessor::processModule (ModuleId module, juce::AudioBuffer<float>& buffer, float smile)
{
    if (module == ModuleId::open && getBoolParam (apvts, "openEnabled"))
        processOpen (buffer, smile);

    if (module == ModuleId::space && getBoolParam (apvts, "spaceEnabled"))
        processSpace (buffer, smile);

    if (module == ModuleId::master && getBoolParam (apvts, "masterEnabled"))
        processMaster (buffer, smile);
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

void SuperBassAudioProcessor::processOpen (juce::AudioBuffer<float>& buffer, float smile)
{
    const auto subAmount = getFloatParam (apvts, "openSub") / 100.0f * smile;
    const auto bodyAmount = getFloatParam (apvts, "openBody") / 100.0f * smile;
    const auto subFreq = getFloatParam (apvts, "openSubFreq");
    const auto bodyFreq = getFloatParam (apvts, "openBodyFreq");
    const auto color = choiceToColor (getChoiceParam (apvts, "colorMode"));
    const auto bodyDrive = 1.04f + bodyAmount * (1.55f + color * 0.28f);
    const auto subDrive = 1.04f + subAmount * 1.55f;

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
            const auto subBand = subLp.processLowPass (x);
            const auto subSaturated = softSaturate (subBand * 1.05f, subDrive);
            const auto subHarm = subHp.processHighPass (subSaturated) * 0.18f;
            const auto bodyBand = bodyHp.processHighPass (bodyLp.processLowPass (x));
            const auto bodyThick = softSaturate (bodyBand * (1.1f + bodyAmount), bodyDrive);
            const auto bodyHarm = bodyThick - bodyBand * 0.12f;
            const auto subSupport = subBand * (0.34f * subAmount);
            const auto passiveLift = bodyBand * (0.52f * bodyAmount);
            const auto lowMidBloom = bodyBand * bodyAmount * (0.11f + color * 0.08f);

            const auto polished = x + subHarm * (0.14f * subAmount) + subSupport + passiveLift + lowMidBloom + bodyHarm * (0.24f * bodyAmount);
            data[i] = juce::jlimit (-4.0f, 4.0f, softSaturate (polished * dbToGain (subAmount * 0.35f), 1.08f + bodyAmount * 0.45f) * dbToGain (-bodyAmount * 0.18f));
        }
    }
}

void SuperBassAudioProcessor::processSpace (juce::AudioBuffer<float>& buffer, float smile)
{
    const auto channels = buffer.getNumChannels();
    const auto samples = buffer.getNumSamples();
    depthSmooth.setTarget (getFloatParam (apvts, "spaceDepth") / 100.0f * smile);
    wideSmooth.setTarget (getFloatParam (apvts, "spaceWide") / 100.0f * smile);
    wideFreqSmooth.setTarget (getFloatParam (apvts, "spaceWideFreq"));
    const auto depthChoice = getChoiceParam (apvts, "spaceDepthPosition");
    const auto wideMode = getChoiceParam (apvts, "spaceWideMode");

    for (int ch = 0; ch < channels; ++ch)
        depthHighPass[static_cast<size_t> (ch % 2)].setHighPass (getDepthHighPass (depthChoice));

    const int delaySamples = juce::jlimit (1, delayBuffer.getNumSamples() - 1,
        static_cast<int> (currentSampleRate * getDepthDelayMs (depthChoice) / 1000.0));
    const auto feedback = depthChoice == 2 ? 0.16f : (depthChoice == 1 ? 0.1f : 0.05f);
    const std::array<int, 6> earlyOffsets {
        juce::jmax (1, static_cast<int> (currentSampleRate * 0.0017)),
        juce::jmax (1, static_cast<int> (currentSampleRate * 0.0031)),
        juce::jmax (1, static_cast<int> (currentSampleRate * 0.0049)),
        juce::jmax (1, static_cast<int> (currentSampleRate * 0.0072)),
        juce::jmax (1, static_cast<int> (currentSampleRate * 0.0098)),
        juce::jmax (1, static_cast<int> (currentSampleRate * 0.0134))
    };

    for (int i = 0; i < samples; ++i)
    {
        const auto depth = depthSmooth.getNext();
        const auto wetGain = depth * (depthChoice == 2 ? 0.46f : (depthChoice == 1 ? 0.34f : 0.22f));
        const int readPosition = (delayWritePosition + delayBuffer.getNumSamples() - delaySamples) % delayBuffer.getNumSamples();

        for (int ch = 0; ch < channels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            auto filteredSend = depthHighPass[static_cast<size_t> (ch % 2)].processHighPass (data[i]);
            const auto delayed = delayBuffer.getSample (ch, readPosition);
            float early = 0.0f;
            for (size_t tap = 0; tap < earlyOffsets.size(); ++tap)
            {
                const auto pos = (delayWritePosition + delayBuffer.getNumSamples() - earlyOffsets[tap]) % delayBuffer.getNumSamples();
                const auto tapSign = ((tap + static_cast<size_t> (ch)) % 2 == 0) ? 1.0f : -1.0f;
                early += delayBuffer.getSample (ch, pos) * tapSign * (0.25f - static_cast<float> (tap) * 0.025f);
            }

            delayBuffer.setSample (ch, delayWritePosition, filteredSend + delayed * feedback);
            data[i] += (delayed * 0.32f + early) * wetGain;
        }

        delayWritePosition = (delayWritePosition + 1) % delayBuffer.getNumSamples();
    }

    if (channels >= 2)
    {
        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);
        const auto modeDelayMs = wideMode == 0 ? 8.0f : (wideMode == 1 ? 1.8f : 2.6f);
        wideDelaySmooth.setTarget (static_cast<float> (currentSampleRate * modeDelayMs / 1000.0));

        sideHighPass[0].setHighPass (360.0f);
        sideHighPass[1].setHighPass (360.0f);

        for (int i = 0; i < samples; ++i)
        {
            const auto wide = wideSmooth.getNext();
            const auto wideFreq = wideFreqSmooth.getNext();
            const auto lfo = std::sin (wideModPhase);
            wideModPhase += 2.0f * pi * (wideMode == 1 ? 0.065f : 0.045f) / static_cast<float> (currentSampleRate);
            if (wideModPhase > 2.0f * pi)
                wideModPhase -= 2.0f * pi;

            const auto mid = 0.5f * (left[i] + right[i]);
            const auto sideRaw = 0.5f * (left[i] - right[i]);
            const auto inputL = left[i];
            const auto inputR = right[i];
            const auto delayBase = wideDelaySmooth.getNext();
            const auto sweep = (wideMode == 0 ? 0.0f : (wideMode == 1 ? 0.55f : 0.28f)) * (lfo + 1.0f);
            const auto delayedL = interpolateDelay (wideDelayBuffer, 0, delayBase + sweep, wideDelayWritePosition);
            const auto delayedR = interpolateDelay (wideDelayBuffer, 1, delayBase + sweep * 1.37f + 1.0f, wideDelayWritePosition);

            wideAllPass[0].setFrequency (juce::jlimit (20.0f, 20000.0f, wideFreq * (wideMode == 2 ? 0.82f : 1.0f)));
            wideAllPass[1].setFrequency (juce::jlimit (20.0f, 20000.0f, wideFreq * (wideMode == 2 ? 1.46f : 1.19f)));
            const auto phaseL = wideAllPass[0].process (mid);
            const auto phaseR = wideAllPass[1].process (mid);

            float decorrelated = 0.0f;
            if (wideMode == 0)
                decorrelated = (delayedR - delayedL) * 0.34f + (phaseL - phaseR) * 0.11f;
            else if (wideMode == 1)
                decorrelated = ((delayedL - inputR) - (delayedR - inputL)) * 0.16f + (phaseL - phaseR) * 0.1f;
            else
                decorrelated = (phaseL - phaseR) * 0.28f + (delayedR - delayedL) * 0.09f;

            const auto side = juce::jlimit (-0.72f, 0.72f,
                sideHighPass[0].processHighPass (sideRaw) * (1.0f + wide * 0.95f)
                + sideHighPass[1].processHighPass (decorrelated) * wide * 0.58f);
            const auto monoGuard = 1.0f - wide * 0.06f;
            left[i] = juce::jlimit (-2.0f, 2.0f, mid * monoGuard + side);
            right[i] = juce::jlimit (-2.0f, 2.0f, mid * monoGuard - side);

            wideDelayBuffer.setSample (0, wideDelayWritePosition, inputL);
            wideDelayBuffer.setSample (1, wideDelayWritePosition, inputR);
            wideDelayWritePosition = (wideDelayWritePosition + 1) % wideDelayBuffer.getNumSamples();
        }
    }
}

void SuperBassAudioProcessor::processMaster (juce::AudioBuffer<float>& buffer, float smile)
{
    const auto channels = buffer.getNumChannels();
    const auto samples = buffer.getNumSamples();
    const auto clipper = getFloatParam (apvts, "masterClipper") / 100.0f * smile;
    const auto compression = getFloatParam (apvts, "masterCompression") / 100.0f;
    const auto compAttackMs = getFloatParam (apvts, "masterCompAttack");
    const auto compReleaseMs = getFloatParam (apvts, "masterCompRelease");
    const auto transient = getFloatParam (apvts, "masterTransient") / 100.0f;
    const auto transientAttackAmount = getFloatParam (apvts, "masterTransientAttack") / 100.0f;
    const auto transientReleaseAmount = getFloatParam (apvts, "masterTransientRelease") / 100.0f;
    const auto ratio = 6.0f + compression * 4.0f;
    const auto thresholdDb = -24.0f + compression * 8.0f;
    const auto compAttackCoeff = std::exp (-1.0f / static_cast<float> (currentSampleRate * compAttackMs * 0.001));
    const auto compReleaseCoeff = std::exp (-1.0f / static_cast<float> (currentSampleRate * compReleaseMs * 0.001));
    const auto transFastCoeff = std::exp (-1.0f / static_cast<float> (currentSampleRate * 0.006));
    const auto transSlowCoeff = std::exp (-1.0f / static_cast<float> (currentSampleRate * 0.12));
    const auto preClipGain = dbToGain (clipper * 1.8f);
    const auto postClipGain = getBoolParam (apvts, "masterAutoGain") ? dbToGain (compression * 3.5f - clipper * 0.55f) : 1.0f;
    const auto threshold = 0.98f - clipper * 0.06f;
    const auto softness = 0.58f + (1.0f - clipper) * 0.24f;

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto channel = static_cast<size_t> (ch % 2);

        for (int i = 0; i < samples; ++i)
        {
            auto x = data[i];
            x = eqLow[channel].process (x);
            x = eqLowMid[channel].process (x);
            x = eqMid[channel].process (x);
            x = eqHighMid[channel].process (x);
            x = eqHigh[channel].process (x);

            const auto detector = std::abs (x);
            const auto envCoeff = detector > compressorEnv[channel] ? compAttackCoeff : compReleaseCoeff;
            compressorEnv[channel] = envCoeff * compressorEnv[channel] + (1.0f - envCoeff) * detector;
            const auto envDb = juce::Decibels::gainToDecibels (juce::jmax (compressorEnv[channel], 0.000001f));
            float gainReductionDb = 0.0f;
            if (envDb > thresholdDb)
                gainReductionDb = (thresholdDb + (envDb - thresholdDb) / ratio) - envDb;
            const auto punchyCompression = gainReductionDb * compression * 0.48f;
            x *= dbToGain (punchyCompression);

            transientFastEnv[channel] = transFastCoeff * transientFastEnv[channel] + (1.0f - transFastCoeff) * detector;
            transientSlowEnv[channel] = transSlowCoeff * transientSlowEnv[channel] + (1.0f - transSlowCoeff) * detector;
            const auto transientDelta = juce::jlimit (-1.0f, 1.0f, transientFastEnv[channel] - transientSlowEnv[channel]);
            const auto sustainDelta = juce::jlimit (-1.0f, 1.0f, transientSlowEnv[channel] - transientFastEnv[channel]);
            x *= 1.0f + transient * (transientDelta * (1.6f + transientAttackAmount * 1.4f) + sustainDelta * transientReleaseAmount * 1.2f);

            x = softClip (x * preClipGain, threshold, softness) * postClipGain;
            data[i] = juce::jlimit (-1.5f, 1.5f, x);
        }
    }
}

void SuperBassAudioProcessor::processSmile (juce::AudioBuffer<float>& buffer, float smile)
{
    const auto amount = juce::jlimit (0.0f, 1.0f, smile);
    const auto shaped = amount * amount * (3.0f - 2.0f * amount);
    const auto colorChoice = getChoiceParam (apvts, "colorMode");
    const auto color = choiceToColor (colorChoice);
    const auto glueAttack = std::exp (-1.0f / static_cast<float> (currentSampleRate * 0.012));
    const auto glueRelease = std::exp (-1.0f / static_cast<float> (currentSampleRate * 0.18));
    const auto drive = 1.0f + shaped * (0.22f + color * 0.42f);
    const auto lowLift = shaped * (0.035f + color * 0.045f);
    const auto airTrim = shaped * (0.018f + color * 0.028f);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);
        auto channel = static_cast<size_t> (ch % 2);
        smileLowPass[channel].setLowPass (colorChoice >= 2 ? 150.0f : 120.0f);
        smileHighPass[channel].setHighPass (colorChoice == 0 ? 7200.0f : 5200.0f);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            auto x = wet[i];
            const auto detector = std::abs (x);
            const auto envCoeff = detector > smileGlueEnv[channel] ? glueAttack : glueRelease;
            smileGlueEnv[channel] = envCoeff * smileGlueEnv[channel] + (1.0f - envCoeff) * detector;
            const auto glue = dbToGain (-juce::jlimit (0.0f, 2.2f, (smileGlueEnv[channel] - 0.16f) * (2.4f + color * 1.6f)));
            const auto low = smileLowPass[channel].processLowPass (x);
            const auto top = smileHighPass[channel].processHighPass (x);
            const auto rounded = softSaturate ((x + low * lowLift - top * airTrim) * drive, 1.02f + color * 0.55f) * glue;
            const auto colourComp = dbToGain (-shaped * (0.12f + color * 0.3f));
            const auto processed = rounded * colourComp;
            wet[i] = dry[i] + (processed - dry[i]) * shaped;
        }
    }
}

void SuperBassAudioProcessor::updateFilters()
{
    const auto smile = juce::jlimit (0.0f, 1.0f, getFloatParam (apvts, "smile") / 100.0f);
    const auto low = getFloatParam (apvts, "eqLow") * smile;
    const auto lowMid = getFloatParam (apvts, "eqLowMid") * smile;
    const auto mid = getFloatParam (apvts, "eqMid") * smile;
    const auto highMid = getFloatParam (apvts, "eqHighMid") * smile;
    const auto high = getFloatParam (apvts, "eqHigh") * smile;

    for (int ch = 0; ch < 2; ++ch)
    {
        eqLow[static_cast<size_t> (ch)].setLowShelf (70.0f, 0.5f, low * 0.85f);
        eqLowMid[static_cast<size_t> (ch)].setPeak (180.0f, 0.42f, lowMid * 0.82f);
        eqMid[static_cast<size_t> (ch)].setPeak (650.0f, 0.38f, mid * 0.78f);
        eqHighMid[static_cast<size_t> (ch)].setPeak (2400.0f, 0.4f, highMid * 0.76f);
        eqHigh[static_cast<size_t> (ch)].setHighShelf (10500.0f, 0.5f, high * 0.8f);
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
