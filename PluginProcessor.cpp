// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Schlensog Audio Lab

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
    float getValue (GriseldaAutoFilterAudioProcessor::APVTS& apvts, const juce::String& id)
    {
        if (auto* raw = apvts.getRawParameterValue (id))
            return raw->load();
        return 0.0f;
    }

    bool getBool (GriseldaAutoFilterAudioProcessor::APVTS& apvts, const juce::String& id)
    {
        return getValue (apvts, id) > 0.5f;
    }

    int slopeToStages (int slopeIndex)
    {
        switch (slopeIndex)
        {
            case 0:  return 1;
            case 1:  return 2;
            default: return 4;
        }
    }

    float hzFromNoteDivision (float rateHzLikeValue, double bpm)
    {
        static constexpr std::array<double, 8> noteLengthsInBeats { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
        const auto norm = juce::jlimit (0.0f, 1.0f, (rateHzLikeValue - 0.05f) / (20.0f - 0.05f));
        const auto index = juce::jlimit (0, (int) noteLengthsInBeats.size() - 1,
                                         (int) std::round (norm * (float) (noteLengthsInBeats.size() - 1)));
        const auto beatsPerSecond = bpm / 60.0;
        return (float) (1.0 / (noteLengthsInBeats[(size_t) index] / beatsPerSecond));
    }

    float hannWindow (float phase01)
    {
        phase01 = juce::jlimit (0.0f, 1.0f, phase01);
        return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase01);
    }
}

GriseldaAutoFilterAudioProcessor::GriseldaAutoFilterAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

GriseldaAutoFilterAudioProcessor::APVTS::ParameterLayout GriseldaAutoFilterAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    const auto hzRange       = juce::NormalisableRange<float> (20.0f, 20000.0f, 0.01f, 0.22f);
    const auto lowHzRange    = juce::NormalisableRange<float> (20.0f, 1500.0f, 0.01f, 0.32f);
    const auto gainDb        = juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f);
    const auto gainEq        = juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f);
    const auto midHzRange    = juce::NormalisableRange<float> (150.0f, 6000.0f, 0.01f, 0.28f);
    const auto highHzRange   = juce::NormalisableRange<float> (1200.0f, 18000.0f, 0.01f, 0.28f);
    const auto qRange        = juce::NormalisableRange<float> (0.10f, 10.0f, 0.01f, 0.35f);
    const auto pctRange      = juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f);

    auto addFloat = [&] (const juce::String& id, const juce::String& name,
                         juce::NormalisableRange<float> range, float defaultValue,
                         const juce::String& label = {})
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, defaultValue,
            juce::AudioParameterFloatAttributes().withLabel (label)));
    };

    auto addBool = [&] (const juce::String& id, const juce::String& name, bool defaultValue)
    {
        params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, defaultValue));
    };

    addBool  ("FILTER_ON",   "Filter On", true);
    addFloat ("LP_CUTOFF",   "Lowpass Cutoff",  hzRange,    20000.0f, "Hz");
    addFloat ("HP_CUTOFF",   "Highpass Cutoff", lowHzRange,    20.0f, "Hz");
    addFloat ("RESONANCE",   "Resonance",       { 0.10f, 1.00f, 0.01f }, 0.10f);
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "FILTER_SLOPE", 1 }, "Filter Slope", juce::StringArray { "12 dB", "24 dB", "48 dB" }, 1));
    addFloat ("FILTER_MIX",  "Filter Mix",      pctRange, 0.0f, "%");

    addBool  ("EQ_ON",       "EQ On", true);
    addFloat ("LOW_GAIN",    "Low Gain",        gainEq, 0.0f, "dB");
    addFloat ("LOW_FREQ",    "Low Frequency",   lowHzRange,  200.0f, "Hz");
    addFloat ("LOW_Q",       "Low Q",           qRange, 0.71f);
    addFloat ("MID_GAIN",    "Mid Gain",        gainEq, 0.0f, "dB");
    addFloat ("MID_FREQ",    "Mid Frequency",   midHzRange, 1000.0f, "Hz");
    addFloat ("MID_Q",       "Mid Q",           qRange, 1.0f);
    addFloat ("HIGH_GAIN",   "High Gain",       gainEq, 0.0f, "dB");
    addFloat ("HIGH_FREQ",   "High Frequency",  highHzRange, 5200.0f, "Hz");
    addFloat ("HIGH_Q",      "High Q",          qRange, 0.71f);

    addBool  ("MOD_ON",      "Modulation On", true);
    addFloat ("LFO_RATE",    "LFO Rate",        { 0.05f, 20.0f, 0.001f, 0.25f }, 0.05f, "Hz");
    addFloat ("LFO_DEPTH",   "LFO Depth",       pctRange, 0.0f, "%");
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "LFO_SHAPE", 1 }, "LFO Shape", juce::StringArray { "Sine", "Triangle", "Saw", "Square", "Random" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "LFO_SYNC", 1 }, "LFO Sync", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "LFO_TARGET", 1 }, "LFO Target", juce::StringArray { "Cutoff", "Resonance", "Pitch", "Distortion" }, 0));
    addFloat ("ENV_ATTACK",  "Envelope Attack",  { 1.0f, 300.0f, 0.01f, 0.35f }, 20.0f, "ms");
    addFloat ("ENV_RELEASE", "Envelope Release", { 5.0f, 1000.0f, 0.01f, 0.35f }, 220.0f, "ms");
    addFloat ("ENV_SENS",    "Envelope Sensitivity", pctRange, 80.0f, "%");
    addFloat ("ENV_AMOUNT",  "Envelope Amount", pctRange, 0.0f, "%");

    addBool  ("DIST_ON",     "Distortion On", true);
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "DIST_TYPE", 1 }, "Distortion Type", juce::StringArray { "Soft Clip", "Hard Clip", "Tube", "Foldback" }, 1));
    addFloat ("DIST_DRIVE",  "Distortion Drive", pctRange, 0.0f, "%");
    addFloat ("DIST_TONE",   "Distortion Tone",  pctRange, 0.0f, "%");
    addFloat ("DIST_MIX",    "Distortion Mix",   pctRange, 0.0f, "%");

    addBool  ("PITCH_ON",    "Pitch On", true);
    addFloat ("PITCH_SEMI",  "Pitch Semitones", { -24.0f, 24.0f, 0.01f }, 0.0f, "st");
    addFloat ("PITCH_FINE",  "Pitch Fine",      { -100.0f, 100.0f, 0.01f }, 0.0f, "ct");
    addFloat ("PITCH_MIX",   "Pitch Mix",       pctRange, 0.0f, "%");

    addBool  ("STRETCH_ON",  "Stretch On", true);
    addFloat ("STRETCH",     "Stretch Amount",  pctRange, 0.0f, "%");
    addFloat ("GRAIN_SIZE",  "Grain Size",      { 5.0f, 250.0f, 0.01f, 0.35f }, 5.0f, "ms");
    addFloat ("SMOOTH",      "Smooth",          pctRange, 0.0f, "%");
    addFloat ("STRETCH_MIX", "Stretch Mix",     pctRange, 0.0f, "%");

    addFloat ("INPUT_GAIN",  "Input Gain",  gainDb, 0.0f, "dB");
    addFloat ("OUTPUT_GAIN", "Output Gain", gainDb, 0.0f, "dB");
    addFloat ("DRY_WET",     "Dry Wet", pctRange, 50.0f, "%");
    params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "BYPASS", 1 }, "Bypass", false));

    return { params.begin(), params.end() };
}

GriseldaAutoFilterAudioProcessor::AnalyzerArray GriseldaAutoFilterAudioProcessor::getAnalyzerSnapshot() const
{
    const juce::SpinLock::ScopedLockType lock (analyzerLock);
    return analyzerMagnitudes;
}

bool GriseldaAutoFilterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void GriseldaAutoFilterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    lfoPhase = 0.0f;
    randomLfoValue = 0.0f;
    lastRandomLfoTrigger = 0.0f;
    envelopeFollower = 0.0f;
    envelopeVisual.store (0.0f);
    analyzerFifoIndex = 0;
    std::fill (analyzerFifo.begin(), analyzerFifo.end(), 0.0f);
    std::fill (fftData.begin(), fftData.end(), 0.0f);

    const juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (samplesPerBlock), 1u };

    for (auto& stageBank : highPassStages)
        for (auto& filter : stageBank)
        {
            filter.reset(); filter.prepare (spec); filter.coefficients = Coeff::makeHighPass (sampleRate, 20.0, 0.7071);
        }

    for (auto& stageBank : lowPassStages)
        for (auto& filter : stageBank)
        {
            filter.reset(); filter.prepare (spec); filter.coefficients = Coeff::makeLowPass (sampleRate, 20000.0, 0.7071);
        }

    for (auto& filter : lowShelf)
    {
        filter.reset(); filter.prepare (spec); filter.coefficients = Coeff::makeLowShelf (sampleRate, 200.0, 0.7071f, 1.0f);
    }
    for (auto& filter : midPeak)
    {
        filter.reset(); filter.prepare (spec); filter.coefficients = Coeff::makePeakFilter (sampleRate, 1000.0, 1.0f, 1.0f);
    }
    for (auto& filter : highShelf)
    {
        filter.reset(); filter.prepare (spec); filter.coefficients = Coeff::makeHighShelf (sampleRate, 5200.0, 0.7071f, 1.0f);
    }

    const juce::dsp::ProcessSpec monoSpec { sampleRate, static_cast<juce::uint32> (samplesPerBlock), 1u };
    for (auto& filter : distortionToneFilters)
    {
        filter.reset(); filter.prepare (monoSpec); filter.setType (SVF::Type::lowpass); filter.setCutoffFrequency (14000.0f); filter.setResonance (0.7071f);
    }

    inputGainSmoothed.reset (sampleRate, 0.02);
    outputGainSmoothed.reset (sampleRate, 0.02);
    dryWetSmoothed.reset (sampleRate, 0.02);
    inputGainSmoothed.setCurrentAndTargetValue (1.0f);
    outputGainSmoothed.setCurrentAndTargetValue (1.0f);
    dryWetSmoothed.setCurrentAndTargetValue (1.0f);

    pitchShifter.prepare (sampleRate, juce::jmin (maxChannels, juce::jmax (1, getMainBusNumOutputChannels())));
    grainStretch.prepare (sampleRate, juce::jmin (maxChannels, juce::jmax (1, getMainBusNumOutputChannels())));

    updateEqCoefficients();
    updateToneFilters (getValue (apvts, "DIST_TONE"));
    updateFilterCoefficients (getValue (apvts, "HP_CUTOFF"), getValue (apvts, "LP_CUTOFF"), getValue (apvts, "RESONANCE"),
                              slopeToStages ((int) std::round (getValue (apvts, "FILTER_SLOPE"))));
}

void GriseldaAutoFilterAudioProcessor::releaseResources() {}

void GriseldaAutoFilterAudioProcessor::updateFilterCoefficients (float hpHz, float lpHz, float resonance, int activeStages)
{
    hpHz = juce::jlimit (20.0f, 18000.0f, hpHz);
    lpHz = juce::jlimit (30.0f, 20000.0f, lpHz);
    if (hpHz >= lpHz - 5.0f)
        hpHz = juce::jlimit (20.0f, lpHz - 5.0f, hpHz);

    const auto q = juce::jmap (juce::jlimit (0.10f, 1.00f, resonance), 0.10f, 1.00f, 0.7071f, 8.0f);
    const auto active = juce::jlimit (1, maxFilterStages, activeStages);

    for (int i = 0; i < maxFilterStages; ++i)
    {
        const auto hpCoeff = Coeff::makeHighPass (currentSampleRate, hpHz, q);
        const auto lpCoeff = Coeff::makeLowPass  (currentSampleRate, lpHz, q);
        const auto apCoeff = Coeff::makeAllPass  (currentSampleRate, 1000.0);
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            highPassStages[(size_t) i][(size_t) ch].coefficients = (i < active) ? hpCoeff : apCoeff;
            lowPassStages[(size_t) i][(size_t) ch].coefficients  = (i < active) ? lpCoeff : apCoeff;
        }
    }
}

void GriseldaAutoFilterAudioProcessor::updateEqCoefficients()
{
    const auto lowFreq  = getValue (apvts, "LOW_FREQ");
    const auto lowGain  = juce::Decibels::decibelsToGain (getValue (apvts, "LOW_GAIN"));
    const auto lowQ     = getValue (apvts, "LOW_Q");
    const auto midFreq  = getValue (apvts, "MID_FREQ");
    const auto midGain  = juce::Decibels::decibelsToGain (getValue (apvts, "MID_GAIN"));
    const auto midQ     = getValue (apvts, "MID_Q");
    const auto highFreq = getValue (apvts, "HIGH_FREQ");
    const auto highGain = juce::Decibels::decibelsToGain (getValue (apvts, "HIGH_GAIN"));
    const auto highQ    = getValue (apvts, "HIGH_Q");

    auto lowCoeff  = Coeff::makeLowShelf   (currentSampleRate, lowFreq,  juce::jmax (0.1f, lowQ),  lowGain);
    auto midCoeff  = Coeff::makePeakFilter (currentSampleRate, midFreq,  juce::jmax (0.1f, midQ),  midGain);
    auto highCoeff = Coeff::makeHighShelf  (currentSampleRate, highFreq, juce::jmax (0.1f, highQ), highGain);

    for (int ch = 0; ch < maxChannels; ++ch)
    {
        lowShelf[(size_t) ch].coefficients  = lowCoeff;
        midPeak[(size_t) ch].coefficients   = midCoeff;
        highShelf[(size_t) ch].coefficients = highCoeff;
    }
}

void GriseldaAutoFilterAudioProcessor::updateToneFilters (float tonePct)
{
    const auto cutoff = juce::jmap (juce::jlimit (0.0f, 100.0f, tonePct), 0.0f, 100.0f, 700.0f, 14000.0f);
    for (auto& filter : distortionToneFilters)
    {
        filter.setCutoffFrequency (cutoff);
        filter.setResonance (0.7071f);
    }
}

float GriseldaAutoFilterAudioProcessor::foldback (float x, float threshold)
{
    if (std::abs (x) <= threshold)
        return x;
    const auto wrapped = std::fmod (std::abs (x - threshold), threshold * 4.0f);
    return std::abs (wrapped - threshold * 2.0f) - threshold;
}

float GriseldaAutoFilterAudioProcessor::fastTanh (float x) { return std::tanh (x); }

float GriseldaAutoFilterAudioProcessor::processDistortionSample (float x, int type, float drivePct) const
{
    const auto drive = juce::jmap (juce::jlimit (0.0f, 100.0f, drivePct), 0.0f, 100.0f, 1.0f, 24.0f);
    const auto y = x * drive;

    switch (type)
    {
        case 0:  return fastTanh (y);
        case 1:  return juce::jlimit (-0.9f, 0.9f, y) / 0.9f;
        case 2:  return (2.0f / juce::MathConstants<float>::pi) * std::atan (y * 0.8f) + 0.05f * y;
        case 3:  return foldback (y, 0.6f);
        default: return x;
    }
}

double GriseldaAutoFilterAudioProcessor::getHostBpmOrDefault() const
{
    if (auto* playhead = getPlayHead())
        if (auto position = playhead->getPosition())
            if (auto bpm = position->getBpm())
                return *bpm;
    return 120.0;
}

float GriseldaAutoFilterAudioProcessor::getLfoValue (float rateHz, int shapeIndex, bool sync, double hostBpm, int numSamples) noexcept
{
    const auto effectiveHz = sync ? hzFromNoteDivision (rateHz, hostBpm) : rateHz;
    const auto delta = juce::MathConstants<float>::twoPi * effectiveHz / (float) currentSampleRate;
    float value = 0.0f;

    switch (shapeIndex)
    {
        case 0: value = std::sin (lfoPhase); break;
        case 1:
        {
            const auto phase01 = juce::jmap (std::fmod (lfoPhase, juce::MathConstants<float>::twoPi), 0.0f, juce::MathConstants<float>::twoPi, 0.0f, 1.0f);
            value = 4.0f * std::abs (phase01 - 0.5f) - 1.0f; break;
        }
        case 2:
        {
            const auto phase01 = juce::jmap (std::fmod (lfoPhase, juce::MathConstants<float>::twoPi), 0.0f, juce::MathConstants<float>::twoPi, 0.0f, 1.0f);
            value = 2.0f * phase01 - 1.0f; break;
        }
        case 3: value = std::sin (lfoPhase) >= 0.0f ? 1.0f : -1.0f; break;
        case 4:
        default:
        {
            const auto phase01 = std::fmod (lfoPhase, juce::MathConstants<float>::twoPi) / juce::MathConstants<float>::twoPi;
            if (phase01 < lastRandomLfoTrigger)
                randomLfoValue = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
            lastRandomLfoTrigger = phase01;
            value = randomLfoValue; break;
        }
    }

    lfoPhase += delta * (float) numSamples;
    while (lfoPhase >= juce::MathConstants<float>::twoPi)
        lfoPhase -= juce::MathConstants<float>::twoPi;
    return juce::jlimit (-1.0f, 1.0f, value);
}

float GriseldaAutoFilterAudioProcessor::readInterpolated (const std::vector<float>& buffer, float index) noexcept
{
    const auto size = (int) buffer.size();
    if (size == 0) return 0.0f;
    while (index < 0.0f) index += (float) size;
    while (index >= (float) size) index -= (float) size;
    const int i0 = (int) index;
    const int i1 = (i0 + 1) % size;
    const float frac = index - (float) i0;
    return buffer[(size_t) i0] + (buffer[(size_t) i1] - buffer[(size_t) i0]) * frac;
}

void GriseldaAutoFilterAudioProcessor::PitchShifter::prepare (double newSampleRate, int channels)
{
    sampleRate = newSampleRate;
    for (int ch = 0; ch < juce::jmin (channels, maxChannels); ++ch)
        ringBuffers[(size_t) ch].assign (pitchBufferSize, 0.0f);
    reset();
}

void GriseldaAutoFilterAudioProcessor::PitchShifter::reset()
{
    for (auto& buffer : ringBuffers) std::fill (buffer.begin(), buffer.end(), 0.0f);
    std::fill (writePositions.begin(), writePositions.end(), 0);
    phase = 0.0f;
}

float GriseldaAutoFilterAudioProcessor::PitchShifter::processSample (int channel, float x, float semitones, float fineCents, float mix)
{
    auto& buffer = ringBuffers[(size_t) channel];
    if (buffer.empty()) return x;
    auto& writePos = writePositions[(size_t) channel];
    buffer[(size_t) writePos] = x;

    const auto ratio = std::pow (2.0f, (semitones + fineCents / 100.0f) / 12.0f);
    const auto windowSamples = juce::jlimit (256, pitchBufferSize / 2, (int) std::round (sampleRate * 0.045));
    const auto phaseInc = (float) ratio;
    const auto phase1 = phase;
    const auto phase2 = std::fmod (phase + 0.5f * (float) windowSamples, (float) windowSamples);

    auto readHead = [&] (float localPhase)
    {
        const float progress = localPhase / (float) windowSamples;
        const float window = hannWindow (progress);
        const float readIndex = (float) writePos - (float) windowSamples + localPhase;
        return GriseldaAutoFilterAudioProcessor::readInterpolated (buffer, readIndex) * window;
    };

    const auto wet = readHead (phase1) + readHead (phase2);
    phase += phaseInc;
    while (phase >= (float) windowSamples) phase -= (float) windowSamples;
    writePos = (writePos + 1) % (int) buffer.size();
    return x + (wet - x) * juce::jlimit (0.0f, 1.0f, mix);
}

void GriseldaAutoFilterAudioProcessor::GrainStretch::prepare (double newSampleRate, int channelsToPrepare)
{
    sampleRate = newSampleRate;
    for (int ch = 0; ch < juce::jmin (channelsToPrepare, maxChannels); ++ch)
    {
        channels[(size_t) ch].ringBuffer.assign (stretchBufferSize, 0.0f);
        channels[(size_t) ch].writePosition = 0;
        channels[(size_t) ch].phasor = 0.0f;
        channels[(size_t) ch].toneState = 0.0f;
    }
    reset();
}

void GriseldaAutoFilterAudioProcessor::GrainStretch::reset()
{
    for (auto& state : channels)
    {
        std::fill (state.ringBuffer.begin(), state.ringBuffer.end(), 0.0f);
        state.writePosition = 0;
        state.phasor = 0.0f;
        state.toneState = 0.0f;
    }
}

float GriseldaAutoFilterAudioProcessor::GrainStretch::processSample (int channel, float x, float amountPct, float grainMs, float smoothPct, float mix)
{
    auto& state = channels[(size_t) channel];
    auto& buffer = state.ringBuffer;
    if (buffer.empty()) return x;

    buffer[(size_t) state.writePosition] = x;

    const auto mixNorm = juce::jlimit (0.0f, 1.0f, mix);
    const auto amountNorm = juce::jlimit (0.0f, 1.0f, amountPct / 100.0f);
    const auto smoothNorm = juce::jlimit (0.0f, 1.0f, smoothPct / 100.0f);
    if (mixNorm <= 0.0001f || amountNorm <= 0.0001f)
    {
        state.writePosition = (state.writePosition + 1) % (int) buffer.size();
        return x;
    }

    const auto grainNorm = std::pow (juce::jlimit (0.0f, 1.0f, (grainMs - 5.0f) / 245.0f), 0.72f);
    const auto shapedGrainMs = 5.0f + grainNorm * 245.0f;
    const auto grainSamples = juce::jlimit (96, stretchBufferSize / 4,
                                            (int) std::round ((shapedGrainMs / 1000.0f) * (float) sampleRate));

    const auto overlap = juce::jmap (smoothNorm, 0.0f, 1.0f, 0.35f, 0.78f);
    const auto hop = juce::jmax (24, (int) std::round (grainSamples * (1.0f - overlap)));
    const auto intraGrainSpeed = juce::jmap (amountNorm, 0.0f, 1.0f, 1.0f, 0.42f);
    const auto lagSamples = juce::jmap (amountNorm, 0.0f, 1.0f, (float) grainSamples * 1.25f, (float) grainSamples * 3.35f);
    const auto jitterDepth = (1.0f - smoothNorm) * amountNorm * (float) hop * 0.35f;

    auto renderGrain = [&] (float phaseOffset) -> float
    {
        float local = std::fmod (state.phasor + phaseOffset, (float) grainSamples);
        if (local < 0.0f) local += (float) grainSamples;
        const float local01 = local / (float) grainSamples;
        const float window = hannWindow (local01);

        const float drift = std::sin ((state.phasor + phaseOffset) * 0.0117f) * jitterDepth;
        const float readIndex = (float) state.writePosition - lagSamples + drift + local * intraGrainSpeed;
        return GriseldaAutoFilterAudioProcessor::readInterpolated (buffer, readIndex) * window;
    };

    const auto grainA = renderGrain (0.0f);
    const auto grainB = renderGrain ((float) grainSamples * 0.5f);
    float wet = grainA + grainB;

    const auto toneCoeff = juce::jmap (smoothNorm, 0.0f, 1.0f, 0.38f, 0.10f);
    state.toneState += (wet - state.toneState) * (1.0f - toneCoeff);
    wet = juce::jmap (smoothNorm, 0.0f, 1.0f, wet, state.toneState);

    const auto psychoAmount = std::pow (amountNorm, 0.82f);
    const auto wetGain = juce::jmap (psychoAmount, 0.0f, 1.0f, 0.92f, 1.18f);
    wet *= wetGain;

    state.phasor += juce::jmap (smoothNorm, 0.0f, 1.0f, 1.0f, 0.92f) * (float) hop / (float) grainSamples;
    while (state.phasor >= (float) grainSamples)
        state.phasor -= (float) grainSamples;

    state.writePosition = (state.writePosition + 1) % (int) buffer.size();
    return x + (wet - x) * mixNorm;
}

void GriseldaAutoFilterAudioProcessor::pushNextAnalyzerSample (float sample) noexcept
{
    analyzerFifo[(size_t) analyzerFifoIndex++] = sample;
    if (analyzerFifoIndex == analyzerFFTSize)
    {
        analyzerFifoIndex = 0; computeAnalyzerFrame();
    }
}

void GriseldaAutoFilterAudioProcessor::computeAnalyzerFrame() noexcept
{
    std::fill (fftData.begin(), fftData.end(), 0.0f);
    std::copy (analyzerFifo.begin(), analyzerFifo.end(), fftData.begin());
    fftWindow.multiplyWithWindowingTable (fftData.data(), analyzerFFTSize);
    forwardFFT.performFrequencyOnlyForwardTransform (fftData.data());

    AnalyzerArray temp {};
    for (size_t i = 0; i < temp.size(); ++i)
    {
        const auto magnitude = juce::Decibels::gainToDecibels (fftData[i] / (float) analyzerFFTSize, -120.0f);
        temp[i] = juce::jmap (magnitude, -100.0f, 0.0f, 0.0f, 1.0f);
    }
    const juce::SpinLock::ScopedLockType lock (analyzerLock);
    analyzerMagnitudes = temp;
}

void GriseldaAutoFilterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto totalInputChannels  = getTotalNumInputChannels();
    const auto totalOutputChannels = getTotalNumOutputChannels();
    for (int ch = totalInputChannels; ch < totalOutputChannels; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (getBool (apvts, "BYPASS"))
        return;

    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf (buffer, true);

    inputGainSmoothed.setTargetValue  (juce::Decibels::decibelsToGain (getValue (apvts, "INPUT_GAIN")));
    outputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (getValue (apvts, "OUTPUT_GAIN")));
    dryWetSmoothed.setTargetValue     (juce::jlimit (0.0f, 1.0f, getValue (apvts, "DRY_WET") / 100.0f));

    const auto filterOn  = getBool (apvts, "FILTER_ON");
    const auto eqOn      = getBool (apvts, "EQ_ON");
    const auto modOn     = getBool (apvts, "MOD_ON");
    const auto distOn    = getBool (apvts, "DIST_ON");
    const auto pitchOn   = getBool (apvts, "PITCH_ON");
    const auto stretchOn = getBool (apvts, "STRETCH_ON");

    const auto attackMs  = getValue (apvts, "ENV_ATTACK");
    const auto releaseMs = getValue (apvts, "ENV_RELEASE");
    const auto envSens   = getValue (apvts, "ENV_SENS") / 100.0f;
    const auto envAmount = getValue (apvts, "ENV_AMOUNT") / 100.0f;
    const auto lfoRate   = getValue (apvts, "LFO_RATE");
    const auto lfoDepth  = getValue (apvts, "LFO_DEPTH") / 100.0f;
    const auto lfoShape  = (int) std::round (getValue (apvts, "LFO_SHAPE"));
    const auto lfoSync   = getBool (apvts, "LFO_SYNC");
    const auto lfoTarget = (int) std::round (getValue (apvts, "LFO_TARGET"));

    auto hpCutoff = getValue (apvts, "HP_CUTOFF");
    auto lpCutoff = getValue (apvts, "LP_CUTOFF");
    auto resonance = getValue (apvts, "RESONANCE");
    const auto slopeStages = slopeToStages ((int) std::round (getValue (apvts, "FILTER_SLOPE")));

    updateEqCoefficients();
    const auto bpm = getHostBpmOrDefault();
    const auto lfoValue = modOn ? getLfoValue (lfoRate, lfoShape, lfoSync, bpm, buffer.getNumSamples()) : 0.0f;

    float envPeak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const auto* data = buffer.getReadPointer (ch);
        for (int n = 0; n < buffer.getNumSamples(); ++n)
            envPeak = juce::jmax (envPeak, std::abs (data[n]));
    }

    const auto attackCoeff  = std::exp (-1.0f / (0.001f * attackMs * (float) currentSampleRate));
    const auto releaseCoeff = std::exp (-1.0f / (0.001f * releaseMs * (float) currentSampleRate));
    if (envPeak > envelopeFollower)
        envelopeFollower = attackCoeff * envelopeFollower + (1.0f - attackCoeff) * envPeak;
    else
        envelopeFollower = releaseCoeff * envelopeFollower + (1.0f - releaseCoeff) * envPeak;

    const auto envMod = modOn ? juce::jlimit (0.0f, 1.0f, envelopeFollower * (0.5f + envSens * 2.0f)) : 0.0f;
    envelopeVisual.store (envMod);

    float pitchSemi = getValue (apvts, "PITCH_SEMI");
    float pitchFine = getValue (apvts, "PITCH_FINE");
    float distDrive = getValue (apvts, "DIST_DRIVE");

    if (modOn && lfoTarget == 0)
        lpCutoff *= std::pow (2.0f, lfoValue * lfoDepth * 1.75f + envMod * envAmount * 2.0f);
    else if (modOn)
        lpCutoff *= std::pow (2.0f, envMod * envAmount * 2.0f);

    if (modOn && lfoTarget == 1)
        resonance = juce::jlimit (0.10f, 1.00f, resonance + lfoValue * lfoDepth * 0.45f);
    if (modOn && lfoTarget == 2)
        pitchSemi += lfoValue * lfoDepth * 12.0f;
    if (modOn && lfoTarget == 3)
        distDrive = juce::jlimit (0.0f, 100.0f, distDrive + lfoValue * lfoDepth * 45.0f);

    lpCutoff = juce::jlimit (30.0f, 20000.0f, lpCutoff);
    updateFilterCoefficients (hpCutoff, lpCutoff, resonance, slopeStages);
    updateToneFilters (getValue (apvts, "DIST_TONE"));

    const auto distType = (int) std::round (getValue (apvts, "DIST_TYPE"));
    const auto distMix  = getValue (apvts, "DIST_MIX") / 100.0f;
    const auto pitchMix = getValue (apvts, "PITCH_MIX") / 100.0f;
    const auto stretchAmount = getValue (apvts, "STRETCH");
    const auto grainSize = getValue (apvts, "GRAIN_SIZE");
    const auto smooth = getValue (apvts, "SMOOTH");
    const auto stretchMix = getValue (apvts, "STRETCH_MIX") / 100.0f;
    const auto filterMix = getValue (apvts, "FILTER_MIX") / 100.0f;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            auto sample = wet[n] * inputGainSmoothed.getNextValue();
            if (ch == 0) pushNextAnalyzerSample (sample);

            if (filterOn)
            {
                auto filtered = sample;
                for (auto& stageBank : highPassStages)
                    filtered = stageBank[(size_t) ch].processSample (filtered);
                for (auto& stageBank : lowPassStages)
                    filtered = stageBank[(size_t) ch].processSample (filtered);
                sample = sample + (filtered - sample) * filterMix;
            }

            if (eqOn)
            {
                sample = lowShelf[(size_t) ch].processSample  (sample);
                sample = midPeak[(size_t) ch].processSample   (sample);
                sample = highShelf[(size_t) ch].processSample (sample);
            }

            if (distOn && distMix > 0.0f)
            {
                const auto driven = processDistortionSample (sample, distType, distDrive);
                sample = sample + (driven - sample) * distMix;
                sample = distortionToneFilters[(size_t) ch].processSample (0, sample);
            }

            if (pitchOn)
                sample = pitchShifter.processSample (ch, sample, pitchSemi, pitchFine, pitchMix);

            if (stretchOn)
                sample = grainStretch.processSample (ch, sample, stretchAmount, grainSize, smooth, stretchMix);

            wet[n] = sample * outputGainSmoothed.getNextValue();
        }
    }

    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        const auto dryWet = dryWetSmoothed.getNextValue();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto dry = dryBuffer.getSample (ch, n);
            buffer.setSample (ch, n, dry + (buffer.getSample (ch, n) - dry) * dryWet);
        }
    }
}

void GriseldaAutoFilterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        copyXmlToBinary (*xml, destData);
    }
}

void GriseldaAutoFilterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* GriseldaAutoFilterAudioProcessor::createEditor()
{
    return new GriseldaAutoFilterAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GriseldaAutoFilterAudioProcessor();
}
